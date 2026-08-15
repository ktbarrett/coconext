# C++20 Coroutine Scheduler Design Notes

This document describes the current coconext C++ scheduler: its object model,
ownership rules, lifecycle, and extension points. The Python bridge is outside
its scope.

## Object model

| Public type | State/lifetime | Purpose |
| --- | --- | --- |
| `Coro<T>` | Move-only owner of a coroutine frame | Lazy, single-use coroutine composition |
| `Task<T>` | Intrusively refcounted `TaskState<T>` in a coroutine frame | Started, shared, scheduled work |
| `AbstractFuture<StateT>` | Intrusively refcounted `AbstractFutureState<T>` | Extensible externally completed awaitable |
| `Future<T>` | `AbstractFuture<FutureState<T>>` | Ad-hoc inter-Task communication |
| `TaskManager` | Normal, non-copyable polymorphic object | Owns and applies policy to a set of Tasks |
| `EventLoop` / `Event` | Plain scheduler objects | Ready-queue substrate |

`TaskManager` deliberately differs from `Task` and Future handles. It is not
shared state, it has no result value, and it is not itself awaitable. Its
lifecycle is explicit:

```cpp
TaskManager manager;
co_await manager.start();

Task<int> first = manager.start_soon(read_value());
Task<void> second = manager.start_soon(write_value());

co_await manager.join();
```

`start()` binds the manager to the enclosing Task's event loop and global
manager. `start_soon()` is legal only after that binding. `join()` waits for
the manager's policy to finish it; it does not itself close the manager.

## Scheduling substrate

### Intrusive deques

`IntrusiveDeque<T>` stores links in each node and owns no entries. This gives
the scheduler:

- O(1) anonymous removal when an awaiter is canceled or destroyed;
- no allocation when a coroutine suspends or resumes;
- O(1) splicing of a Future's complete waiter list onto the ready queue; and
- one representation for an awaiter while parked and while ready.

An intrusive node can belong to only one deque at a time. The scheduler state
machines enforce that invariant.

### Event loop

An `EventLoop` owns a ready deque of `Event`s. External drivers acquire a
`Handle`, schedule work, and let the handle drain the queue before releasing
the loop's recursive mutex. Internal completion paths may acquire the same
loop recursively to append work, but `is_running_` prevents recursively
draining it.

The concrete events are:

- `TaskState<>::Scheduled`, which starts a queued Task; and
- `AwaitableAwaiter<S>`, which resumes a Task parked on a Future or Task.

`TaskManager::join()` is an ordinary `Coro<void>` built on a private
`Future<void>`. It therefore uses the same multi-consumer awaitable machinery
as other Future users rather than defining a manager-specific event.

## Coro and Task

`Coro<T>` is lazy and move-only. Awaiting one uses symmetric transfer between
coroutine frames. It is a composition primitive, not independently scheduled
work.

A `Task<T>` is what a manager creates when it accepts a `Coro<T>`:

```cpp
Task<T> TaskManager::start_soon(Coro<T> coro);
```

The Coro-to-Task constructor is private. A raw Task-returning coroutine can
still produce an unstarted Task because `Task` is a coroutine return type, but
awaiting it does not start it implicitly and is an error. Normal scheduled
work therefore goes through a manager, making the owner explicit before its
body can run.

Task handles share an intrusive reference count stored in `TaskState<T>`.
While a Task is active there can be three additional kinds of ownership:

1. the manager holds a reference while the Task is in `tasks_`;
2. the scheduler holds a reference from initial scheduling through completion;
3. the currently executing event takes a temporary resume guard.

The scheduler reference means dropping every public `Task` handle cannot
destroy an active coroutine. The resume guard means completion may release
both the manager and scheduler references without destroying the coroutine
frame until `.resume()` has returned and the frame is safely suspended at
`final_suspend`.

Manager ownership also installs a dedicated, allocation-free internal done
callback on the `TaskState`. The callback notifies the owning manager and is
cleared when the Task is unlinked or detached; user callbacks remain separate.

### Task state and cancellation

A Task moves through these execution states:

```text
unstarted -> Scheduled -> Running -> Pending -> Running -> ... -> done
```

Cancellation is a pending-request flag. `cancelling()` reports whether cancellation
is pending; `cancelled()` is true only after the Task has terminated by propagating
`CancelledError`. The
terminal state uses a dedicated `Cancelled` tag rather than retaining an
`exception_ptr`; `result()` throws a fresh `CancelledError`, and `exception()`
constructs one on demand.

