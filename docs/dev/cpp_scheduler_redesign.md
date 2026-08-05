# C++20 Coroutine Scheduler Design Notes

Design notes for the coconext C++20 coroutine scheduler. Describes the object
model, state machines, binding rules, and the substrate (`EventLoop`,
`IntrusiveDeque`) they sit on. The Python-facing surface is out of scope here.

## Object model at a glance

Five user-visible types plus their per-type "state" holders:

| Return object     | State holder            | Role                                          |
| ----------------- | ----------------------- | --------------------------------------------- |
| `Coro<T>`         | `CoroState<T>`          | Lightweight chained coroutine. Move-only.     |
| `Task<T>`         | `TaskState<T>`          | Scheduled, refcounted, shared awaitable.      |
| `Future<StateT>`  | `FutureState<T>`        | Shared, single-shot, externally-resolved.     |
| `TaskManager<S>`  | `TaskManagerState<T>`   | Structured-concurrency group of Tasks.        |
| (none)            | `EventLoop` / `Event`   | The scheduling substrate.                     |

Three of these — `Task`, `Future`, `TaskManager` — are refcounted handles to a
heap-allocated `*State` object. `Coro` is a move-only owner of a coroutine
frame with no shared state. `EventLoop` is a plain object driven from outside.

All three shared-state types (`FutureState`, `TaskState<>`, `TaskManagerState<>`)
follow the same shape: a result slot, an awaiter list, a done-callback list,
a lazily-bound `EventLoop*`, and a refcount. That shape is what
`AwaitableAwaiter` (below) is written against as a concept.

### Type-erased bases

Each `*State<T>` derives from a `*State<detail::Erased>` base. The base holds
everything that doesn't depend on `T` (state machine, waiter list, cancellation
flag, refcount, event-loop binding). The typed derived class layers only the
result slot and `return_value` / `return_void` on top.

This matters because the scheduler queues, cancels, and links these objects
without knowing `T`. `current_task` is a `TaskState<>*` (erased); the awaiter
concept binds to `TaskState<>&`; `TaskManagerState<>::tasks_` is an intrusive
deque of erased tasks. Only user-facing `result()` / `set_result()` calls need
the typed layer.

## Substrate: `EventLoop`, `Event`, `IntrusiveDeque`

### `IntrusiveDeque`

`IntrusiveDeque<T>` is a doubly-linked list where the links live *inside* the
node (via `IntrusiveDequeNode`). It owns nothing but its two sentinels.

Why intrusive rather than `std::deque<T*>`:

- **O(1) anonymous remove.** Any node can unlink itself in O(1) without
  knowing which deque it's in, without a lookup, and without touching the
  container. This is what makes cancellation cheap: a suspending awaiter
  destroys itself (frame destruction runs its dtor), and the dtor calls
  `deque_remove()` — no scan of the trigger's waiter list, no map from
  handle-to-position.
- **No allocations on suspend/resume.** The awaiter lives in the coroutine
  frame (which is already allocated); pushing it onto a waiter list is a
  four-pointer store, popping is another four.
- **Splice in O(1).** `extend_back` moves an entire foreign list onto the tail
  in constant time. `FutureState::on_done` uses this to move all waiters onto
  the event loop's ready queue in one call rather than N `schedule_back`
  invocations.
- **Same node, many roles.** `Event` is both a waiter (lives in a Future's
  `waiters_` deque) and a ready item (lives in the EventLoop's `queue_`
  deque). Being intrusive means "moving a waiter to the ready queue" is a
  relink of the same node — no allocation, no wrapper object.

The one cost: nodes can only be in one deque at a time. That constraint
matches the state machines exactly (see below).

### `Event`

Base class for anything the EventLoop can run. Single virtual `event_run()`
plus `event_unschedule()` (which is just `deque_remove()`). Two concrete
subclasses today:

- `AwaitableAwaiter<S>` — lives in the awaiting coroutine's frame; `event_run`
  resumes the parent handle.
- `TaskState<>::Scheduled` — lives in a Task's promise; `event_run` resumes
  the Task's own coroutine handle.

### `EventLoop` and the recursive mutex

`EventLoop` holds `IntrusiveDeque<Event> queue_`, a `std::recursive_mutex`,
and an `is_running_` flag. External callers can't touch the queue directly;
they must go through `acquire()`, which returns a `Handle` holding the lock.

The **critical ownership pattern**: an external event source (a simulator
callback, a Python entry, `run()`) acquires the handle, schedules whatever
events it wants, then drops the handle. On drop, if the loop isn't already
running, the handle drives the queue to exhaustion before releasing the lock.
So from the outside: "one external event => one batch of scheduled work runs
to completion." From the inside, events can freely schedule more events; those
just extend the current drain.

Why `std::recursive_mutex` instead of a plain mutex:

