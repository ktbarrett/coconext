# C++20 Coroutine Scheduler Design Notes

Draft notes for porting cocotb's scheduler/Trigger system to C++20 coroutines,
while keeping the Python frontend backwards-compatible.

## Core mental model

- A C++20 coroutine has three associated objects:
  - **Frame** - heap-allocated activation record (parameters, locals, suspend
    point, promise). Owned by whoever holds a RAII handle.
  - **`promise_type`** - lives *inside* the frame. Per-coroutine state machine
    scratchpad (result slot, waiter list, cancellation flag, refcount, etc.).
    Not user-facing.
  - **Return object** (e.g. `Task<T>`, `Coro<T>`) - what the coroutine function
    returns. RAII handle to the frame, also the awaitable users hold and
    manipulate.
- `std::coroutine_handle<P>` is a non-owning, pointer-sized, trivially-copyable
  token. `.resume()`, `.destroy()`, `.done()`, `.promise()`. Many handles can
  alias one frame; conventionally one RAII owner exists, others are aliases.
- `std::promise` from `<future>` is the wrong abstraction here. It's
  thread-sync, single-producer/single-consumer, not awaitable without adapters.
  Build awaitables on plain handle lists held by Triggers.

## Trigger / awaiter shape

Triggers own a list of waiting `coroutine_handle<>`s. The awaiter is the RAII
slot in that list.

- `await_suspend(h)` receives the handle of the coroutine that is suspending.
  Stash it in the trigger's waiter list, return.
- `await_resume()` checks the promise's cancellation flag and rethrows if set;
  otherwise returns `self` or `void` (standardize on one of these two only).
- Awaiter destructor unregisters from the trigger. Since the awaiter lives in
  the frame (it's a local of the `co_await` expression that's currently
  suspended), destroying the frame runs the destructor, which automatically
  unprimes the trigger if the waiter list goes empty.
- Multi-consumer Triggers are trivial because we own the list. No
  `std::promise`/`shared_future` machinery needed.

## Two return types

### `Coro<T>` - lightweight chaining helper

- Single awaiter, no scheduler awareness, no shared ownership.
- `promise_type` holds: result/exception slot, single continuation
  `coroutine_handle<>`.
- `initial_suspend()` returns `std::suspend_always` (lazy - matches Python's
  "calling an async fn doesn't run it").
- `final_suspend()` returns an awaiter that symmetric-transfers to the
  continuation.
- Cancellation cascade is automatic via RAII: destroy parent frame -> awaiter
  destructor -> child Coro destructor -> child frame destroyed -> child's
  trigger-awaiter destructor unregisters.

### `Task<T>` - scheduled, shared-ownership top-level

- Refcounted public handle. Shared state lives in the promise (no second
  allocation).
- Promise holds: result/exception slot, waiter list (multiple consumers can
  `co_await` the task), cancellation flag, refcount.
- `final_suspend()` suspends instead of destroying, so consumers can still read
  the result after completion. Frame destroyed when refcount hits zero.
- `start_soon` accepts both `Coro<T>` and `Task<T>` - the `Coro` overload wraps
  in a `Task<T>` returning coroutine.

## Result storage

Three-state slot in the promise (pending / value / exception):

```cpp
std::variant<std::monostate, T, std::exception_ptr> result_;
// or for void:
std::exception_ptr exception_;
bool done_ = false;
```

- `std::exception_ptr` is refcounted, copying is an atomic bump, rethrow is
  non-consuming, thread-safe across consumers. No copies of the exception
  object regardless of how many waiters resume.

## Symmetric transfer

`await_suspend` can return `std::coroutine_handle<>` instead of `void`/`bool`.
The compiler emits a tail call to that handle, popping the current `resume()`
frame before the next one starts. Stack depth stays O(1) regardless of await
chain depth.

Required in the hot paths:

1. **`Coro` -> `Coro` chaining.** `Coro::operator co_await` returns the child
   handle from `await_suspend`.
2. **`Coro` completion.** `final_suspend` returns the continuation handle (or
   `std::noop_coroutine()` if none).
3. **Scheduler resume loop.** No special handling needed; a single `.resume()`
   call now walks the chain via tail calls.

Trigger awaiters that just park in a list and return `void` are fine - nothing
to tail-call to.

Gotchas:

- `await_suspend` returning a handle must be `noexcept`.
- Return `std::noop_coroutine()`, never a default-constructed (null) handle.
- Zero state, zero bookkeeping, zero runtime cost. Change the return type and
  `return handle;` instead of `handle.resume();`.

