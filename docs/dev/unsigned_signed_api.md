# `Unsigned<R>` and `Signed<R>` API Specification

This document specifies the API for the `Unsigned` and `Signed` fixed-width integer types in coconext. It is a design spec; the implementation has not been written. The closest existing approximation lives on `origin/unsigned-signed`, which this spec deliberately diverges from.

These types model VHDL `numeric_std`'s `unsigned`/`signed` with semantics informed by `fixed_pkg` `ufixed`/`sfixed` restricted to zero fractional bits. They are intended to share a storage primitive (`detail::Bits<W>`) and a resize API with future `Sfixed`/`Usfixed`/`Float` types.

## Overview

`Unsigned<R>` and `Signed<R>` are bit-arrays indexed by an HDL `Range`, interpreted respectively as unsigned binary and two's-complement signed integers. They are **proper subtypes** of `BitArray<R>`: every `Unsigned<R>`/`Signed<R>` *is* a `BitArray<R>` with added numeric semantics, and the upcast is implicit. Indexing, slicing, iteration, formatting-as-bits, bitwise operators, and concat all reach the BitArray surface either via the upcast or via forwarding members on Unsigned/Signed. Equality and hash are intentionally **not** routed through the upcast — they are per-(type, range) only; see [Comparisons](#comparisons) and [Hash](#hash).

Storage is `detail::Bits<W>`, a sign-agnostic two's-complement primitive with small-buffer optimization (SBO) for `W <= 64` (native `intN_t`/`uintN_t`) and roll-your-own `std::array<uint64_t, N>` for wider. The same primitive backs `BitArray` and the future `Sfixed`/`Usfixed`/`Float`. There is no width cap.