- **Single `on_done()` in `FutureState`.** A Future can be resolved either by
  an external event (a DPI callback, a user calling `set_result`) or by a
  coroutine that is *itself* running under the loop lock. Both must schedule
  the Future's waiters. With a recursive mutex, `FutureState::on_done` can
  unconditionally `event_loop_->acquire()` — it works whether we already hold
  the lock or not. With a non-recursive mutex, every resolver would need to
  know its calling context and pass it in.
- **Sometimes-recursive callers exist.** A DPI import that resolves a Future
  can be called directly from the RTL (external, no lock held) or reached
  via a DPI export invoked from within a Python callback (recursive, lock
  held). We don't want the resolver to have to distinguish.

Recursive-mutex overhead is roughly a `thread_id` compare + atomic bump on
recursive acquires — cheap enough not to matter next to the coroutine work.

The `is_running_` flag catches a different bug: `Handle::run()` called from
inside an already-running loop, which would corrupt the drain. The recursive
mutex allows *re-acquisition* (fine, because the Handle dtor won't re-drain
if `is_running_` is set), but forbids nested `run()`.

## Shared awaitable state and the unified awaiter

The three shared-state types (`FutureState<T>`, `TaskState<>`,
`TaskManagerState<>`) all expose the same small set of members that the
awaiter machinery relies on:

- `value_type` — the typed result the awaiter returns.
- `done()` — result is available (drives `await_ready` short-circuit).
- `get_event_loop()` — the bound loop, or `nullptr` if still unbound.
- `register_waiter(event)` — append an awaiter's `Event` to `waiters_`.
- `on_awaited(task)` — late-bind bookkeeping. For `FutureState` this
  propagates the awaiter's loop. For an unstarted `TaskState` it also
  schedules the body. For a `TaskManagerState` it also starts every
  queued child.
- `result()` — typed result; throws if not done.

`TaskState<>` and `TaskManagerState<>` also implement `get_task()` / the
promise-side plumbing needed to be the *awaiting* coroutine — but the
awaiter side only leans on the members above.

One awaiter — `detail::AwaitableAwaiter<S>` — is templated on the state
type and serves `co_await future`, `co_await task`, and `co_await
task_manager` uniformly. It is itself an `Event` (it lives on the
awaitable's waiter deque, and later gets relinked onto the loop's ready
deque). Its dtor calls `event_unschedule()`, which is what makes
cancellation-of-losers and frame destruction correctly de-register from
whatever list the awaiter was on. There is no `concept` constraint on
`S` today — the requirements above are enforced by ordinary compile
errors at the template instantiation.

## Threading and loop affinity

Every `Task`, `TaskManager`, and `Future` belongs to exactly one `EventLoop`,
and that binding is enforced. Late binding decides *which* loop; from that
point on, cross-loop use is a caught error.

The invariant differs slightly across the three types:

- **`Future`**: one loop for fan-out. Multiple waiters allowed, all must
  share a loop. This works because a Future has no body — nothing writes
  `result_` except an explicit `set_result` from outside, and everything
  past `on_done` runs under the (single) bound loop's lock.
- **`Task` and `TaskManager`**: body loop = awaiter loop = every sibling's
  loop, all the same. Not just "reasonable" — required, because their
  public state is unprotected by design.

### Why Task/TaskManager can't be cross-loop

Their per-object state has no lock, on the assumption that one loop owns
each object:

- `done()` reads `result_`, written by the body from `return_value` /
  `unhandled_exception`.
- `cancelled()` reads `cancelled_`, written by external `cancel()` calls
  and read by every `await_resume()` in the body.
- `state_` (the Scheduled/Pending/Running variant) is written by the body
  on every suspend/resume and read by `cancel()` from awaiter code.
- `TaskManagerState::tasks_`, `closed_`, `cancelled_` — touched by
  `internal_child_done` on the body-loop and by `add` / `close` / `cancel`
  from awaiter code.

If body-on-B and awaiter-on-A, every one of these becomes a race. The
`EventLoop` mutex only protects `queue_`; it does not protect per-object
state. Locking per-object would be strictly worse than the current design
for the single-loop case that is 100% of real use.

### Where the binding happens

Late binding decides loop affinity at the first operation that needs it.
For Task/TaskManager, that means every entry point that touches or connects
the object must reconcile loops:

- `Task::start_soon()` binds `event_loop_` from `current_event_loop()`
  (the caller's loop) if not already bound. That defines the body's loop.
- `co_await task` / `co_await manager`: `on_awaited(awaiting_task)` binds
  the awaitable's loop from the awaiter's. Match → fine. Mismatch → throws.
  First-awaiter-wins if not yet bound. For a TaskManager, `on_awaited`
  also drives every queued (unstarted) child through `start_soon(loop, gtm)`.
- `TaskManager::add(task)`: rejects a task already bound to another
  manager. Does *not* start the task. If the manager is already
  running (`started()`), the added task is started immediately on the
  manager's bound loop and global manager. Otherwise the child is left
  queued in `tasks_` and started later when the manager itself starts.
  If the incoming task is already running, `add` opportunistically
  propagates its `event_loop_` / `global_task_manager_` onto the (still
  unbound) manager, which throws on mismatch.
- `TaskManager::start_soon()` binds `event_loop_` and `global_task_manager_`
  from the caller's context if not already bound, then starts every
  queued child.

The `add`-time reconciliation matters: without it, a manager could accept
tasks from two different loops, and later work — `cancel()` iterating
`tasks_`, `on_child_done` running on whichever loop happened to fire —
would silently race across loops. The manager itself would be one race
target (`tasks_`, `closed_`, `cancelled_`); each child's per-object state
would be another.

### Cross-loop would require a redesign, not more locking

The single-loop model is load-bearing. Making Task/TaskManager cross-loop
safe would mean either:

- Locks on `state_`, `result_`, `cancelled_`, `tasks_`, etc. — every
  read from an awaiter and every write from the body. Cost paid by the
  common case for a use case that doesn't exist.
- An explicit hand-off protocol (submit-to-loop-X, receive-result-on-loop-Y),
  which is a different abstraction, not this one.

Neither is planned. Current design: one loop per Task, per Manager,
per Future.

## Late binding: what and why

Nothing in the scheduler stores a "current EventLoop" TLS. Nothing that
doesn't strictly need it looks up a `current_task`. Bindings happen **at the
first operation that actually needs them**, not at construction, not on
schedule. Concretely:

- A `Future`'s `event_loop_` is nullptr until the first Task awaits it. A
  Future that is set but never awaited never needs to know about a loop.
- A `TaskState`'s `event_loop_` and `global_task_manager_` are nullptr until
  the Task is `start_soon()`'d **or** first awaited by another running Task
  **or** the enclosing `TaskManager` is itself started.
- A `TaskManagerState`'s `event_loop_` and `global_task_manager_` bind when
  the manager is first awaited, when its own `start_soon()` is called, or
  when an already-running task is `add`'d (which propagates its bindings).
- `current_task` (a `thread_local TaskState<>*`) is set on Task resume and
  read only when we genuinely have no cheaper handle — namely on the
  `start_soon(coro)` free function called from user code, and on the
  `current_*()` accessors. `Coro`/`Task` awaiting a Future do **not** consult
  TLS; they get the awaiting Task from the awaiting coroutine's promise.

**Why bind late.** Two reasons:

1. **TLS lookups aren't free.** `inline thread_local` in local-exec mode is a
   direct offset from `%fs`, but every one is still a load and a branch on
   nullness. When a Coro nested three deep awaits a Future, we already know
   the enclosing Task via the promise chain; going to TLS to find it is
   wasteful and — worse — wrong if the "current" Task doesn't match the
   actual awaiter (see below).
2. **Correctness under nesting and cross-loop use.** TLS assumes a single
   scheduling context per thread. Late binding through the promise chain
   makes cross-loop misuse a caught error (`bind_event_loop` throws on
   conflict) rather than a silent state corruption.

**How the promise-chain lookup works.** `AwaitableAwaiter::await_suspend`
takes `coroutine_handle<PromiseType> h`. It calls `h.promise().get_task()` —
`CoroStateBase` and `TaskState<T>` both implement this. For a `Task<T>`
promise it returns `*this`. For a `CoroState<T>` promise it returns a stored
`TaskState<>*` that was threaded in when the Coro was first awaited (see
`Coro::Awaiter::await_suspend`, which writes `p.task_ = h.promise().get_task()`
before symmetric-transferring to the child). So even deep in a chain of
Coros, the awaiting promise always knows the enclosing Task.

## `co_await` — the choreography

When a running `Task` (directly or through nested `Coro`s) executes
`co_await future`, four objects are wired up in `await_suspend`:

```cpp
void AwaitableAwaiter<S>::await_suspend(coroutine_handle<P> h) noexcept {
    parent_ = h;                          // remember who to resume
    auto& task = h.promise().get_task();  // enclosing Task (via promise chain, not TLS)
    task.on_awaiting(*this);              // Task: "I'm parked on this Event"
    task_ = &task;                        // remember for resume + cancellation check
    awaitable_.on_awaited(task);          // Future: "propagate my loop; also start if unstarted"
    awaitable_.register_waiter(*this);    // Future: append me to waiters_
}
```

The awaiter holds three references: the parent coroutine handle (to resume),
the enclosing Task (for cancellation and resume-side bookkeeping), and the
awaitable (for `await_resume()` to fetch the result). The awaitable holds
one back-reference — the awaiter as an `Event` on its `waiters_` deque.

`on_awaiting` on the Task stores an `Event*` in the Task's `state_` variant
(`Pending{&awaiter}`). That's how `Task::cancel()` on a suspended task can
find the awaiter to un-park it — see the cancellation section.

`on_awaited` on the awaitable does two things:

1. **Late-bind the event loop.** If the awaitable's `event_loop_` is nullptr,
   set it from the awaiting Task's loop. If it's already bound to a
   different loop, throw — that's cross-loop misuse.
2. **Kick, if applicable.** For `TaskState`, if not `started()`, schedule the
   `Scheduled` event on the loop. This is why awaiting an unstarted Task
   works even if the user never explicitly `start_soon()`'d it.

When the Future is resolved (`set_result` / `set_exception` / `set_void`),
`on_done()` acquires the loop and calls `schedule_all_back(std::move(waiters_))`.
Every parked awaiter is now on the ready queue. When the loop runs each one,
`AwaitableAwaiter::event_run()` marks the enclosing Task as resumed
(`task_->on_resume()` — sets `Running{}`, updates `current_task` TLS) and
calls `parent_.resume()`. The resumed coroutine enters `await_resume()`,
which checks `task_->cancelled()` and either throws `Cancelled` or returns
`awaitable_.result()`.

**Awaiter destruction.** The awaiter is a local of the currently-suspended
`co_await` expression, so it lives in the coroutine frame. If the frame is
destroyed (Coro dropped, Task cancelled and unwound), the awaiter dtor runs
`event_unschedule()`, which removes it from whichever deque it's in
(`waiters_` if still parked; the loop's `queue_` if already scheduled but
not yet run). No dangling waiters.

## `Coro<T>`: lightweight chaining

`Coro<T>` is intentionally minimal:

- `initial_suspend()` = `suspend_always` (lazy — a Coro doesn't start
  running just because you called the function).
- `final_suspend()` returns a `TransferAwaitable` whose `await_suspend`
  **returns the parent handle** — symmetric transfer back to whichever
  Coro or Task awaited this one. See "Symmetric transfer" below.
- No refcount, no waiter list, no cancellation flag. Move-only. Dtor
  destroys the frame if still alive.
- `CoroState<T>` carries only a `variant<monostate, Value<T>, Exception>`
  result, a `coroutine_handle<>` parent, and a `TaskState<>* task_`.

`Coro::Awaiter::await_suspend` sets the child promise's `task_` and `parent_`
and returns the child handle. This is the second symmetric-transfer hop.

The `task_` pointer propagated through the Coro chain is what lets a Coro
five levels deep still answer `get_task()` in O(1) without TLS. It's set
once per `co_await`, so the cost is one pointer store per hop.

`Coro` is the natural building block for helpers that don't need
scheduling — a shift-register model, an intermediary that awaits a Trigger
and returns processed data, etc. When one of these needs to run
independently, wrap it in a `Task`.

## `Task<T>`: scheduled shared handle

`TaskState<T>` is a coroutine promise **and** a scheduler item. Its typed
public API mirrors `Coro`: `initial_suspend()` = `suspend_always`,
`final_suspend()` = `suspend_always` (frame kept alive so waiters can read
the result; destroyed when refcount hits zero), `return_value`/`return_void`,
`unhandled_exception` capturing to `std::exception_ptr`.

The erased base `TaskState<>` holds the state machine and shared plumbing.

### State machine

```
      +------------+   start_soon()      +-----------+
      | monostate  | ------------------> | Scheduled |
      | (unstart.) |    (or awaited)     +-----+-----+
      +------------+                           |
             ^                                 |  loop runs Scheduled event
             |                                 v
             |                            +---------+
             |     awaiter fires,         | Running |
             |     schedules Task         +----+----+
             |                                 |
             |                                 | co_await X
             |                                 v
             |                            +---------+
             +---(on_done: monostate)---+ | Pending |
                                          +---------+
```

- `monostate` — unstarted. `start_soon()` is legal here; also, being awaited
  transitions us to `Scheduled` automatically.
- `Scheduled` — the `Scheduled` event (an `Event` living inside the state)
  is on the loop's queue. Waiting for the loop to run it.
- `Running` — coroutine body is executing. `current_task` TLS points here.
- `Pending` — coroutine has hit a `co_await` and is parked. Holds the awaiter
  `Event*` so `cancel()` can find it.
- Back to `monostate` on completion (final_suspend runs `on_done`).

### Late binding on Tasks

A `TaskState` has `event_loop_`, `task_manager_` (the group it belongs to,
if any), and `global_task_manager_` (the ambient root manager — see below).
All three are nullptr at construction. They fill in when we cross the first
threshold that requires them:

- `start_soon()` (no-arg) binds `event_loop_` from `current_event_loop()`
  and `global_task_manager_` from `current_global_task_manager()` if
  nullptr, then schedules the `Scheduled` event.
- `start_soon(loop, gtm)` (the manager-driven overload) binds
  `event_loop_` from the passed loop (throwing on mismatch) and
  `global_task_manager_` from the passed manager if nullptr, then
  schedules the `Scheduled` event. Used by `TaskManager::add` and
  `TaskManager::start_soon` / `TaskManager::on_awaited` to start children
  under the manager's already-decided context.
- Being awaited before ever being started: `on_awaited(task)` binds
  `event_loop_` and `global_task_manager_` from the awaiting Task (both of
  which must already be bound because the awaiter is running), then
  schedules `Scheduled`. If the Task is already started, `on_awaited` is
  a no-op — the existing bindings stand.
- `TaskManager::add(task)` sets `task_manager_` (rejecting a task already
  bound to a different manager). It does not itself bind or start the
  task. If the manager is already running it forwards to
  `task->start_soon(loop, gtm)`; otherwise the child sits in `tasks_`
  until the manager starts.

Rebinding to a different loop or a different `TaskManager` throws. Rebinding
`global_task_manager` is silently a no-op (first binding wins) because tasks
propagated through several intermediate managers can legitimately be
re-added.

### `start_soon` vs `TaskManager::add`

Two operations, sharply different:

- **`start_soon(task)`** (free function or member): promotes the task from
  `unstarted` to `Scheduled`, and — if not already bound — inherits
  `event_loop_` and `global_task_manager_` from the caller's context.
  Additionally, the free-function form does
  `current_global_task_manager().add(task.get_state())` so the task is
  tracked. It's the "just run this thing under my current context" API.
- **`TaskManager::add(task)`** takes a Task and binds it to *this* manager
  for lifecycle tracking. It refuses if the manager is `done()` or
  `closed()`, or if the task is already bound to a different manager.
  It does **not** start unstarted tasks by itself — if the manager is
  unstarted, the child sits queued in `tasks_` until the manager itself
  starts (via its own `start_soon()` or via being awaited). If the
  manager is already running when `add` is called, the child is started
  immediately on the manager's bound loop and global manager.

The two compose: `start_soon` schedules under whatever manager is ambient;
`add` associates with a specific manager whose lifecycle you want to bound.
Queued-then-started semantics let a manager be assembled fully before any
of its children run — useful for setting up siblings that reference each
other.

### Cancellation

`Task::cancel()` is a counter (`uint16_t cancelled_`) not a bool, so nested
cancel/uncancel scopes compose. Behavior depends on state:

- `done()` — no-op.
- `unstarted` (monostate) — set exception to `Cancelled{}` directly. The
  Task never runs.
- `Pending{event}` — unschedule the parked awaiter
  (`pending.event->event_unschedule()`, O(1) via intrusive remove), transition
  to `Scheduled`, and push onto the loop. When the loop runs it, the
  coroutine resumes into `await_resume()`, sees `cancelled()`, throws
  `Cancelled`.
- `Scheduled` or `Running` — nothing to do; the flag will be seen at the
  next `await_resume()`.

`uncancel()` decrements the counter and throws if it was already zero. Only
awaiters check the flag, so cancellation is delivered exclusively at suspend
points — same behavior as Python asyncio, different mechanism.

## `Future<StateT>`: shared externally-resolved

The plainest of the three shared types. `FutureState<T>` has a result slot,
a waiter deque, done callbacks, an event-loop slot, and a refcount. Users
subclass `FutureState<T>` to add domain state (e.g. a Timer holds an
`NTime`, a ValueChange holds a signal handle) and override the virtual
`unprime()` hook — called from the state's dtor if the refcount reaches
zero while still pending, so the subclass can un-register from whatever
external event source it hooked into. `Future<StateT>` is the refcounted
handle.

Late binding on `FutureState`: `event_loop_` is nullptr until first awaited.
`on_awaited` propagates from the awaiting Task. If never awaited, never
bound — a Future that's created, resolved, and dropped without a consumer
does no loop work at all.

`add_done_callback` runs synchronously from `on_done`, before waiters are
scheduled. Prefer awaits over callbacks — callbacks are the escape hatch
for non-coroutine consumers (Python bridge, C++ FFI).

## `TaskManager<StateT>`: structured concurrency

`TaskManagerState<>` owns an `IntrusiveDeque<TaskState<>> tasks_`. When a
Task is `add()`'d, the manager bumps its refcount and links it into `tasks_`.
When each child completes, `TaskState::on_done` calls back into
`internal_child_done`, which unlinks the task, drops the refcount, and
invokes user hooks. Three virtual hooks let subclasses shape policy:

- `on_add(task)` — right after linking. Used to attach per-task callbacks
  or bookkeeping.
- `on_child_done(task)` — after unlinking; the task's result is readable
  here. Used to make policy decisions (cancel siblings on failure, forward
  exception, etc.).
- `on_drain_complete()` — fired exactly once after `tasks_` empties **and**
  the manager is `closed()`. Subclasses set the manager's own result here.

Closure and cancellation:

- `close()` — no more `add()`s will succeed. Idempotent. If `tasks_` is
  already empty on close, `on_drain_complete` fires immediately.
- `cancel()` — sets `cancelled_`, cancels every task in `tasks_`. New
  children can still be added (which is a legitimate use case for
  best-effort shutdown), but they'll be cancelled on `close()` if the
  subclass wires it up.
- No auto-close on drain. `internal_child_done` fires
  `on_drain_complete` only when the manager has already been explicitly
  `close()`'d and `tasks_` empties. Subclasses that want the
  "start N children, wait for them all" pattern call `close()` themselves
  (e.g. from an `on_add` hook once the initial batch is queued, or after
  awaiting the manager once).

`TaskManager` is itself awaitable — it exposes the same shared-state
surface as `Future` and `Task`. Waiters fire when `on_drain_complete`
sets the result (via subclass calls to `set_result` / `set_void` /
`set_exception`).

### Global task manager and `start_soon`

Every running Task carries a `global_task_manager_` pointer, distinct from
its group `task_manager_`. This is the ambient scope — the root manager
that owns all top-level fire-and-forget work in the current run.

- `start_soon(coro)` and `start_soon(task)` (free functions) both call
  `current_global_task_manager().add(...)`. This means fire-and-forget tasks
  are still tracked; they don't leak, and their exceptions can be surfaced.
- New tasks inherit `global_task_manager_` from the parent Task on
  `start_soon`, so the ambient scope propagates all the way down.
- User-created `TaskManager`s do *not* replace the global one — they're
  independent groups. A task added to a user `TaskManager` still points at
  the same `global_task_manager_` it inherited.

`run()` creates a `RunTaskManagerState` and installs it as both the root
Task's group manager and its global manager. When the root Task finishes,
`on_child_done` closes the manager and cancels any surviving siblings
(fire-and-forget tasks that outlived the root). `on_drain_complete` then
propagates the root's exception (if any) as the manager's result.

## `run()` — the outer harness

`run(Task<T>)` (in `run.hpp`) is the entry point when there is no external
driver (no simulator). It builds an `EventLoop`, wraps the task in a
`RunTaskManager`, `add()`s it, then:

1. `handle = loop.acquire()`
2. `handle.run()` — drains the queue.
3. If the root task isn't done (some external event source may still be
   working — condition variables, threads), block on
   `condition_variable_any::wait` against the recursive mutex, waking on
   the task's done callback.
