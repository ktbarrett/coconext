# `SimObject` API Specification

This document specifies the API for the C++ simulator-object type in coconext. It is a design spec; the implementation has not been written.

Since we are in C++ and can't dynamically decide the type of an object, we can't do the exact equivalent of what cocotb does. So instead we implement a single type which supports all operations of all simulator-object kinds and uses runtime checks to ensure the operation is valid.

The type is called `SimObject`. `handle` was the working name (cocotb uses it) but it says little about what the object represents; `SimObject` reads better at call sites.

`SimObject` is the C++ analogue of cocotb's `cocotb.handle` module and the `dut` parameter to `cocotb.test`. A future static-structure representation of the DUT will be reachable via `SimObject::downcast<T>()`; that static surface is out of scope here.

## Overview

`SimObject` wraps an opaque GPI object. It exposes:

- traversal by name and index; exception-free lookup via `try_child`
- named-child iteration for scopes via `children()`; indexed iteration for arrays via `begin()`/`end()`
- metadata (kind, path, defining module, signedness, constness, size)
- typed value read via `get<T>()` and typed value write via `set(v, action)`
- freeze/release
- edge and value-change awaitables (scalars: rising/falling/value_change; vectors: value_change only)
- equality and hashing on sim-object identity

All access routes through GPI. Reads on logic-storage objects always go through `gpi_get_signal_value_binstr` so that X/Z survive; the `get<T>` specialization is responsible for interpretation (rejecting X/Z where the target type requires it) and width checking.

## Ownership and lifetime

`SimObject` is a value type over a shared implementation:

```cpp
class SimObject {
    std::shared_ptr<detail::SimObjectImpl> impl_;
};
```

Copying a `SimObject` bumps a refcount. Users treat it as a value; the shared indirection is not visible in the public API. The `SimObjectImpl` can be swapped for an intrusively refcounted type later without changing any call site.

The ownership graph between objects is:

- `SimObjectImpl` holds a **strong** `shared_ptr` to its parent `SimObjectImpl`.
- `SimObjectImpl` holds a map of **weak** `weak_ptr`s to its named/indexed children.

Consequences:

- Holding a deeply nested child (`top["mod"]["sub"]["signal"]`) keeps every ancestor alive as long as any user reference exists.
- A child that no user is holding is evicted from its parent's map on the next lookup miss / cleanup pass, and re-materialized on the next request.
- Repeated `parent["x"]` returns the same `SimObjectImpl` while any user holds it, so identity (`==`) and cached metadata are stable across lookups.

`top()` returns a `SimObject` that owns the root `SimObjectImpl`; the root has no parent ref.

## Kinds

```cpp
enum class SimObjectKind {
    Module, GenerateBlock, GenerateArray,
    Logic, LogicVector, LogicArray,
    Integer, Real, String,
    Unknown,
};
```

`Unknown` is retained because some simulators surface objects whose kind we cannot classify; propagating an `Unknown` `SimObject` is preferred to throwing at construction.

The packed/unpacked distinction (e.g. packed `LogicVector` vs unpacked `LogicArray`-of-`Logic`) lives in `SimObjectKind` alone. It does not appear in the value-type API: the routing (single `binstr` call for packed, per-element calls for unpacked) is decided inside `get`/`set` based on `kind()`.

## Set actions

```cpp
enum class SetAction { Deposit, Force, Immediate };
```

Maps to `gpi_set_action` as: `Deposit` -> the scheduled deposit, `Force` -> force, `Immediate` -> `GPI_NO_DELAY`. `Freeze` and `Release` are value-less operations and are exposed as separate methods rather than actions. `_OldImmediate` is intentionally not surfaced.

## Class