There is no `value()` accessor and no implicit native-int conversion. Egress to native integers is via explicit conversion operators (one per supported native type); see [Integer egress](#integer-egress). There are no dynamic-range counterparts (no `DynUnsigned`/`DynSigned`). All widths are part of the type.

## Range semantics

The HDL Range describes the **position labeling** of the bits, not the integer interpretation. The leftmost bit is always the most significant bit, regardless of direction. This matches `numeric_std`, which normalizes all inputs to `(length-1 downto 0)` before integer operations.

Operator results carry `{N-1 DOWNTO 0}` for length-only requests; `resize` honors explicitly-requested directions (see [`resize`](#resize)).

## Header layout (Layout 3)

```
cpp/include/coconext/types/
  int_base.hpp       — detail::Bits<W>           [internal]
  resize_mode.hpp    — overflow_mode / round_mode (shared with Sfixed/Ufixed/Float)
  unsigned.hpp       — Unsigned<R>: class + same-type ops + literal helpers (u8..u64)
                       + resize<...>(Unsigned) / as overloads
                       + std::formatter<Unsigned<R>> + std::hash<Unsigned<R>>
  signed.hpp         — Signed<R>: same shape (s8..s64 helpers); plus implicit
                       Unsigned->Signed converting ctor; plus all cross-type
                       Unsigned<->Signed operators (since they produce Signed)
```

Users include the umbrella `types.hpp` (or the per-type header). Users should NOT include individual files under `types/` directly. Dependency direction: `signed.hpp` depends on `unsigned.hpp`; never the reverse. The previous `int.hpp` one-line stub is removed.

All public symbols live in `coconext::types`. Internal helpers live in `coconext::types::detail`.

## NTTP forms

Same shape as `Array<T, ...>`, reusing `detail::make_logic_static_range` (also used by `LogicArray`/`BitArray`) so the DOWNTO default is consistent:

```
Unsigned<8>                  -> {7 DOWNTO 0}
Unsigned<Range{...}>         -> passthrough
Unsigned<7, 0>               -> {7 DOWNTO 0}
Unsigned<0, 7>               -> {0 TO 7}
Unsigned<3, 3>               -> {3 DOWNTO 3}  (defaults DOWNTO when L == R)
Unsigned<L, Direction, R>    -> {L Direction R}
```

Identical for `Signed`.

## Storage and `constexpr`

Storage is `detail::Bits<W>`, sign-agnostic.

**Public contract**: `constexpr` is supported for **SBO widths** (`R.length() <= 64`). All members and operators are marked `constexpr` uniformly; the SBO path is constexpr-callable everywhere.

The wide path is **implementation-defined** with respect to `constexpr`. Today's roll-your-own wide implementation is constexpr-friendly under C++20 (loops over `std::array<uint64_t, N>`); a future APInt-backed implementation would not be. The wording leaves room without breaking the contract.

Throws (out-of-range, divide-by-zero) make a specific call non-constexpr only when the throw actually fires.

The default-constructed value is `0`.

## Type-level introspection

- `Unsigned<R>::static_range` — the NTTP `Range`.
- `r.range()` — member returning `Range`.
- `r.size()` — member returning `size_t`, equal to `R.length()`, equal to the bit count. (Same name as `Array::size`. There is no `width()` member.)

No `std::numeric_limits` specialization in the initial spec. Generic numeric introspection (min, max, digits) is deferred; if needed, a coconext-native API will be added later.

## Concepts

- The existing `Integer` concept covers native integer types only.
- The customization point `is_int<T>` is `true` for native integers AND for any `Unsigned<R>` / `Signed<R>`. Users writing generic code over "any integer including the coconext fixed-width ones" template against `is_int` / the corresponding concept.

## Governing principle for conversions

> Operations which can change type without value loss are implicitly allowed. Lossy operations are explicit. We care about the value, not the type.

Two carve-outs for ergonomic universals: compound assignment wraps at LHS width; shifts are destructive. Both noted at the relevant operator sections.

## Construction

### Implicit (lossless)

- **From native integer**, statically lossless only:
  - `Unsigned<W>` ← unsigned `T` iff `bit_width(T) <= W`
  - `Signed<W>` ← signed `T` iff `bit_width(T) <= W`
  - `Signed<W>` ← unsigned `T` iff `bit_width(T) < W` (sign bit added)
- **Same-kind widening**:
  - `Unsigned<M> ← Unsigned<N>` for `M >= N` (zero-extension)
  - `Signed<M> ← Signed<N>` for `M >= N` (sign-extension)
- **Cross-kind widening**:
  - `Signed<M> ← Unsigned<N>` for `M > N` (always lossless)

The reverse (`Signed → Unsigned`) is never implicit — negative values fail. Stays explicit, value-checked.

### Explicit (lossy, runtime-checked)

All constructors below are `explicit` and throw `std::out_of_range` on values that don't fit. The width of the source type does NOT relax the check.

- `Unsigned<R>{}` / `Signed<R>{}` — value-initialized to 0.
- `explicit Unsigned<R>(T)` for native `Integer T` — runtime-checked; throws if not in `[0, 2^N)`. When the source value is a constant expression, the check fires at compile time (the throw makes the call non-constant-expression).
- `explicit Signed<R>(T)` — runtime-checked; throws if not in `[-2^(N-1), 2^(N-1))`. Constant-source folding as above.
- `explicit Unsigned<R>(Unsigned<R2>)` for narrowing — value-checked at runtime.
- `explicit Signed<R>(Signed<R2>)` for narrowing — value-checked at runtime.
- `explicit Unsigned<R>(Signed<R2>)` — value-checked at runtime. E.g. `Unsigned<3>(Signed<8>(5))` succeeds; `Unsigned<3>(Signed<8>(-1))` throws.
- `explicit Signed<R>(Unsigned<R2>)` for narrowing — value-checked at runtime. (The widening direction is implicit.)

Implementations should elide the runtime check when the source's representable range is statically known to fit in the destination (no-op widening through the explicit form).
- `explicit Unsigned<R>(BitArray<R2>)` — `static_assert(R.length() == R2.length())`, reinterprets bits as unsigned. Mismatched widths are a compile error.
- `explicit Signed<R>(BitArray<R2>)` — `static_assert(R.length() == R2.length())`, reinterprets bits as two's-complement signed. Mismatched widths are a compile error.

Copy-init from literal (`Unsigned<8> a = 0;`) is **ill-formed**: the `Integer` ctor is `explicit`, copy-init can't fire it. Use direct-init `Unsigned<8>(0)`, the helper `u8(0)`, or brace-init `Unsigned<8>{0}`. Idiomatic in the docs is `u8(0)` for literals at standard widths.

Construction from string / bit-literal is not directly provided. Use the BitArray UDL and then construct: `Unsigned<8>("11111111"_b)` (notation per BitArray spec).

## Subtype relationship with BitArray

`Unsigned<R>` and `Signed<R>` are implicitly convertible to `BitArray<R>`. This is *the* load-bearing implicit conversion of the design — all bit-level operators reach the BitArray surface either via this upcast (free functions over `LogicArrayType`/`RangedSequence`) or via forwarding members (indexing, slicing, iteration).

Round-trip identity: for any `u : Unsigned<R>`, `Unsigned<R>(BitArray<R>(u)) == u`. Same for `Signed`. The bit-order convention is whatever BitArray uses for its Range; the integer types adopt it.

## Reinterpret: `as<U>` (and deduced-target `as`)

`as` reinterprets the bit pattern between same-width sibling types. It has two forms, mirroring `resize`:

### Spelled-target form

```cpp
auto s = as<Signed<8>>(u8);    // unsigned bits -> two's-complement signed bits
auto u = as<Unsigned<8>>(s8);  // two's-complement bits -> unsigned bits
auto b = as<BitArray<8>>(u8);  // upcast (redundant with implicit, but explicit form)
auto u = as<Unsigned<8>>(b8);  // downcast from BitArray
```

### Deduced-target form

`as(x)` (no NTTP) returns `detail::auto_reinterpreted<T>` carrying x. Unsigned/Signed/BitArray have non-explicit converting ctors and assignment operators that accept this wrapper and reinterpret to the destination's type:

```cpp
Signed<8> s = as(u8);                // u8's bits reinterpreted as Signed<8>
Unsigned<8> u; u = as(s8);           // reinterpret-assign
void f(Unsigned<8>);  f(as(b8));     // destination type drives
```

Same `[[nodiscard]]` and generic-context hazard apply as for deduced-target `resize`.

### Common rules

- **Same width only.** Width-changing reinterprets are ill-formed: a `static_assert` fires when the destination width is statically known to differ from the source. The user must compose `as` and `resize` explicitly.
- Defined for `Unsigned <-> Signed <-> BitArray` at equal `R.length()`. Range direction need not match; the result carries the destination's `R`.
- `noexcept` — there is no error case.

### `as` vs `resize` complement

- `as` requires equal widths; destination type/signedness may differ.
- `resize` requires same kind; destination width may differ.
- For both axes changing simultaneously, compose: `as<Unsigned<8>>(resize<8>(s16))` or use deduced-target form at each step.

## Integer egress

Native-integer egress is via **explicit conversion operators**, one per supported destination type. No `to_int<T>` free function, no `value()` member, no implicit conversion.

### Supported destinations

Exactly the twelve native integer types in the project's `Integer` set, plus a separate `bool` for the contextual-conversion idiom:

```cpp
explicit operator signed char       () const noexcept(/* elided */);
explicit operator unsigned char     () const noexcept(/* elided */);
explicit operator short             () const noexcept(/* elided */);
explicit operator unsigned short    () const noexcept(/* elided */);
explicit operator int               () const noexcept(/* elided */);
explicit operator unsigned int      () const noexcept(/* elided */);
explicit operator long              () const noexcept(/* elided */);
explicit operator unsigned long     () const noexcept(/* elided */);
explicit operator long long         () const noexcept(/* elided */);
explicit operator unsigned long long() const noexcept(/* elided */);
explicit operator __int128_t        () const noexcept(/* elided */);  // GCC/Clang only
explicit operator __uint128_t       () const noexcept(/* elided */);  // GCC/Clang only

explicit operator bool              () const noexcept;
```

Character types (`char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`) are **excluded**: they are character types, not integer types, per the project's `is_int`/`is_char` split.

`__int128_t` / `__uint128_t` are GCC/Clang extensions and are conditionally compiled if `__SIZEOF_INT128__` is defined.

Enumeration over a templated operator is deliberate. It side-steps the constraint problem that `is_int<Unsigned<R>>` is itself `true` (which would make `Integer T` overlap with the same-family explicit ctors), and it gives each destination a unique overload slot. Implementations should delegate all twelve to a private `template <typename T> constexpr T to_native_int() const` to avoid code duplication.

### Semantics

Each operator returns the value of the source if it fits in `T`, otherwise throws `std::out_of_range`. When invoked in a constant-expression context with an out-of-range source, the throw becomes a compile error (same mechanism as the explicit narrowing ctors).

The `bool` operator is **distinct**: it returns `*this != 0` and never throws. This is what the user expects from `if (u) { ... }` — the throw-on-non-{0,1} semantics that would fall out of "value must fit in `bool`" would be wrong for the idiom.

Contextual-bool conversions consequently work as in C/C++:

```cpp
if (u) { ... }      // true iff u != 0
!u                  // true iff u == 0
u && v              // both non-zero
u || v              // either non-zero
```

The price is that a typo of `&&` for `&` (or `||` for `|`) produces a different but valid expression rather than a compile error. Accepted trade-off for having the idiom.

### Elision and `noexcept`

The runtime bounds check is elided (and the operator is `noexcept(true)`) when the source's representable range provably fits in `T`'s range. Formally:

- `Unsigned<W>` → unsigned `T`: elide iff `W <= numeric_limits<T>::digits`.
- `Unsigned<W>` → signed `T`: elide iff `W <= numeric_limits<T>::digits` (`digits` excludes the sign bit for signed `T`, so this is correct).
- `Signed<W>` → signed `T`: elide iff `W - 1 <= numeric_limits<T>::digits` (the `-1` accounts for the sign bit of the source).
- `Signed<W>` → unsigned `T`: never elide (the sign matters at runtime, even when the magnitude fits).

`noexcept(/* elided */)` follows the same predicate exactly. Users can statically enforce "this cast is safe at this width" with `static_assert(noexcept(int(u)))`.

### Spelling

Any of `int(u)`, `(int)u`, or `static_cast<int>(u)` fires the operator; no preference is enforced. Note that functional-cast notation requires a single-identifier type name, so `unsigned long long(u)` is a parse error — use `(unsigned long long)u` or an alias for multi-word types. `__int128_t(u)` works as written.

### Asymmetry with `Bit → int`

`Bit` has an implicit `int` and `bool` conversion (see `type_system1.md`). Unsigned/Signed require an explicit cast. The split: single-bit scalar types are trivially representable in any integer; multi-bit numeric types can overflow, so the explicit cast names the destination.

## Arithmetic

### Width growth

Binary arithmetic operators **grow**. Result widths follow `fixed_pkg`'s integer-only specializations:

| Op | `Unsigned<Wa> op Unsigned<Wb>` | `Signed<Wa> op Signed<Wb>` |
|----|--------------------------------|----------------------------|
| `+` | `Unsigned<max(Wa,Wb)+1>` | `Signed<max(Wa,Wb)+1>` |
| `-` | `Signed<max(Wa,Wb)+1>` (see below) | `Signed<max(Wa,Wb)+1>` |
| `*` | `Unsigned<Wa+Wb>` | `Signed<Wa+Wb>` |
| `/` | `Unsigned<Wa+1>` | `Signed<Wa+1>` |
| `%` | `Unsigned<Wb>` | `Signed<Wb>` |
| `rem` | `Unsigned<Wb>` | `Signed<Wb>` |

All result `Range`s are `{N-1 DOWNTO 0}`.

### Unary `-` and `+`

- `-Signed<W>` returns `Signed<W+1>` (grow-on-negate, matches the binary subtract rule).
- `-Unsigned<W>` returns `Signed<W+1>` (mathematically negative result must be Signed; lossless because `-(2^W - 1)` fits in `Signed<W+1>`).
- `+Unsigned<W>` returns `Signed<W+1>` — integral promotion, direct analogue of C++'s `+uint8_t{x}` becoming `int`. This is the explicit "make it signed" spelling for `auto` users who want to disambiguate.
- `+Signed<W>` is identity.

`abs(Signed<W>) -> Signed<W+1>` (free function).

### `Unsigned` subtraction

`Unsigned<Wa> - Unsigned<Wb>` returns `Signed<max(Wa,Wb)+1>`, not Unsigned. Diverges from `numeric_std` (which would wrap silently to Unsigned) in favor of the "we care about the value, not the type" principle.

The result range `[-(2^Wb - 1), 2^Wa - 1]` fits in `Signed<max(Wa,Wb)+1>` without loss. Users who specifically want wrap-to-Unsigned semantics (modular subtraction at a fixed width) write `as<Unsigned<W>>(resize(u - v))` or use compound assignment (`u -= v` wraps at u's width).

### Division and modulus

`/` and `%` (and `rem`) throw `std::domain_error` on zero divisor.

### Compound assignment and in/decrement

`+=`, `-=`, `*=`, `/=`, `%=`, `++`, `--` (both prefix and postfix) wrap at LHS width. They do **not** grow.

This is the carve-out from the "lossy is explicit" principle: the LHS *is* the user-named storage width, so the wrap is implicitly named by the assignment target. Universal C/C++/SV/VHDL idiom. RHS may be any compatible type (same-signed, mixed via Unsigned→Signed implicit promotion, native int via the type-driven implicit rule). `/=`, `%=` throw `std::domain_error` on zero divisor.

### Mixed signedness

`Unsigned op Signed` is **allowed** via the implicit Unsigned→Signed promotion. The previous spec's "mixed signedness is ill-formed" line is dropped.

Resolution: the implicit `Unsigned<A> → Signed<A+1>` lifts the Unsigned operand; the existing `Signed<X> op Signed<Y>` rule then applies.

Worked example:
```
Unsigned<8> + Signed<5>
  -> implicit-promote Unsigned<8> to Signed<9>
  -> Signed<9> + Signed<5>
  -> Signed<max(9, 5) + 1>
  -> Signed<10>
```

Compound assignment with mixed signedness wraps at LHS width per the compound-assignment rule. A negative result wrapped into Unsigned uses standard two's-complement wrap.

Lowered form, for `Unsigned<W> u` and `Signed<X> s`:

```
u op= s   <=>   u = as<Unsigned<W>>(resize<W>(Signed<...>(u) op s))
```

The inner expression follows the standard arithmetic rules (Unsigned→Signed promotion, then grow-on-op). The `resize<W>` narrows back to LHS width using `overflow_mode::wrap`. The `as<Unsigned<W>>` reinterprets the two's-complement bits as unsigned. Symmetrically for `Signed op= Unsigned`, the lowering reduces to `s = resize<X>(s op Signed<...>(u))` — no `as` needed since the result is already Signed.

### Mixed with native integer

Native integer operands are NOT admitted in binary arithmetic (`u + t`, `u - t`, `u * t`, etc.) or in comparisons (`u == t`, `u < t`, `u <=> t`). Even integer-literal RHS (`u + 5`) is ill-formed, despite the literal being convertible. The binary operator has no explicit destination type to drive the conversion to — and silently inflating to the smallest holding type, or attempting to deduce from the LHS alone, both lead to surprising results.

The user must spell the destination explicitly:
- Use a typed literal helper: `u + u8(5)`, `u == u8(0)`. The `u8`/`s8`/`u16`/`s16`/`u32`/`s32`/`u64`/`s64` helpers (see [Typed literal helpers](#typed-literal-helpers)) are the canonical way to spell typed literals.
- Or construct explicitly with the full type: `u + Unsigned<8>(t)`.

Native int is *not* a "useful at all costs" type in coconext arithmetic; HDL-shaped code carries explicit widths everywhere.

**Compound assignment is the exception**: `u += t` (native int RHS) is allowed because the LHS provides the destination type unambiguously. The native int constructs an `Unsigned<W>(t)` of LHS width (explicit ctor; runtime value-check, or compile-time check when `t` is a constant expression), then the standard compound rule fires (wrap at LHS width). Same logic for `-=`, `*=`, `/=`, `%=`, `++`, `--`.

### Shifts

`<<` and `>>` are **destructive**: return same `Unsigned<R>` / `Signed<R>`. No growth. This is the second carve-out from the principle — shift operators have a single universally-recognized HDL meaning (in-register manipulation).

- RHS may be native `int`, `Unsigned`, or `Signed`. For Unsigned/Signed RHS the value is read as a native shift amount via the `int` conversion operator.
- Negative shift amount: when the RHS is a constant expression (literal or `constexpr` variable), `static_assert` rejects at compile time. When the RHS is a runtime value (signed path only), throws `std::invalid_argument`.
- Amount `>= width()`: when the RHS is constant, `static_assert` is NOT used (this is well-defined collapse, not an error). At runtime, collapses to 0 for `Unsigned` and `Signed<<`; collapses to sign-extension (all-0 or all-1) for `Signed>>`. Implementations may optimize the collapse to a compile-time constant when both width and amount are known.
- `Signed>>` is arithmetic (sign-extending). `Unsigned>>` is logical.
- `<<=`, `>>=` follow with the same semantics.

There are no named `shift_left` / `shift_right` free functions.

## Bitwise operators

`&`, `|`, `^`, `~` are not defined on `Unsigned`/`Signed` directly. They fire on the `LogicArrayType` constraint (`RangedSequence<T> + LogicType<elem>`), which Unsigned/Signed match by being `RangedSequence<Bit>`. The result is `BitArray<R>` (or the most-specific common Array-like type for cross-shape operands). User converts back with `as<Unsigned<R>>(...)` if a numeric type is needed.

Length checks: when both operands have `StaticRangedSequence`-known widths (Unsigned, Signed, static BitArray, static LogicArray, etc.), a mismatched-width bitwise operation is rejected by `static_assert` at compile time. Runtime length-mismatch throw is reserved for the case where at least one operand has only a runtime-known range (Vector, ArraySlice). This matches the existing dispatch in `logic_array.hpp`.

Concretely:
- `u & v` (both Unsigned) → `BitArray<R>`
- `u = u & v` is **ill-formed** (BitArray doesn't implicitly become Unsigned)
- `u &= v` is **ill-formed** on Unsigned (no `operator&=` defined; the BitArray-returning `&` can't store back)
- `as<Unsigned<R>>(u & v)` is the recover pattern

This is an intentional asymmetry. Numeric types route bit-level operations through BitArray.

## Comparisons

Equality and ordering are **strict**: both operands must be the *exact same type*, including the same `R`. No implicit conversion participates in equality, including the `Unsigned`/`Signed` → `BitArray` upcast and the `Unsigned` → `Signed` widening promotion.

- `Unsigned<R> == Unsigned<R>` — well-formed, value equality.
- `Unsigned<R1> == Unsigned<R2>` where `R1 != R2` — ill-formed. There is no cross-range operator declared, and template-argument deduction does not consider implicit conversions on the deduced parameter, so the call fails by deduction failure rather than by a deleted operator. The same mechanism rules out cross-range `Signed == Signed`.
- `Unsigned == Signed` of any range — ill-formed. Users wanting cross-signedness comparison write `Signed<...>(u) == s` explicitly.
- `Unsigned == BitArray<R>` — ill-formed. Bit-pattern lookup against a `BitArray`-keyed collection requires `as<BitArray<R>>(u)` at the call site.
- `Signed == BitArray<R>` — ill-formed for the same reason.
- `Unsigned == native int` — ill-formed. Users construct an `Unsigned`/`Signed` from the int explicitly.

Ordering operators (`<`, `<=`, `>`, `>=`, `<=>`) have the same strictness.

Standard library generics that need same-type comparison (`std::min`, `std::max`) work; cross-type generics do not, by design.

This strictness pairs with `std::hash`: the equivalence class is exactly per-(type, range), and there is no hash collision between siblings (`Unsigned<8>` and `Signed<8>` of the same bit pattern hash differently). Equality and hash deliberately do **not** honor the subtype relationship — subtypes substitute for *operations* (bitwise ops, concat, formatting through BitArray), not for equality. This is a defensible weakening of strict Liskov substitutability in exchange for one simple, type-checkable rule.

## `resize`

`resize` is a free function shared with the future `Sfixed`/`Usfixed`/`Float`. Modes live in `cpp/include/coconext/types/resize_mode.hpp`:

```cpp
namespace coconext::types {

enum class overflow_mode { wrap, saturate };
enum class round_mode { truncate, round };

}
```

`round_mode` is accepted on Unsigned/Signed resizes for API consistency with the fixed-point family but is **always ignored** (no fractional bits to round).

### Two forms: spelled-target and deduced-target

The user can spell the target width, or defer it to a destination context (constructor or assignment) that knows its own width.

```cpp
// Spelled-target form: NTTPs name the result width.
template <auto... Args, /* Unsigned or Signed */ X>
constexpr /* Unsigned or Signed */ resize(X x);
template <auto... Args, X> constexpr ... resize(X x, overflow_mode);
template <auto... Args, X> constexpr ... resize(X x, overflow_mode, round_mode);

// Deduced-target form: no NTTP. Returns a deferred-resize wrapper that
// narrows when consumed by an Unsigned/Signed ctor or assignment.
//
// Two overloads, distinguished by value category of the source:
//   - lvalue source: wrapper holds `T const&`; no copy of x.
//   - prvalue source: wrapper holds `T` by value; the final consume moves
//     the carried value into the destination type's ctor.
template <typename T>
constexpr detail::auto_resized<T const&> resize(T const& x);
template <typename T>
constexpr detail::auto_resized<T> resize(T&& x);   // prvalue overload
// Both overloads also accept (overflow_mode) and
// (overflow_mode, round_mode = round_mode::truncate) trailing arguments.
```

The two forms disambiguate at the call site by whether an NTTP is present.

#### Spelled-target form

`Args...` accepts `<N>` (length), `<Range>`, `<L, R>`, `<L, D, R>` — the same sugar as the type templates. Result range:
- Length-only `<N>` → `{N-1 DOWNTO 0}` via the shared `make_logic_static_range` helper.
- Explicit `<Range>`, `<L, R>`, `<L, D, R>` → **honor** the user's requested direction.

Passing a `Signed`/`Unsigned`/`BitArray` *type* as a size argument is not supported.

#### Deduced-target form (deferred resize)

`resize(x)` (no NTTP) returns `detail::auto_resized<T>` carrying `x` plus the requested modes. Unsigned/Signed expose non-explicit converting ctors and assignment operators that accept this wrapper and narrow the contained value to the destination's `R`.

```cpp
Unsigned<8> a;
Unsigned<16> wide = ...;
a = resize(wide);                   // assignment-side: a's range drives narrowing
Unsigned<8> b = resize(wide);       // copy-init: b's range drives narrowing
void log(Unsigned<8>);
log(resize(wide));                  // parameter type drives narrowing
Counter c{resize(wide + u8(1))};    // field type drives narrowing
return resize(wide);                // return type drives narrowing

// Generic context: deduced target may not be a concrete Unsigned/Signed.
// Use the spelled-target form there:
template <typename T> void g(T x);
g(resize(wide));                    // T deduces to auto_resized<...> -- hazard
g(resize<8>(wide));                 // explicit width; T deduces to Unsigned<8>
```

The wrapper is `[[nodiscard]]` so the bare-statement case (`resize(wide);` on its own line) emits a warning. `auto x = resize(...)` binds without warning but `x` has type `auto_resized<...>` until consumed by a typed context; documented hazard.

The wrapper is **move-only** and **consumes its source at most once**:

- Copy constructor and copy assignment are deleted.
- The consume-side (the converting ctor / assignment on Unsigned/Signed) takes the wrapper by value (or by `&&`) and moves the carried `T` (prvalue overload) or rebinds the reference (lvalue overload) into the destination ctor.
- After consumption the wrapper is in the standard moved-from state; a second consume is ill-formed at compile time (rvalue-only consume) and, where defeated, results in a runtime check (the wrapper stores a single bit tracking consumption in debug builds).

`auto_reinterpreted<T>` follows the same contract.

Three distinct narrowing intents have three distinct spellings:
- `a = resize(wider)` — "I want a's width; wrap (or the explicit mode)."
- `Unsigned<8>{wider}` (explicit ctor) — "value-check, throw on overflow."
- `as<Unsigned<8>>(same_width)` — "reinterpret bits, no narrowing."

### Defaults

For Unsigned and Signed: `overflow_mode::wrap`, `round_mode::truncate` (ignored).

### Semantics

- **Widening** (`N >= W`): modes ignored. Value preserved exactly. Sign-extends for Signed, zero-extends for Unsigned. (Same outcome as the implicit widening conversion; `resize` widening is allowed for consistency in generic code.)
- **Narrowing with `wrap`**: truncate high bits (two's-complement reinterpret for Signed).
- **Narrowing with `saturate`**: clamp to target's representable range.

The cross-type form `resize` between `Signed` and `Unsigned` is **not** supported. Use `as` followed by `resize`, or `resize` followed by `as`.

## Iteration, indexing, slicing

Unsigned/Signed expose the BitArray surface via **forwarding members**, not via reaching through the implicit upcast. (Reaching through the upcast doesn't work for members — members don't participate in implicit conversion of `this`.)

### v1 surface

- `static constexpr Range static_range = R;` (satisfies `StaticRangedSequence`)
- `constexpr Range range() const noexcept;`
- `constexpr size_t size() const noexcept;`
- `operator[](index_type)` — single-bit **read**, returns `Bit` by value (storage is packed)
- `index<I>()` — compile-time-checked single-bit read
- `begin()/end()/rbegin()/rend()` returning `Bit`-yielding (read-only) iterators

### Deferred to BitProxy work

The `BitProxy` machinery is needed for `BitArray` on packed `Bits<W>` storage and will be designed once with the BitArray packing rework, reused across BitArray/Unsigned/Signed/Sfixed/Ufixed:

- Mutable `operator[](index_type)` returning a `BitProxy` with a back-pointer to packed storage.
- `operator[](Range)` → slice (type TBD; possibly a new `BitArraySlice` view that natively walks packed storage).
- `slice<R2>()` → static-bounded slice (same TBD).
- Mutating iterators yielding `BitProxy`.

Indexing follows the `Range`'s convention: for `Unsigned<7, 0>`, `u[0]` is the LSB; for `Unsigned<0, 7>`, `u[0]` is the MSB. In both cases the **left index is the high bit**.

## Free functions

- `as<U>(x)` — see [Reinterpret](#reinterpret-asu).
- `resize<...>(x, ...)` — see [`resize`](#resize).
- `abs(s)` for `Signed<W> -> Signed<W+1>`.
- `std::min`, `std::max` — work via the strict same-type comparison.

Bitwise free functions (`~`, `&`, `|`, `^`), `concat`, `and_reduce`, `or_reduce`, `xor_reduce` all bind on Unsigned/Signed through the `LogicArrayType` constraint. Result types are BitArray; use `as` to recover the numeric type. (`resolve` is LogicArray-only — it resolves metavalues; BitArray has none.)

### `concat` across the integer/bit family

`concat` is the canonical example of the BitArray-as-lingua-franca pattern. Operands of any mix of `Unsigned<R>`, `Signed<R>`, `BitArray<R>` (and future `Sfixed`/`Ufixed`/`Float` once they share `Bits<W>` storage) implicitly upcast to their `BitArray<R_i>` views; the result is `BitArray<R_concat>` with `R_concat.length() == sum of input lengths`.

```cpp
auto u = u8(0xAA);              // Unsigned<8>  bits 1010_1010
auto s = s4(-1);                // Signed<4>    bits 1111
auto b = "00001111"_b;          // BitArray<8>  bits 0000_1111

auto x = concat(u, s, b);       // BitArray<20>, bits 1010_1010_1111_0000_1111
                                // {19 DOWNTO 0}
```

No numeric semantics are preserved across the concat — the result is a pure bit pattern. Users who want a numeric type back wrap with `as<Unsigned<20>>(x)` or `as<Signed<20>>(x)`.

No `pow`, no `min`/`max` overloads beyond what `std` provides, no named shifts.

## Hash

`std::hash<Unsigned<R>>` and `std::hash<Signed<R>>` are specialized independently. Each mixes a per-type seed into the bit-pattern hash so that `Unsigned<R>` and `Signed<R>` of the same bit pattern hash to **different** values. The equivalence class is exactly per-(type, range), matching the strict-equality rule.

`std::hash<BitArray<R>>` is inherited from `std::hash<Array<Bit, R>>` and is also distinct from the Unsigned/Signed hashes at the same width.

Implementations should compute the bit-pattern hash from shared `Bits<W>` storage when available, then perturb with a type-distinguishing constant.

## Formatter

`std::formatter<Unsigned<R>>` and `std::formatter<Signed<R>>` are specialized. Format specs:

- `{}` / `{:d}` — decimal integer literal (default). E.g. `Unsigned[7 downto 0]{170}`
- `{:b}` — binary, BitArray-style. E.g. `Unsigned[7 downto 0]{"10101010"}`
- `{:o}` — octal string. E.g. `Unsigned[7 downto 0]{"252"}`
- `{:x}` — hexadecimal string. E.g. `Unsigned[7 downto 0]{"AA"}`

Output style is `Type[range]{body}` matching the array formatter convention.

`to_string` is **not** provided on `Unsigned`/`Signed`. For a bit-string representation use `std::format("{:b}", u)`; for decimal use `std::format("{}", u)` or `std::format("{:d}", u)`. `to_string` remains on `LogicVector`/`LogicArray`/`BitVector`/`BitArray` — those are the types where bit-string is the only natural string representation, so the function name carries unambiguous meaning. On numeric types it would invite confusion (decimal? bit-pattern? hex?) when `std::format` already covers all of them.

## Typed literal helpers

There are no user-defined literals (`_u8`, `_s8`, etc.) for the integer types. A set of `consteval` factory functions in `coconext::types` serves the same role:

```cpp
namespace coconext::types {

consteval Unsigned<8>  u8 (unsigned long long v);
consteval Signed<8>    s8 (long long          v);
consteval Unsigned<16> u16(unsigned long long v);
consteval Signed<16>   s16(long long          v);
consteval Unsigned<32> u32(unsigned long long v);
consteval Signed<32>   s32(long long          v);
consteval Unsigned<64> u64(unsigned long long v);
consteval Signed<64>   s64(long long          v);

}  // namespace coconext::types
```

The `uN` helpers take `unsigned long long`: passing a negative literal is a hard compile error (the conversion from a signed literal would either narrow under a warning or wrap silently, both confusing) and the full 64-bit range is reachable through `u64`. The `sN` helpers take `long long`: the parens scope unary `-` so both positive and negative literals work naturally.

User code:

```cpp
auto a = u8(5);            // Unsigned<8>{5}
auto b = s8(-128);         // Signed<8>{-128}     reaches min cleanly
auto c = u + u8(5);        // canonical "Unsigned + literal" spelling
auto d = s64(-9'223'372'036'854'775'807LL - 1);  // Signed<64>::min, no SBO cliff
```

`consteval` forces compile-time evaluation: passing a runtime value or an out-of-range value (e.g. `u8(256)`) is a hard compile error.

Note for `s64::min`: `s64(-9'223'372'036'854'775'807LL - 1)` reaches it cleanly. The literal `9223372036854775808` doesn't fit in `long long`, so the `- 1` trick is the standard workaround. Alternatively use the explicit ctor `Signed<64>(LLONG_MIN)`.

### Why consteval functions instead of UDLs

The "UDL takes only `unsigned long long`, so `-N_sN` produces a wider Signed" friction goes away when the literal is a function call — the parens scope the negation:

- `s64(-128)` directly constructs `Signed<64>{-128}`. No SBO cliff, no `_sN` returning `Unsigned<N-1>` twist, no wrapper machinery, no auto-deduction footgun.
- `s64(INT_MIN)` works without a min-value hole.
- Non-power-of-two widths work without adding new spellings: `Unsigned<24>(0xABCDEF)`.

The `consteval` guarantee makes the compile-time bounds-check unconditional, the same way a consteval UDL would.

### Why consteval functions instead of type aliases

A type alias (`using U8 = Unsigned<8>;` with usage `U8(5)`) achieves nearly the same ergonomics, but:
- Aliases would also legitimize `U8 x = ...;` as a type-spelling shorthand, which encourages relying on aliases instead of the canonical type name in declarations.
- The lowercase function form (`u8(5)`) reads as a literal-spelling helper rather than a type name, matching its single-purpose intent.
- `consteval` on the function forces compile-time evaluation; the alias-ctor form is `constexpr` and would only fold to compile-time in constant-evaluation contexts. The function form is stricter.

### Why no `u128`/`s128`

128-bit types are reserved for arithmetic results that genuinely outgrow 64-bit operands (`u64 * u64 → Unsigned<128>`, `s32 + s32 + ... → Signed<65>`, etc.). Direct 128-bit literals are not provided. Users who reach a 128-bit value should arrive there through the type system, not declare one from a constant.

### Why no `u1`/`s1`

`Unsigned<1>` and `Signed<1>` exist as types but are rarely used directly. Use `Bit{0}` / `Bit{1}` / `0_b` / `1_b` for single-bit values, and `Unsigned<1>(b)` / `Signed<1>(b)` when conversion to the integer type is needed.

### Scalar UDLs remain

Logic and Bit retain their UDLs (`'X'_l`, `'1'_b`) since they spell char-shaped literals where the ctor form is wordier and the UDL is the natural spelling. BitArray UDLs (e.g. `"0101"_b`) remain constructible into Unsigned/Signed via the explicit BitArray ctor. The string UDL `"01XZ"_l` produces a `LogicArray`, not a `BitArray`, so it is not directly constructible into Unsigned/Signed — resolve to bits first.

## Out of scope for this version

- Dynamic-range integer types (no `DynUnsigned`/`DynSigned`).
- `std::numeric_limits` specializations. (`Signed<W>::min()`/`max()` as members may be added separately.)
- String / bit-literal direct construction (go through BitArray UDL + explicit ctor).
- Named shift functions.
- `pow` / `**`.
- Formatter spec extensions beyond `{:d}`, `{:b}`, `{:o}`, `{:x}` (e.g. width/fill).
- `resize` between `Signed` and `Unsigned` (must go through `as`).
- Mutable `BitProxy` machinery (designed once with BitArray packing rework).
- Slice types for Unsigned/Signed on packed storage (TBD with BitProxy work; may introduce `BitArraySlice`).

## Open follow-ups

- An int-introspection facade if generic code needs min/max/digits without reaching for `std::numeric_limits`.
- A future formatter spec for width/fill/alignment.
- Decision on `_u1`/`_s1` if a use case appears.
- Selected non-power-of-two width helpers (`u12`, `u24`, `u48`) as demand arises.