4. Return `task.result()` (which rethrows if the task failed).

Under a simulator, there is no `run()` — the simulator's callbacks
`acquire()` the loop, schedule things, and let the Handle dtor drain.

## Interaction summary

```
User code:                                 Runtime relationships:

Task<T> t = start_soon(my_coro());        t.state_ -> TaskState<T>
                                              |
                                              +-- event_loop_ (bound from caller)
                                              +-- global_task_manager_ (bound; auto-added)
                                              +-- task_manager_ (nullptr; not in a group)
                                              +-- state_ = Scheduled{&this}
                                              +-- Scheduled Event pushed onto loop.queue_

// loop runs, event_run() flips state_ to Running{}, resumes handle.
// coroutine body runs to `co_await some_future`.

AwaitableAwaiter awaiter(some_future_state);
  parent_ = task_handle
  task = task_handle.promise().get_task()   // Task via promise chain, no TLS
  task.on_awaiting(awaiter)                 // state_ = Pending{&awaiter}
  task_ = &task
  some_future_state.on_awaited(task)        // late-binds loop; no-op for Future
  some_future_state.waiters_.push_back(awaiter)

// external event resolves the Future:
some_future.set_result(v);
  -> FutureState::on_done()
       loop.acquire().schedule_all_back(std::move(waiters_))   // O(1) splice

// loop runs the awaiter's event_run():
awaiter.event_run():
  task_->on_resume()          // state_ = Running{}, current_task = task_
  parent_.resume()            // -> await_resume() -> Cancelled check -> result()
```