```cpp
namespace coconext {

class SimObject {
public:
    SimObject() = delete;
    SimObject(SimObject const&) = default;
    SimObject(SimObject&&) = default;

    // ---- traversal ----------------------------------------------------
    SimObject operator[](std::string_view name) const;  // throws if missing / wrong kind
    SimObject operator[](std::size_t index)      const;  // throws if missing / wrong kind

    std::optional<SimObject> try_child(std::string_view name) const;
    std::optional<SimObject> try_child(std::size_t index)     const;

    // ---- iteration ----------------------------------------------------
    // Indexed iteration for array-like kinds (LogicVector, LogicArray,
    // GenerateArray, ...). Throws on non-array kinds.
    auto begin() const;
    auto end()   const;

    // Named-scope iteration (Module, GenerateBlock).
    // Throws on non-scope kinds. Yields pair<std::string, SimObject>.
    auto children() const;

    // ---- metadata -----------------------------------------------------
    std::string_view name() const;
    std::string      path() const;
    SimObjectKind    kind() const;
    std::string_view def_name() const;
    std::string_view def_file() const;

    // Bit width for integer-kind objects; vector/array length otherwise.
    std::size_t size() const;

    // Range of the object's *children*, for array-like kinds.
    Range range() const;

    bool is_const()  const;
    bool is_signed() const;

    // Same underlying sim object; hash keyed on path().
    friend bool operator==(SimObject const&, SimObject const&) noexcept;
    friend struct std::hash<SimObject>;

    // ---- reads --------------------------------------------------------
    template <typename T> T get() const;

    // ---- writes -------------------------------------------------------
    void set(Bit   v,             SetAction = SetAction::Deposit);
    void set(Logic v,             SetAction = SetAction::Deposit);
    void set(bool  v,             SetAction = SetAction::Deposit);

    template <BitViewable T>
    void set(T const& v,          SetAction = SetAction::Deposit);

    template <typename T, std::size_t N>
    void set(Array<T, N> const& v, SetAction = SetAction::Deposit);
    template <typename T>
    void set(Vector<T> const& v,   SetAction = SetAction::Deposit);

    void set(std::int64_t v,      SetAction = SetAction::Deposit);
    void set(double v,            SetAction = SetAction::Deposit);
    void set(std::string_view v,  SetAction = SetAction::Deposit);  // dispatched by kind

    void freeze();
    void release();

    // ---- edges --------------------------------------------------------
    RisingEdge  rising_edge()  const;  // scalar-Logic kinds
    FallingEdge falling_edge() const;  // scalar-Logic kinds
    ValueChange value_change() const;  // scalars and vectors

    // ---- future static downcast ---------------------------------------
    template <StaticSimObject T>
    std::optional<T> downcast() const;

private:
    std::shared_ptr<detail::SimObjectImpl> impl_;
};

SimObject top();

}  // namespace coconext
```

Free trigger constructors (`rising_edge(SimObject const&)`, `falling_edge(SimObject const&)`, `value_change(SimObject const&)`) live in `triggers.hpp` and forward to the corresponding member.

## Traversal

Children are reached by indexing:

```cpp
SimObject module = coconext::top()["module"];
// ints index into arrays or generate arrays
module["gen_array"][0]["array_signal"][1];
```

`operator[](string_view)` and `operator[](size_t)` throw on missing children or on kind mismatch (e.g. name lookup on a `LogicVector`). `try_child` is the exception-free fetch, returning `std::nullopt` on absence or kind mismatch. There is no separate `contains` predicate; `try_child(name).has_value()` covers the case at the cost of materializing the child on hit, which is cheap since the child is cached in the parent's weak-child map either way.

## Iteration

There are two iteration surfaces, one per shape:

**Named-scope iteration** (`Module`, `GenerateBlock`) via `children()`:

```cpp
for (auto const& [name, child] : module.children()) {
    // ...
}
```

Yields owned `pair<std::string, SimObject>`. Names are owned to avoid depending on simulator-owned string lifetimes.

**Indexed iteration** (`LogicVector`, `LogicArray`, `GenerateArray`, ...) via `begin()`/`end()`:

```cpp
for (auto child : signal) {
    child.set(Bit{0});
}
```

Yields `SimObject`. The iteration order matches `range()`.

