# `Handle` API Specification

This document specifies the API for the C++ simulator-object `Handle` in coconext. It is a design spec; the implementation has not been written.

`Handle` is the C++ analogue of cocotb's `cocotb.handle` module and the `dut` parameter to `cocotb.test`. It is a single dynamic handle type that supports every operation any simulator object can support, throwing at runtime when the underlying object does not support the requested operation. A future static-structure representation of the DUT will be reachable via `Handle::downcast<T>()`; that static surface is out of scope here.

## Overview

`Handle` wraps an opaque GPI object. It exposes:

- traversal by name and index, plus optional lookup
- named-child iteration for scopes; indexed traversal via the object's `Range`
- metadata (kind, path, defining module, signedness, constness, size)
- typed value read via `get<T>()` and typed value write via `set(v, action)`
- freeze/release
- edge and value-change awaitables (scalars: rising/falling/value_change; vectors: value_change only)
- equality and hashing on sim-object identity

All access routes through GPI. Reads on logic-storage handles always go through `gpi_get_signal_value_binstr` so that X/Z survive; the `get<T>` specialization is responsible for interpretation (rejecting X/Z where the target type requires it) and width checking.

There is no `begin()`/`end()` on `Handle`. Named iteration uses `children()`; indexed iteration uses the object's `range()`.

## Kinds

```cpp
enum class HandleKind {
    Module, GenerateBlock, GenerateArray,
    Logic, LogicVector, LogicArray,
    Integer, Real, String,
    Unknown,
};
```

`Unknown` is retained because some simulators surface objects whose kind we cannot classify; propagating an `Unknown` handle is preferred to throwing at construction.

The packed/unpacked distinction (e.g. packed `LogicVector` vs unpacked `LogicArray`-of-`Logic`) lives in `HandleKind` alone. It does not appear in the value-type API: `LogicVector` and `Vector<Logic>` are aliases, `BitVector` and `Vector<Bit>` are aliases, and the routing (single `binstr` call for packed, per-element calls for unpacked) is decided inside `get`/`set` based on `kind()`.

## Set actions

```cpp
enum class SetAction { Deposit, Force, Immediate };
```

Maps to `gpi_set_action` as: `Deposit` -> the scheduled deposit, `Force` -> force, `Immediate` -> `GPI_NO_DELAY`. `Freeze` and `Release` are value-less operations and are exposed as separate methods rather than actions. `_OldImmediate` is intentionally not surfaced.

## Class

```cpp
namespace coconext {

class Handle {
public:
    Handle() = delete;
    Handle(Handle const&) = default;
    Handle(Handle&&) = default;

    // ---- traversal ----------------------------------------------------
    Handle operator[](std::string_view name) const;   // throws if missing / wrong kind
    Handle operator[](std::size_t index)      const;   // throws if missing / wrong kind

    bool contains(std::string_view name)  const;
    bool contains(std::size_t index)      const;
    std::optional<Handle> try_child(std::string_view name) const;
    std::optional<Handle> try_child(std::size_t index)     const;

    // Named-scope iteration (Module, GenerateBlock).
    // Throws on non-scope kinds. Yields pair<std::string, Handle>.
    auto children() const;

    // Indexed traversal via the object's Range:
    //   for (auto i : bus.range()) do_something(bus[i]);
    Range range() const;

    // ---- metadata -----------------------------------------------------
    std::string_view name() const;
    std::string      path() const;
    HandleKind       kind() const;
    std::string_view def_name() const;
    std::string_view def_file() const;

    // Bit width for integer-kind handles; vector/array length otherwise.
    std::size_t size() const;

    bool is_const()  const;
    bool is_signed() const;

    // Same underlying sim object; hash keyed on path().
    friend bool operator==(Handle const&, Handle const&) noexcept;
    friend struct std::hash<Handle>;

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
    void set(std::string_view v,  SetAction = SetAction::Deposit);   // dispatched by kind

    void freeze();
    void release();

    // ---- edges --------------------------------------------------------
    RisingEdge  rising_edge()  const;   // scalar-Logic kinds
    FallingEdge falling_edge() const;   // scalar-Logic kinds
    ValueChange value_change() const;   // scalars and vectors

    // ---- future static downcast ---------------------------------------
    template <StaticHandle T>
    std::optional<T> downcast() const;

private:
    detail::GpiObject obj_;
};

Handle top();

}  // namespace coconext
```

Free trigger constructors (`rising_edge(Handle const&)`, `falling_edge(Handle const&)`, `value_change(Handle const&)`) live in `triggers.hpp` and forward to the corresponding member.

## Traversal

`operator[](string_view)` and `operator[](size_t)` throw on missing children or on kind mismatch (e.g. name lookup on a `LogicVector`). `contains` is the exception-free predicate; `try_child` is the exception-free fetch. Both forms exist for the same reason `std::map::contains` and `std::map::find` both exist: cheap check without a fetch, and fetch-or-default without a second lookup.

`children()` yields owned `pair<std::string, Handle>`. Names are owned to avoid depending on simulator-owned string lifetimes.

Indexed traversal has no dedicated method. Users obtain the object's `Range` and index through it:

```cpp
for (auto i : bus.range()) {
    bus[i].set(Bit{0});
}
```

This unifies the iteration surface with the range/direction semantics already used elsewhere in coconext and keeps handles from growing a second iterator API.

## Value reads

```cpp
template <typename T> T Handle::get() const;
```

All logic-storage reads route through `gpi_get_signal_value_binstr`. The specialization for `T` interprets the binstr and enforces width and value constraints:

- `get<Bit>()` / `get<Logic>()`: single-bit binstr; `Bit` rejects X/Z.
- `get<BitViewable T>()`: width of `T` must equal handle width; types in the 2-state family (`Unsigned<W>`, `Signed<W>`, `BitArray<W>`, `Usfixed`, `Sfixed`, `Float`) reject X/Z; `LogicArray`/`Vector<Logic>` do not.
- `get<Array<T, N>>()`: `N` must equal handle length. On packed handles, one `binstr` call is split into `N` elements. On unpacked handles, per-child GPI calls.
- `get<Vector<T>>()`: dynamic width; range filled from the handle. Packed/unpacked routing as above.
- `get<std::int64_t>()`: rejects X/Z; throws if handle carries more than 63 significant bits with the sign appropriate for `is_signed()`.
- `get<double>()`: `gpi_get_signal_value_real`. Applicable to `HandleKind::Real` only.
- `get<std::string>()`: dispatched by `kind()`. On `HandleKind::String`, calls `gpi_get_signal_value_str`. On packed logic-vector kinds, calls `gpi_get_signal_value_binstr` and returns the raw binstr. On other kinds, throws.

Width mismatches are always errors; there is no implicit resize on read. If a user has a 16-bit handle and wants an `Unsigned<8>`, they call `get<Unsigned<16>>()` and `resize<8>` explicitly.

## Value writes

Writes are an overload set on the source type. Each overload is responsible for picking the right GPI setter:

- `set(Bit)` / `set(Logic)` / `set(bool)`: scalar handles. `Bit` and `bool` throw if the handle carries an X/Z that would be overwritten? no; write is unconditional. `Logic` may write X/Z.
- `set(BitViewable T const&)`: width of `T` must equal handle width. Always routes through `gpi_set_signal_value_binstr`. The `gpi_set_signal_value_int` fast path is not used, so that the read and write paths agree on X/Z handling and there is only one code path to maintain.
- `set(Array<T, N> const&)`: `N` must equal handle length. Packed handle: one `binstr` write. Unpacked handle: per-element writes.
- `set(Vector<T> const&)`: dynamic width; range must equal handle range.
- `set(int64_t)`: range-checked against handle width and signedness. Value writes as a bit pattern via `binstr`; out-of-range values throw.
- `set(double)`: `gpi_set_signal_value_real`. `HandleKind::Real` only.
- `set(string_view)`: single overload, dispatched by `kind()`. On `HandleKind::String`, calls `gpi_set_signal_value_str`. On packed logic-vector kinds, calls `gpi_set_signal_value_binstr` (width must match). On other kinds, throws.

Users who want to write a bit pattern from a string literal to something that is *not* a logic-vector handle construct the typed value themselves (`LogicVector{"1010"}`) and let the `BitViewable` overload route.

## Freeze and release

`freeze()` and `release()` have no value argument. They correspond to the cocotb `Freeze` and `Release` actions and are exposed as methods rather than `SetAction` values so that call sites do not need to invent a dummy value to pass alongside them.

## Edges

Edge triggers live on the handle:

- `rising_edge()` / `falling_edge()`: scalar-Logic kinds only. Throws on vectors.
- `value_change()`: scalars and vectors. On vectors, fires whenever any bit changes.

Free constructors (`triggers::rising_edge(handle)`, etc.) forward to the member. Awaitable types (`RisingEdge`, `FallingEdge`, `ValueChange`) are defined in the trigger module and are the same types users can construct directly.

## Metadata and identity

`kind()` returns the coconext classification. `def_name()` and `def_file()` return the defining module name and source-file path where the simulator can supply them (some cannot; expect empty views).

`size()` has one meaning per kind:

- integer kinds: bit width
- vector/array kinds: `range().length()`
- scope kinds: number of children (best-effort; some simulators do not surface this until enumeration)

`is_signed()` is meaningful for integer and logic-array kinds. `is_const()` is meaningful for anything writable; a `true` result causes `set(...)`/`freeze()`/`release()` to throw.

Equality compares the underlying sim object, not the C++ pointer: two `Handle`s obtained through different lookup paths that reach the same object compare equal. `std::hash<Handle>` is keyed on `path()` so that the hash and equality relation agree without requiring stable pointer identity from the simulator.

`bool` conversion is deliberately not provided. `if (handle)` is a bug (there is no null `Handle`; the type has no default ctor and no invalid state), and cocotb's decision to `NotImplemented` `__bool__` exists to catch that mistake. In C++ we get the same effect for free by not defining a conversion; users who want existence checks use `contains` / `try_child`.

## Obtaining the top-level handle

```cpp
Handle top();
```

Returns a handle to the DUT root. How tests receive this (free-function call vs macro-injected parameter) is a separate decision; the sketch here assumes free-function access.

## Naming decisions worth calling out

- `try_child` (not `get`, not `_get`, not `try_get`). `get` is the value getter. Cocotb's `_get` is underscored to avoid collision with HDL child names in Python's `__getattr__` namespace, a constraint we do not have; the underscore prefix is not appropriate for us.
- `children()` (not `keys`/`values`/`items`). Python-dict names carry the wrong intuition. `children()` yields the named-child pairs; the split into names-only and handles-only was rejected as unneeded surface.
- `size()` (kept, not dropped in favor of `range().length()`). Integer-kind handles have no `Range` but still have a bit width.
- No `left()`/`right()`/`direction()` sugar on `Handle`. The `Range` fields are public; call sites use `handle.range().left`.

## Not in this spec

- Static-structure downcast targets. The `downcast<T>()` slot exists but the constraints on `StaticHandle`, and the shape of the static classes, are separate work.
- How tests are declared and how `top()` is bootstrapped.
- Awaitable machinery (`RisingEdge`, `FallingEdge`, `ValueChange`) beyond the shape of their entry points on `Handle`.
- The GPI shim (`detail::GpiObject`), including how it is constructed, how it is cached across handle copies, and how kind classification is derived from GPI object types.