## Design decisions worth noting

- **Cancellation exception is `coconext::Cancelled`** (a concrete type,
  derived from `std::exception`), not a wrapped `exception_ptr`. Waiters
  can catch it precisely.
- **No `First` / `Combine` / `with_timeout` combinators in C++.** The
  intrusive waiter list would make them cheap to add if needed, but there's
  no current C++ user; combinators live in the Python layer for now.
- **Awaiters return `T` or `void` from `await_resume`.** No `self`-returning
  awaiters, no per-Trigger value semantics. The single templated awaiter
  is possible because of this uniformity.
- **`std::exception_ptr` for result storage.** Copies are atomic bumps,
  rethrow is non-consuming and thread-safe. Suits many-waiter fan-out
  without exception-object copies.
- **`add_done_callback` runs synchronously before scheduling waiters.**
  Callbacks are for non-coroutine bridges; ordering with respect to waiters
  is defined but not something to rely on for logic.

## One awaitable, one loop

Awaitables belong to exactly one `EventLoop`, decided lazily by the first
awaiter. `bind_event_loop` throws if a second awaiter arrives from a
different loop. Cross-loop `co_await` is not supported and is caught at
`await_suspend`.

Why it can't be relaxed:

- **Waiter list splice targets one loop.** `on_done` moves the entire
  waiter deque onto `event_loop_->queue_`. Mixed-loop waiters would land
  half on the wrong queue, resumed by the wrong thread's drain.