`begin()`/`end()` throw on non-array kinds; `children()` throws on non-scope kinds.

## Value reads

```cpp
template <typename T> T SimObject::get() const;
```

Since we don't know the type of the object statically, `get` cannot return the "correct" type on its own; the user asks for a particular type:

```cpp
signal.get<LogicVector>();
signal.get<Unsigned<7, 0>>();  // width must match; construction succeeds
signal.get<float>();            // nonsense for this kind; throws
```

All logic-storage reads route through `gpi_get_signal_value_binstr`. The specialization for `T` interprets the binstr and enforces width and value constraints:

- `get<Bit>()` / `get<Logic>()`: single-bit binstr; `Bit` rejects X/Z.
- `get<BitViewable T>()`: width of `T` must equal object width; 2-state types (`Unsigned<W>`, `Signed<W>`, `BitArray<W>`, `Usfixed`, `Sfixed`, `Float`) reject X/Z; `LogicArray`/`Vector<Logic>` do not.
- `get<Array<T, N>>()`: `N` must equal object length. Packed: one `binstr` call split into `N` elements. Unpacked: per-child GPI calls.
- `get<Vector<T>>()`: dynamic width; range filled from the object. Packed/unpacked routing as above.
- `get<std::int64_t>()`: rejects X/Z; throws if the value does not fit `int64_t` given `is_signed()`.
- `get<double>()`: `gpi_get_signal_value_real`. Applicable to `SimObjectKind::Real` only.
- `get<std::string>()`: dispatched by `kind()`. On `SimObjectKind::String`, calls `gpi_get_signal_value_str`. On packed logic-vector kinds, calls `gpi_get_signal_value_binstr` and returns the raw binstr. On other kinds, throws.

There are a lot of possible type overloads, and some may be valid for a given underlying kind (e.g. simulator integers can reasonably read as `int64_t`, `Unsigned<W>`, `LogicVector`, etc.). The set of `get<T>()` specializations is a perpetual work in progress; the list above is the starting point, not a closed set.

Width mismatches are always errors; there is no implicit resize on read. If a user has a 16-bit object and wants an `Unsigned<8>`, they call `get<Unsigned<16>>()` and `resize<8>` explicitly.

## Value writes

Writes are an overload set on the source type; each overload picks the right GPI setter. The set-action is a defaulted second argument:

```cpp
signal.set(1);
signal.set("10101010"_l);
signal.set(12, SetAction::Force);
signal.set(12, SetAction::Immediate);
signal.set(12, SetAction::Deposit);
```

- `set(Bit)` / `set(Logic)` / `set(bool)`: scalar objects. `Logic` may write X/Z; `Bit`/`bool` cannot.
- `set(BitViewable T const&)`: width of `T` must equal object width. Routes through `gpi_set_signal_value_binstr`. The `gpi_set_signal_value_int` fast path is not used, so the read and write paths agree on X/Z handling and there is only one code path to maintain.
- `set(Array<T, N> const&)`: `N` must equal object length. Packed: one `binstr` write. Unpacked: per-element writes.
- `set(Vector<T> const&)`: dynamic width; range must equal object range.
- `set(int64_t)`: range-checked against object width and signedness. Value writes as a bit pattern via `binstr`; out-of-range values throw.
- `set(double)`: `gpi_set_signal_value_real`. `SimObjectKind::Real` only.
- `set(string_view)`: single overload, dispatched by `kind()`. On `SimObjectKind::String`, calls `gpi_set_signal_value_str`. On packed logic-vector kinds, calls `gpi_set_signal_value_binstr` (width must match). On other kinds, throws.

Users who want to write a bit pattern from a string literal to something that is *not* a logic-vector object construct the typed value themselves (`LogicVector{"1010"}`) and let the `BitViewable` overload route.

Like `get<T>()`, the `set` overload set is not closed. New source types can be added as new value types land in coconext.

## Freeze and release

`freeze()` and `release()` have no value argument. They correspond to the cocotb `Freeze` and `Release` actions and are exposed as methods rather than `SetAction` values so that call sites do not need to invent a dummy value to pass alongside them.