Delivery depends on the execution state:

- done: no-op;
- unstarted: complete immediately with `CancelledError`;
- Scheduled: remove the ready event and complete without entering the body;
- Pending: move the parked awaiter to the ready queue so `await_resume()`
  throws `CancelledError`;
- Running: throw `CancelledError` synchronously into the running body.

The Scheduled case is important for fail-fast managers. A sibling queued
behind a failing Task must not begin running after the manager cancels it.

Catching `CancelledError` does not clear the request. Returning normally,
propagating a different exception, or attempting another scheduler suspension
while cancellation remains pending turns the Task outcome into `std::runtime_error`.
Only internal structured-concurrency cleanup may temporarily clear cancellation. A
`CancelledError` thrown without an outstanding request remains an ordinary stored
exception and does not make `cancelled()` true.

## Futures

`AbstractFutureState<T>` contains the result or exception, waiter deque, done
callbacks, event-loop binding, and intrusive reference count.
`AbstractFuture<StateT>` is its shared handle. Together they have
`shared_ptr`-like behavior without a separate control-block allocation.

The state destructor is virtual. A trigger-backed state normally primes its
external trigger in its constructor and unprimes in its destructor if the
Future was dropped before completion:

```cpp
class TimerState : public AbstractFutureState<void> {
  public:
    TimerState(...) { trigger_.prime([this] { set_void(); }); }

    ~TimerState() override {
        if (!done()) {
            trigger_.unprime();
        }
    }
};
```

Domain handles subclass `AbstractFuture<MyState>` and forward any additional
state API. `Future<T>` is the concrete convenience form:

- `FutureState<T>` publicly exposes `set_result`, `set_void`, and
  `set_exception`;
- `Future<T>` forwards those operations from its handle.

This form is intended for quick internal or user inter-Task communication.

A Future binds to the event loop of its first suspending Task. Later waiters
must use the same loop. Completion invokes callbacks synchronously and then
splices all waiters onto that loop's queue.

## Shared await choreography

Tasks and Futures use `detail::AwaitableAwaiter<S>`. When an await actually
suspends, it performs these operations in order:

```text
parent promise -> enclosing TaskContext
awaitable.on_awaited(context)      validate/bind loop before mutating the Task
context.get_task().on_awaiting()   Task becomes Pending
awaitable.register_waiter(...)     awaiter enters the waiter deque
```

Validation happens before the Task records a pointer to the awaiter. If loop
validation throws, the running Task is not left pointing at a destroyed
awaiter.

When completion schedules the awaiter, `event_run()` takes a temporary Task
reference, marks it Running, resumes the saved parent handle, restores the
previous `current_task`, and releases the reference. `await_resume()` checks
for outstanding cancellation requests before returning or rethrowing the
awaitable's result.

`CoroState` and `TaskState` promises both expose the enclosing `TaskContext`,
so the Task and its scheduler bindings follow the promise chain rather than
depending on thread-local state. That remains correct through arbitrarily
nested Coros.

## TaskManager lifecycle

`TaskManager` has four internal states:

```text
Created --co_await start()--> Open --close()--> Closed
                                      |             |
                                      |       children empty
                                      |             v
                                      +----------> Done
```

- `Created`: unbound; `start_soon`, `close`, `cancel`, and `join` are invalid.
- `Open`: accepts new Coros and starts them immediately.
- `Closed`: rejects new Coros and drains existing Tasks.
- `Done`: closed and empty; every join waiter is made ready.

An empty Open manager does not become Done automatically. It has no child
whose policy hook could decide that work is complete, so the owner must call
`close()` explicitly.

Only the Task that called `start()` may call `join()`. `join()` does not call
`close()`: while its caller is suspended, existing children may use the manager
to add more siblings. Completion policy belongs in `on_child_done()`.

If a joining Task is canceled before the manager is Done, `join()` cancels the
manager but continues waiting for every child to finish its cancellation
cleanup. It temporarily clears cancellation from the joining Task so it can
suspend again. A later cancellation request repeats that process. Once the
manager is Done, `join()` reasserts cancellation and throws `CancelledError`.
A `CancelledError` stored as
the manager's own completion exception is distinguished by the absence of a
pending cancellation request on the joining Task.

`cancel()` first closes the manager and then cancels every remaining child.
Thus cancellation always rejects new work and drains to Done. This pairing is
the safe default: a canceled scope that remained Open could accept work after
its shutdown pass and never allow its joiner to finish.