- **Recursive mutex protects one loop.** A resumed coroutine that does
  `co_await other_future` acquires a different mutex than the resuming
  drain thought it was holding — no ordering, no exclusion between loops.
- **`current_task` TLS.** Set by whichever loop's `event_run` fired.
  TLS-dependent code inside the coroutine would see wrong values on the
  wrong thread if the awaitable and its task straddled loops.

Late binding is what makes this checkable: an unbound awaitable has no
loop, so the first awaiter defines the home loop; subsequent awaiters
either match or throw. A Future created, resolved, and dropped without a
consumer never binds at all.

## Interface conventions: pointers, references, and `not_null`

Three tiers, applied uniformly across scheduler internals:

1. **`not_null<T*>`** — non-null identity handle across an internal API.
   The default for scheduler-object parameters and return types where
   non-null is a real invariant. Callers pay one check on construction;
   downstream `not_null → not_null` hops skip redundant checks entirely.
2. **`T*`** — nullable. Only where nullability is actual state
   (`event_loop_` before binding, `task_manager_` when not in a group,
   `global_task_manager_` before install, `Pending{event}` slots), not
   where a reference would have worked.
3. **`T&` / `T const&` / `T&&`** — value semantics. Reading a result,
   consuming an rvalue, passing a callback. Never for scheduler-object
   identity.