## Edges

Edge triggers live on the object:

- `rising_edge()` / `falling_edge()`: scalar-Logic kinds only. Throws on vectors.
- `value_change()`: scalars and vectors. On vectors, fires whenever any bit changes.

Free constructors (`triggers::rising_edge(obj)`, etc.) forward to the member. Awaitable types (`RisingEdge`, `FallingEdge`, `ValueChange`) are defined in the trigger module and are the same types users can construct directly.

## Metadata and identity

`kind()` returns the coconext classification. `def_name()` and `def_file()` return the defining module name and source-file path where the simulator can supply them (some cannot; expect empty views).

`size()` is the size in bits of the object where that is meaningful: bit width for integer and logic-vector kinds, total bits for packed vectors. For unpacked array kinds, `size()` is the element count. For scope kinds, `size()` is the number of children (best-effort; some simulators do not surface this until enumeration).

`range()` is the range of the object's *children*, for array-like kinds. Note the distinction: a packed vector's `range()` describes its children (bit slots), but its `size()` is the total bit count of the object. Range fields (`left`, `right`, `direction`, `length`) are accessed via the returned `Range`:

```cpp
signal.range().left;
signal.range().right;
signal.range().direction;
signal.range().length;
```

`is_signed()` is meaningful for integer and logic-array kinds. `is_const()` is meaningful for anything writable; a `true` result causes `set(...)`/`freeze()`/`release()` to throw.

Equality compares the underlying sim object, not the C++ pointer: two `SimObject`s obtained through different lookup paths that reach the same object compare equal. `std::hash<SimObject>` is keyed on `path()` so that the hash and equality relation agree without requiring stable pointer identity from the simulator. In practice, because of the weak-child cache, equal `SimObject`s will usually share the same `SimObjectImpl` too.

`bool` conversion is deliberately not provided. `if (obj)` is a bug (there is no null `SimObject`; the type has no default ctor and no invalid state), and cocotb's decision to `NotImplemented` `__bool__` exists to catch that mistake. In C++ we get the same effect for free by not defining a conversion; users who want existence checks use `try_child`.

## Obtaining the top-level SimObject

```cpp
SimObject top();
```

Returns a `SimObject` to the DUT root. How tests receive this (free-function call vs macro-injected parameter vs a global variable) is a separate decision; the sketch here assumes free-function access.

## Naming decisions worth calling out

- `SimObject` (not `Handle`). `Handle` says nothing about what the object represents; `SimObject` names the domain concept directly.
- `try_child` (not `get`, not `_get`, not `try_get`). `get` is the value getter. Cocotb's `_get` is underscored to avoid collision with HDL child names in Python's `__getattr__` namespace, a constraint we do not have; the underscore prefix is not appropriate for us.
- `children()` (not `keys`/`values`/`items`). Python-dict names carry the wrong intuition. `children()` yields the named-child pairs; the split into names-only and handles-only was rejected as unneeded surface.
- `size()` kept separate from `range().length()`. Integer-kind objects have no `Range` but still have a bit width; packed vectors have both, and they mean different things (bits vs child count).
- Range fields live on `Range` only. No `left()`/`right()`/`direction()` sugar on `SimObject`; call sites use `obj.range().left`.

## Not in this spec

- Static-structure downcast targets. The `downcast<T>()` slot exists but the constraints on `StaticSimObject`, and the shape of the static classes, are separate work.
- How tests are declared and how `top()` is bootstrapped (free function vs variable vs injected parameter).
- Awaitable machinery (`RisingEdge`, `FallingEdge`, `ValueChange`) beyond the shape of their entry points on `SimObject`.
- The GPI shim (`detail::SimObjectImpl`), including how it is constructed, how the weak-child cache handles concurrent lookups, and how kind classification is derived from GPI object types.
- Any switch from `shared_ptr<SimObjectImpl>` to an intrusive refcount. The public API is unchanged either way; profile before deciding.