Cost of skipping: stack-depth problem (not correctness). Tight
`for(;;) co_await trig;` loops with immediately-ready triggers blow the stack
within a few thousand iterations. Hard to retrofit because it changes every
awaiter's `await_suspend` signature.

## Cancellation

C++ has no equivalent of Python's `coro.throw(exc)`. Pattern:

1. `Task::cancel()` sets `promise.cancelled_ = true`.
2. Scheduler resumes the coroutine normally (or it's already running).
3. The next `await_resume()` checks the flag and throws a cancellation
   exception.
4. Exception propagates up via normal C++ unwinding; awaiter destructors
   unregister from triggers; eventually caught by the top-level `Task`'s
   promise via `unhandled_exception()` and stored.

Cancellation only takes effect at suspend points - same as Python, different
mechanism. Every awaiter in the system must have this check in
`await_resume()`. Design must be fixed early.

## Things we are NOT supporting in C++

- **`First`/`Combine`/"wait for any" combinators.** Skipped in C++. (If
  reintroduced later, intrusive waiter list nodes are needed for O(1)
  cancellation of losers.)
- **Fire-and-forget exception swallowing.** Cocotb-Python warns at GC time. In
  C++: a fire-and-forget `start_soon`'d Task that throws *always fails the
  test*. Other Tasks always capture exceptions of awaited Tasks.
- **Per-Trigger custom value semantics.** All triggers return `self` or
  `void` from `await_resume()`. No template-per-trigger just because one
  produces a value.

## Things we DO need

- **Per-task context** (logger, name, test identity, traceback). Lives in
  `promise_type`. Child `Coro<T>` lookup of enclosing Task: thread-local
  "current task" updated on every resume is simplest and matches asyncio.

## Python backwards compatibility via narrow waist

Strategy: C++ owns scheduler primitives and Triggers. Python wraps them and
re-implements the high-level behaviors we dropped from C++.

### Minimum C++ surface exposed to Python

1. **`Trigger`** - opaque handle.
2. **A way to park a Python task on a Trigger.** Per-Python-task shim
   coroutine in C++:
   - Each Python Task is backed by a C++ `Coro<void>` that loops:
     `co_await trigger_handed_from_python; call_back_into_python_to_resume_pytask();`.
   - Python tells C++ "park me on T", C++ shim does the parking via real C++
     machinery, on fire calls back into Python.
   - Waiter list stays homogeneous (`coroutine_handle<>` only). No polymorphic
     waiter, no variant dispatch on the hot path.
3. **`start_soon`** - schedules the shim coroutine.
4. **Cancellation hook** - Python `Task.cancel()` reaches the C++ cancellation
   path on the shim, which propagates the exception back into Python.

Unit of scheduling in C++ = one Python Task (1:1 with a shim coroutine). Do
not try to make C++ schedule individual coroutine steps; that duplicates
scheduler logic.

### Pure-Python layer on top

- `First` / `Combine` / `with_timeout`: Python constructs N independent shim
  registrations and manages cancellation of losers itself. C++ waiter-list
  code never sees combinator structure.
- Fire-and-forget exception logging / "never retrieved" warnings: Python Task
  wrapper holds the bookkeeping in Python state, fires from `__del__` /
  weakref finalizer.
- Per-task logger / name / traceback: Python Task object. C++ promise carries
  only a back-pointer (`PyObject*` or `void*` slot).

### Sketch

```cpp
class PyTask {
    Coro<void> shim_;
    PyObject* py_task_;
public:
    void park_on(Trigger& t);   // shim does co_await t
    void cancel();              // sets flag, re-resume injects exception
};
void start_soon(PyTask&);
```

Python's Task wraps a `PyTask` and delegates suspend/resume/cancel; keeps all
high-level bookkeeping in pure Python where it's easy to change. User-defined
Triggers in either language work the same way - a Python Trigger calls the
same C++ "fire" entrypoint as a native one, walking the same waiter list.

## Open questions / decisions to revisit

- Exact spelling and ergonomics of `start_soon` accepting both `Coro<T>` and
  `Task<T>` (overload set, or single function with concept-constrained
  template?).
- Cancellation exception type: dedicated `CancelledError` class, or use
  `std::exception_ptr` wrapping whatever Python threw?
- How the thread-local "current task" interacts with simulator callbacks
  re-entering the scheduler (probably fine since we're single-threaded, but
  document the invariant).
- Whether to expose `prime`/`unprime` directly to Python or hide them behind
  the park/fire entrypoints.