**Why not references for non-null identity.** Storage has to be pointers
(late binding needs nullability, shared-state handles need
rebindability), so reference parameters force `&x` at every call site,
and pointer→reference→pointer round-trips lose stable identity — a debug
table wants to key on the actual object pointer, not `&some_ref` inside
a callee. `not_null<T*>` gives the non-null signal without the type
laundering.

**Why not GSL.** `coconext::not_null<T*>` lives in
`cpp/include/coconext/not_null.hpp`. It's ~40 lines, `constexpr`,
`noexcept` on the fast path, asserts on construction from null,
disallows nullptr and default construction, implicitly converts to
`T*` for API tightening incrementally, and hashes/compares as the
raw pointer so debug tables treat it the same.

**Where the check fires.** Exactly once per code path: at the boundary
where a raw `T*` becomes a `not_null<T*>`. Design internal APIs so that
most parameters *arrive* as `not_null` and the check has already
happened upstream. Notably, `current_task()` returns `not_null<TaskState<>*>`
because it just threw on nullptr — every caller inherits the proof.

**Where fields stay raw.** `TaskState<>::event_loop_`,
`task_manager_`, `global_task_manager_`, `Pending::event` all stay
`T*` — their nullability *is* meaningful state. Assignment from a
`not_null<T*>` uses the implicit conversion; no re-check.

## Symmetric transfer: scope and limits

Symmetric transfer (returning a `coroutine_handle<>` from `await_suspend`,
which the compiler emits as a tail call) is only available when the resumer
is *itself a coroutine*. That constraint decides where we can use it and
where we cannot.

**Where it applies.** Both hops are inside the Coro world:

- `Coro`→`Coro` chaining. `Coro::Awaiter::await_suspend` (a coroutine
  awaiter) returns the child handle. Tail-called from the parent's suspend.
- `Coro`→parent completion. `CoroStateBase::final_suspend`'s awaitable
  returns the parent handle (either another `Coro` or the enclosing
  `Task`). Tail-called from the child's final suspend.

So a chain `Task → Coro → Coro → Coro` unwinds and rewinds via tail calls;
depth of the chain doesn't touch the C stack.

**Where it does not apply.** Any hop initiated from `EventLoop::Handle::run_`.
The loop's drain is not a coroutine — it's a plain `while (!queue_.empty())
event->event_run()` — so there is no `await_suspend` frame for the tail call
to replace. `event_run()` must call `.resume()` on the target handle. That
covers both scheduler entry points:

- `TaskState<>::Scheduled::event_run` (loop resuming a Task after schedule).
- `AwaitableAwaiter<S>::event_run` (loop resuming an awaiting Task after
  its awaitable fired).