### Ownership and destruction

`TaskManager` is non-copyable and non-movable. Its child `TaskState`s store raw
back-pointers to its stable address.

Its virtual destructor is `noexcept(false)`. Destruction is valid in two states:

- `Created`, because it never started; or
- `Done`, because every child has drained.

Destroying a started manager in Open or Closed detaches and cancels its children,
then throws `std::logic_error`.
This is a misuse diagnostic, not a mechanism for extending the manager's
lifetime. As with every throwing destructor, destruction during unrelated
exception unwinding would call `std::terminate`; owners should structure
their coroutine so `join()` is reached on all normal and handled-error paths.

### Policy hooks

Three protected virtual hooks allow specialized structured-concurrency
semantics:

- `on_add(task)`: called after a Task is linked and started;
- `on_child_done(task)`: called after the completed Task is unlinked, while
  its result remains readable;
- `on_done()`: called exactly once on the Closed-and-empty transition.

The default `on_child_done()` closes the manager when its last child exits.
Overrides can inspect `task->exception()`, retain their first exception with
`set_exception()`, call `close()` or `cancel()`, and delegate to the base
implementation when appropriate.

Typical policies are:

| Policy | `on_child_done` behavior |
| --- | --- |
| gather, fail-fast | On failure store exception and `cancel()` siblings; otherwise close when empty |
| select | Store the first result and `cancel()` the remaining Tasks |
| wait-all | Record outcomes but continue through failures; close only when empty |
| test manager | First failure stores the exception and cancels siblings; successful children may freely spawn siblings |

A stored manager exception is rethrown by `join()` only after all children
have drained. There is no manager result value.

## Global manager and free `start_soon`

Every running Task stores two manager pointers:

- `task_manager_`: the manager that owns that Task directly;
- `global_task_manager_`: the ambient manager used by free spawning.

User-created managers own their children through `task_manager_`, but pass on
the parent's global pointer. Therefore code anywhere inside a managed child
can call:

```cpp
Task<T> task = coconext::start_soon(coro());
```

The free function looks up the enclosing Task's global manager and delegates
to that manager's `start_soon(Coro)`. It does not accept a pre-existing Task.
Every spawned body is therefore wrapped, owned, bound, and started in one
operation.

## `run()`

`run(Coro<T>)` is the standalone driver. It creates an `EventLoop` and a
private `RunTaskManager<T>`, starts the root Task with that manager as both its
direct and global owner, and drains the loop.

The run-manager policy is fail-fast:

- the first failing root or globally spawned child stores its exception,
  closes the manager, and cancels all siblings;
- successful root completion closes the manager and cancels any surviving
  fire-and-forget siblings;
- completion is reported only after every owned Task has drained.

If an external trigger owns pending work, `run()` waits on a condition
variable until the manager reaches Done. It then rethrows the first failure
through the root Task's result, or returns that result.

Under a simulator, external callbacks drive the same `EventLoop` directly;
there is no standalone `run()` loop.

## Loop affinity and TaskContext

Each active Task, TaskManager, and Future belongs to exactly one EventLoop.
Their mutable state is intentionally not protected by per-object locks.
Cross-loop use would race the Task state machine, manager child deque, waiter
lists, results, and cancellation state, so mismatches are rejected rather
than synchronized.

Bindings happen as late as possible:

- `TaskManager::start()` inherits the enclosing `TaskContext`;
- `TaskManager::start_soon()` passes that context to each new Task;
- a Future binds on its first suspending await;
- the private run manager binds directly to its newly created loop.

`current_task` is thread-local only for explicitly ambient operations such as
the free `start_soon` and `current_context`. Each resume event saves and
restores the prior value so no completed Task is left as the ambient context.

A `TaskContext` stores its event loop, optional global manager, and optional
current Task explicitly. Nested Coros and TaskManagers propagate that context
through their promise chain. Root contexts have no current Task, and contexts
without a global manager reject calls to the free `start_soon` function.

## Extension constraints

- Manager hooks are `noexcept`; policies must retain errors as
  `std::exception_ptr` rather than throw out of a completion callback.
- Awaitable callbacks run synchronously. They must not invalidate scheduler
  objects whose completion path is still executing.
- A TaskManager's address must remain stable from `start()` through `join()`.
- A Task returned by `start_soon()` may outlive its public handles while
  running, but its manager must not.
- Awaiting a completed Task is allowed, including from a later EventLoop,
  because `await_ready()` returns without registering a cross-loop waiter.