This is fine, not a bug. The loop's drain is O(1) on the C stack regardless
of queue depth: `event_run` calls `.resume()`, the resumed coroutine runs
until it suspends again (which returns control back to `event_run`), and
the loop pops the next event. Even a tight
`while (true) co_await ready_trigger;` in a Task doesn't grow the stack
because `AwaitableAwaiter::await_ready` short-circuits on
`awaitable_.done()` — no scheduler round-trip, no `event_run` recursion —
and if the trigger isn't ready, the coroutine actually suspends and the
next resume comes from a fresh top-level loop iteration.

The only way stack could grow is if resuming a coroutine synchronously
re-entered the drain (nested `run()`). That's exactly what `is_running_`
prevents: an inner `set_result` calls `loop.acquire()`, its Handle's dtor
sees `is_running_` and does *not* re-drain — it just appends to `queue_`,
and the outer drain picks the events up.

**Summary.** Symmetric transfer is a Coro-internal optimization. Task
scheduling is loop-driven and uses `.resume()`. The stack stays O(1) by
construction.

## Python compatibility layer

The C++ scheduler exposes nothing Python-specific; the bridge is built on top
using the same primitives everything else uses (Future, Task, Coro). The rule
that makes it tractable: **awaitability crosses the language boundary only
via `Future`**. Native coroutines don't cross — neither Python's
`generator/coroutine` protocol nor C++20 coroutine handles bridge directly,
so we don't try. Everything on the far side gets wrapped until it is
Future-shaped.

Nanobind offers no built-in support for this — no coroutine or awaitable
adapter in either the headers or the 2.13.0 package. Same for pybind11.
The bridge is hand-written on top of the standard bindings.

### `PyCoro`

Nanobind C++ wrapper whose body is a `Coro<>` driving a Python coroutine
object with `send()`:

```
loop:
  yielded = py_coro.send(last_result)   # or .throw(last_exception) on error
  # yielded is a nanobinded C++ Future (the only awaitable that crosses)
  try:
      last_result = co_await yielded    # native C++ await; parks on the Future
  except e:
      last_exception = e; continue
  # StopIteration on send() -> co_return with its value
```

Each Python-level `await` in the wrapped coroutine yields a nanobinded
Future object; Python's coroutine machinery propagates it up to the
enclosing `send()` call in the C++ wrapper. The wrapper `co_await`s it via
the normal `AwaitableAwaiter` path (so this Coro parks in the C++
scheduler on the same waiter list any native awaiter would use), and
threads the result — or exception — back into the next `send()` /
`throw()`. `StopIteration` ends the loop and becomes the Coro's return
value.

Only `PyCoro` exists; there is no `PyTask`. A top-level Python coroutine
becomes a `PyCoro` and — per the "Coro → Python needs a Task" rule below —
is wrapped in a `Task<>` when it needs to be scheduled or exposed back to
Python. Cancellation composes: `Task::cancel()` on the wrapping Task
causes the next `co_await yielded` inside `PyCoro` to throw `Cancelled`
on `await_resume`; the wrapper delivers it into the Python coroutine via
`py_coro.throw(Cancelled(...))`.

### Wrapping native C++ types for Python

Python-side, the wrappers implement the awaitable protocol by hand —
`__await__` returns a generator that `yield`s the wrapped object itself
(a Future) so the enclosing `PyCoro`'s `send()`-loop receives it.

- **`Future<StateT>` → Python.** Direct nanobind wrapper implementing
  `__await__`. This is the load-bearing case — every awaitable that
  crosses into Python is either already a Future or is wrapped in one
  under the hood.
- **`Task<T>` → Python.** Direct nanobind wrapper. Because `Task` is
  already awaitable-state-shaped and refcounted, the wrapper's
  `__await__` yields it as the awaitable and the `PyCoro` loop drives
  it just like a Future.
- **`Coro<T>` → Python.** No direct path. A Python-side awaiter has no
  way to enter a C++ coroutine handle, so we don't try — the Coro gets
  wrapped in a `Task<T>` first, and the Task is what Python sees. One
  extra scheduler round-trip; keeps the bridge trivial.

### Consequences

- The C++ scheduler's public API needs no Python-shaped hooks. Everything
  Python does — awaiting a native Trigger, wrapping a Python coroutine,
  cancelling from either side — goes through Future and Task, using their
  existing APIs.
- Python-side combinators (`First`, `Combine`, `with_timeout`) can be
  implemented in pure Python against the wrapped Futures/Tasks without any
  new C++ surface. The intrusive waiter list underneath makes their
  cancel-losers pattern O(1) anyway.
- No polymorphic waiter on the hot path. The scheduler's waiter deque still
  contains only `Event*` — the Python bridge is layered on top rather than
  cut into.
