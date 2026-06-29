# `Sfixed<R>` and `Ufixed<R>` API Specification

This document specifies the API for the `Sfixed` and `Ufixed` fixed-point
types in coconext. It is a design spec; the implementation has not been
written.

These types model VHDL 2008 `ieee.fixed_pkg`'s `sfixed`/`ufixed`, adapted
to modern C++ conventions where the VHDL surface is awkward or under-defined.
They share storage (`detail::Bits<W>`), `resize`/`as`/`reverse` machinery,
and the subtype-of-`BitArray` relationship with `Unsigned<R>` and `Signed<R>`.
The Unsigned/Signed spec (`unsigned_signed_api.md`) is the prerequisite reading;
this spec frequently references it for shared behavior and only fully documents
the fixed-point-specific surface.

## Overview

`Sfixed<R>` and `Ufixed<R>` are bit-arrays indexed by an HDL `Range` whose
endpoints **carry numeric weight**: a bit at position `p` has weight `2^p`.
The `R.left` value names the highest-weight bit position (must be `>= -1`);
the `R.right` value names the lowest-weight bit position (must be `<= R.left`).
Positive positions are integer bits; negative positions are fractional bits.

`Sfixed<R>` interprets its bits as two's-complement signed; `Ufixed<R>` as
unsigned. They are **proper subtypes** of `BitArray<R>`: every `Sfixed<R>` /
`Ufixed<R>` *is* a `BitArray<R>` with added numeric semantics, and the upcast
is implicit. Bit-level operations (`~`, `&`, `|`, `^`, slicing, iteration,
`concat`) route through the BitArray surface. Equality and hash are per-(type,
range), not routed through the upcast (see Unsigned/Signed spec for the
rationale).

Storage is `detail::Bits<W>` with `W = R.length()`, sign-agnostic
two's-complement, SBO for `W <= 64` (and `W <= 128` where `__int128_t` is
available), `BigInt<W>` for wider. The same primitive backs `BitArray`,
`Unsigned`, `Signed`, and the future `Float`. There is no width cap.

There are no dynamic-range counterparts (no `DynSfixed`/`DynUfixed`). All
widths and radix-point positions are part of the type.

## Range semantics: position-carries-weight

VHDL `fixed_pkg`'s defining rule: the bit declared at `Range` position `p`
has numeric weight `2^p`. So for `Sfixed<3, -4>`:

| Position | Weight | Role |
|----------|--------|------|
| 3 | `2^3 = 8` (sign bit for Sfixed) | integer MSB |
| 2 | `2^2 = 4` | integer |
| 1 | `2^1 = 2` | integer |
| 0 | `2^0 = 1` | integer LSB |
| -1 | `2^-1 = 0.5` | fractional MSB |
| -2 | `2^-2 = 0.25` | fractional |
| -3 | `2^-3 = 0.125` | fractional |
| -4 | `2^-4 = 0.0625` | fractional LSB |

The `Sfixed<3, -4>` type holds 8 bits with the integer-bit range `[-8, 7]`
and resolution `2^-4 = 0.0625`. Total value range is `[-8.0, 7.9375]`.

For `Ufixed<3, -4>`, position 3 is just the integer MSB (no sign bit); value
range is `[0.0, 15.9375]`.

This differs from `Unsigned<R>` / `Signed<R>`, where `R` names arbitrary bit
*labels* and the leftmost bit is always the integer MSB regardless of position
or direction (matching `numeric_std`). The Unsigned/Signed convention is
preserved unchanged. The position-carries-weight rule applies only to
Sfixed/Ufixed.

### Constraints on `R`

For a well-formed `Sfixed<R>` / `Ufixed<R>`:

- `R.left >= -1`. (`R.left == -1` means zero integer bits — pure fractional.
  `R.left == -1` is the smallest representable highest-position; `R.left == 0`
  means one integer bit; etc.)
- `R.right <= R.left`. (Disallows degenerate cases where the rightmost weight
  position exceeds the leftmost.)
- `R.direction` may be `DOWNTO` or `TO`. See [Direction gating](#direction-gating).

For Sfixed: `R.length() >= 2` is recommended (one bit is the sign; one bit
holds at least some magnitude). A one-bit `Sfixed<-1, -1>` is well-formed
storage-wise but trivially represents `{-0.5, 0.0}` — useful for generic-code
edge cases, not for direct use.

For Ufixed: `R.length() >= 1`.

Violations of the above are rejected by `static_assert` at type-instantiation
time.

### Default direction sugar

There is no length-only NTTP shorthand for Sfixed/Ufixed: a length alone does
not name where the radix point sits. The two-argument `<L, R>` form and the
explicit `<Range{...}>` form are the only spellings.

```cpp
Sfixed<3, -4>              -> {3 DOWNTO -4}, 8 bits (4 int + 4 frac)
Sfixed<7, 0>               -> {7 DOWNTO 0},  8 bits (8 int + 0 frac) -- "Sfixed integer"
Sfixed<-1, -8>             -> {-1 DOWNTO -8}, 8 bits (0 int + 8 frac) -- pure-fractional
Sfixed<Range{...}>         -> passthrough
Sfixed<L, Direction, R>    -> {L Direction R}, three-arg form
```

When `L > R` and the two-arg form is used, direction defaults to `DOWNTO`
via `detail::make_logic_static_range` (shared with `LogicArray`/`BitArray`/
Unsigned/Signed). When the user wants TO direction, the three-arg form or
the explicit `Range{...}` form is required. Identical shape for Ufixed.

## Direction gating

A consequence of position-carries-weight: a TO-ranged Sfixed/Ufixed stores
bits whose storage layout *does not* match their numeric weight ordering. The
storage MSB holds the lowest-weight bit, the storage LSB holds the highest.
Arithmetic that reads storage as a number gives garbage.

Coconext rejects this at compile time on every numeric path:

- All arithmetic (`+`, `-`, `*`, `/`, `mod`, `rem`, unary `-`/`+`, compound
  assignment, shifts) `requires R.direction == Direction::DOWNTO` on every
  Sfixed/Ufixed operand.
- All comparisons (`==`, `<=>`, `<`, `<=`, `>`, `>=`) `requires DOWNTO`.
- The `resize` family `requires DOWNTO` for Sfixed/Ufixed sources and
  Sfixed/Ufixed destinations.
- `explicit operator double/float/long double` and the explicit integer-egress
  operators `requires DOWNTO`.
- The `double` ctor produces a DOWNTO-direction result type (it never
  produces a TO).
- `int_bits()` and `frac_bits()` accessors are direction-independent (the
  count of integer-positions and fractional-positions is a property of the
  position set, not the layout) and are available on both directions.

What remains available on TO-direction Sfixed/Ufixed:

- Storage, default-construct, copy/move
- `range()`, `size()`, `int_bits()`, `frac_bits()`
- Implicit upcast to `BitArray<R>`
- Indexing, iteration, slicing (BitArray surface)
- Bitwise ops (via BitArray surface)
- `as<U>(x)` reinterpret to a DOWNTO type (storage is preserved, type label
  is changed)
- `reverse(x)` (the bit-and-direction reverse — produces a same-kind result
  with DOWNTO direction and *preserves numeric value*)
- `to_string` does not exist on Sfixed/Ufixed (see Unsigned/Signed; same rule
  applies — bit-string `to_string` lives on LogicVector/LogicArray/BitVector/
  BitArray only)
- `std::formatter` (binary bit-pattern format; the decimal/rational format
  requires DOWNTO because it reads bits as a number)

This split means a Sfixed read out of a TO-ranged HDL handle can be inspected
(format binary, slice, copy, hash via BitArray) without operator failure, but
attempting `a + b` or `double(a)` on a TO-ranged value is a compile error with
a direct fix: either `as<Sfixed<reverse(R)>>(a)` if the user believes the bits
already follow DOWNTO weight despite the TO declaration, or `reverse(a)` if
the bits follow the declared (TO) weight and need to be physically
re-ordered. The user must choose; the library does not guess.

## `reverse(x)` and `as<U>(x)`

These are the two paths out of a TO-direction Sfixed/Ufixed. They produce
different bit patterns; the user picks based on what the source HDL meant.

### `reverse(x)`: numeric-preserving

```cpp
template <Range R> Sfixed<reverse_direction(R)> reverse(Sfixed<R> const&);
template <Range R> Ufixed<reverse_direction(R)> reverse(Ufixed<R> const&);
```

Element-wise bit reverse **and** range direction flip. Both happen together;
the user does not get half of one. For Sfixed/Ufixed (position-carries-
weight), this preserves numeric value across the direction flip — the bit
that was at user-visible position `p` (weight `2^p`) is at user-visible
position `p` (weight `2^p`) in the result; only the storage layout changes.

The same `reverse` free function applies more broadly across the array family
(specified separately):

- `reverse(Array<T, R>) -> Array<T, reverse_direction(R)>`
- `reverse(LogicArray<R>)`, `reverse(BitArray<R>)`, `reverse(Vector<...>)`,
  `reverse(LogicVector)`, `reverse(BitVector)`, `reverse(ArraySlice<...>)`,
  `reverse(StaticArraySlice<...>)` — type-preserving in each case
- `reverse(Unsigned<R>) -> Unsigned<reverse_direction(R)>`
- `reverse(Signed<R>) -> Signed<reverse_direction(R)>`

For Unsigned/Signed, `reverse` does the bit reversal (it cannot preserve
numeric value across direction flip there, because numeric_std's leftmost-
is-MSB rule means the same bit pattern has the same value regardless of
direction; reversing the bits gives a different number). For Sfixed/Ufixed,
`reverse` preserves numeric value. The function name is the same; the
numeric consequence differs by kind, mirroring how each kind interprets bits.

`reverse(x)` on a DOWNTO source is a no-op for the bits (storage is already
canonical) but still flips the range direction label, producing a TO result.
This is the inverse direction of the common use case and is rarely useful but
defined for symmetry.

### `as<U>(x)`: bit-pattern reinterpret

```cpp
auto d = as<Sfixed<3, -4>>(x_to);       // x_to has Range{-4, TO, 3}; storage preserved
```

Same-width reinterpret (mirrors `as` on Unsigned/Signed). Storage is bit-
identical between source and destination; only the type and Range labels
change. For a TO source labeled with positions whose weights *do not* match
their storage location, `as<U>` to a DOWNTO `Sfixed<R'>` produces a value
whose user-visible bit at position `p` has weight `2^p` according to `R'` —
which is *different* from what the source's labels said. The user is asserting
"the HDL's TO declaration was cosmetic; the bits already follow DOWNTO
weight."

Width-changing `as` is ill-formed (same rule as Unsigned/Signed). For cross-
kind reinterpret (Sfixed <-> Ufixed at equal R) and reinterpret with
BitArray, the same `as` machinery applies; the user-visible value differs
according to the destination kind's interpretation rule.

### Choosing between `reverse` and `as`

| User intent | Use |
|-------------|-----|
| The HDL signal was declared `(L DOWNTO R)` and read in cosmetically as TO; bits already follow DOWNTO weight | `as<Sfixed<L, DOWNTO, R>>(x)` |
| The HDL signal was declared `(L TO R)` with that direction carrying weight (rare but legal); bring it to a usable DOWNTO form | `reverse(x)` |

The library does not auto-pick. The choice depends on what the HDL author
meant, which the type system cannot infer.

## Header layout

```
cpp/include/coconext/types/
  int_base.hpp       -- detail::Bits<W>           [internal, existing]
  resize_mode.hpp    -- overflow_mode / round_mode  [shared with Unsigned/Signed]
  unsigned.hpp       -- Unsigned<R>                 [existing-spec]
  signed.hpp         -- Signed<R>                   [existing-spec]
  ufixed.hpp         -- Ufixed<R>: class + same-kind ops + resize<...>(Ufixed)
                       + as<U>/reverse overloads
                       + std::formatter<Ufixed<R>> + std::hash<Ufixed<R>>
  sfixed.hpp         -- Sfixed<R>: same shape; plus implicit Ufixed->Sfixed
                       converting ctor; plus all cross-kind Ufixed<->Sfixed
                       operators (since they produce Sfixed)
```

Users include the umbrella `types.hpp` (or a per-type header). Users should
NOT include individual files under `types/` directly. Dependency direction:
`sfixed.hpp` depends on `ufixed.hpp`; never the reverse. `ufixed.hpp` /
`sfixed.hpp` depend on `unsigned.hpp` / `signed.hpp` for the integer-subtype
implicit conversions and on `bitarray.hpp` for the subtype upcast.

All public symbols live in `coconext::types`. Internal helpers live in
`coconext::types::detail`.

## Storage and `constexpr`

Storage is `detail::Bits<W>` with `W = R.length()`. Default-constructed value
is `0` (numeric zero, all bits zero). Same constexpr contract as
Unsigned/Signed: SBO widths are constexpr-callable everywhere; the wide path
is implementation-defined.

## Type-level introspection

- `Sfixed<R>::static_range` / `Ufixed<R>::static_range` -- the NTTP `Range`.
- `r.range()` -- member returning `Range`.
- `r.size()` -- bit count, equal to `R.length()`.
- `r.int_bits()` -- `max(R.left, R.right) + 1` if that's positive, else `0`.
  Direction-independent. For `Sfixed<3, -4>`: returns 4 (positions 0..3 are
  integer).
- `r.frac_bits()` -- `-min(R.left, R.right)` if that's positive, else `0`.
  Direction-independent. For `Sfixed<3, -4>`: returns 4 (positions -1..-4 are
  fractional).
- `r.resolution()` -- the smallest representable positive increment, equal to
  `2^R.right` (or `2^R.left` for TO). Returns `double`. For `Sfixed<3, -4>`:
  returns `0.0625`.

No `std::numeric_limits` specialization in the initial spec.

For Sfixed: the sign-bit position is `R.left` (DOWNTO) or `R.right` (TO),
i.e. the highest-weight position. Available as `r.sign_bit_position()` if
needed (returns the integer position; the BitArray-surface access is
`r[r.sign_bit_position()]`). Deferred to the BitProxy work alongside other
bit access primitives.

## Concepts

- `is_int<T>` from the existing concepts file is **not** extended for
  Sfixed/Ufixed (they are not integers).
- A new customization point `is_fixed<T>` is `true` for any `Sfixed<R>` /
  `Ufixed<R>`. Users writing generic code over fixed-point types template
  against `Fixed` (the concept built on `is_fixed`).
- `is_numeric<T>` is `true` for native integers, `Unsigned`, `Signed`,
  `Sfixed`, `Ufixed`, and `Float`. The corresponding `Numeric` concept gates
  generic numeric-arithmetic code.

## Governing principle for conversions

The same principle as Unsigned/Signed:

> Operations which can change type without value loss are implicitly allowed.
> Lossy operations are explicit. We care about the value, not the type.

Two carve-outs for ergonomic universals: compound assignment wraps at LHS
range; shifts are destructive. Both noted at the relevant operator sections.

## Construction

### Implicit (lossless)

All implicit conversions require both source and destination Range to be
DOWNTO. (Cross-direction conversions go through `as` or `reverse`.)

- **From native integer**, statically lossless only:
  - `Ufixed<L, R>` <- unsigned `T` iff `bit_width(T) <= L + 1` (integer bits
    accommodate `T`; fractional bits zero-padded; `R` may be any value `<= L`
    including positive)
  - `Sfixed<L, R>` <- signed `T` iff `bit_width(T) <= L + 1`
  - `Sfixed<L, R>` <- unsigned `T` iff `bit_width(T) < L + 1` (sign bit added)
- **From Unsigned/Signed** (the integer-subtype lift):
  - `Ufixed<W-1, 0>` <- `Unsigned<W>` (same bit pattern, same value)
  - `Sfixed<W-1, 0>` <- `Signed<W>` (same bit pattern, same value)
  - `Sfixed<W, 0>` <- `Unsigned<W>` (sign bit added)
  - Wider Ufixed/Sfixed destinations follow by chaining through the same-kind
    widening below.
- **Same-kind widening** (lossless: more integer bits or more fractional bits):
  - `Ufixed<L', R'>` <- `Ufixed<L, R>` iff `L' >= L && R' <= R` (zero-extends
    high bits, zero-pads low bits)
  - `Sfixed<L', R'>` <- `Sfixed<L, R>` iff `L' >= L && R' <= R` (sign-extends
    high bits, zero-pads low bits)
- **Cross-kind widening** (lossless: a sign bit gains a position):
  - `Sfixed<L', R'>` <- `Ufixed<L, R>` iff `L' > L && R' <= R`

The reverse direction (`Sfixed -> Ufixed`) is never implicit -- negative
values fail. Stays explicit, value-checked.

### Explicit (lossy, runtime-checked)

All constructors below are `explicit` and throw `std::out_of_range` on values
that don't fit. Each accepts an optional `overflow_mode` argument; the
default is `overflow_mode::saturate` (diverges from Unsigned/Signed's `wrap`
default -- see [resize defaults](#resize) for the rationale).

- `Ufixed<R>{}` / `Sfixed<R>{}` -- value-initialized to 0.
- `explicit Ufixed<R>(Integer T)` -- range-checked. Throws if not in `[0,
  2^(L+1))`. Constant-source folding to a compile error.
- `explicit Sfixed<R>(Integer T)` -- range-checked. Throws if not in
  `[-2^L, 2^L)`. Constant-source folding to a compile error.
- `explicit Ufixed<R>(Ufixed<R2>)` for narrowing -- range-checked at runtime
  with the supplied `overflow_mode` and `round_mode`. Default
  `{saturate, round_to_even}`.
- `explicit Sfixed<R>(Sfixed<R2>)` for narrowing -- same.
- `explicit Ufixed<R>(Sfixed<R2>)` -- range-checked. Negative values throw
  under `saturate` (clamp to 0) or `wrap` (two's-complement reinterpret).
- `explicit Sfixed<R>(Ufixed<R2>)` for narrowing -- range-checked at runtime.
- `explicit Ufixed<R>(BitArray<R2>)` -- `static_assert(R.length() ==
  R2.length())`, reinterprets bits as unsigned with the specified radix
  position. Mismatched widths are a compile error.
- `explicit Sfixed<R>(BitArray<R2>)` -- same, two's-complement.

Implementations should elide the runtime check when the source's representable
range is statically known to fit in the destination.

#### Construction from `double` (and `float`, `long double`)

The natural input for fixed-point construction:

```cpp
explicit Ufixed<R>(double v);
explicit Ufixed<R>(double v, overflow_mode);
explicit Ufixed<R>(double v, overflow_mode, round_mode);
explicit Sfixed<R>(double v);
explicit Sfixed<R>(double v, overflow_mode);
explicit Sfixed<R>(double v, overflow_mode, round_mode);
```

Identical overload set with `float` and `long double` parameter types.
Defaults: `{saturate, round_to_even}`.

Behavior:
- The double's value is rounded to the destination's resolution (`2^R.right`)
  using the named `round_mode`.
- If the rounded value exceeds the destination's representable range, the
  `overflow_mode` decides: `saturate` clamps to `min`/`max`; `wrap` does
  two's-complement wrap of the integer part (the natural HDL wrap).
- `NaN` -> throws `std::domain_error` regardless of modes.
- `+inf` -> clamps to `max` under `saturate`; throws under `wrap` (no defined
  wrap for inf).
- `-inf` -> clamps to `min` under `saturate`; throws under `wrap`.
- Subnormal/denormal handling: rounded normally; modes apply.
- Constant-source folding: when `v` is a constant expression and the rounded
  value is in range, the call folds at compile time. Out-of-range under
  `saturate` also folds (to the clamped value). Out-of-range under `wrap`
  folds to the wrapped value. `NaN`/`inf` source at compile time becomes a
  compile error.

The explicit `double` form is the canonical literal spelling: `Sfixed<3,
-4>(0.625)` rather than a UDL or a typed-literal helper. There are no
`q8_8(...)`-style helpers; the ctor is the single channel. (See [Typed
literal helpers](#typed-literal-helpers).)

Copy-init from a literal (`Sfixed<3, -4> a = 0.5;`) is **ill-formed**: the
ctor is `explicit`. Use direct-init `Sfixed<3, -4>(0.5)` or brace-init
`Sfixed<3, -4>{0.5}`.

## Subtype relationship with BitArray

Identical to Unsigned/Signed. `Sfixed<R>` and `Ufixed<R>` are implicitly
convertible to `BitArray<R>`. Round-trip identity: for any `s : Sfixed<R>`,
`Sfixed<R>(BitArray<R>(s)) == s` (uses the explicit `BitArray` ctor on the
return). Bit-level ops reach BitArray via the upcast.

## Numeric egress

Native-numeric egress is via **explicit conversion operators**, one per
supported destination type. No `to_int<T>` free function, no `value()`
member, no implicit conversion. All operators below `require R.direction ==
Direction::DOWNTO`.

### Integer destinations

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
explicit operator __int128_t        () const noexcept(/* elided */);
explicit operator __uint128_t       () const noexcept(/* elided */);

explicit operator bool              () const noexcept;
```

Character types excluded (per the `is_int`/`is_char` split).
`__int128_t`/`__uint128_t` are conditional on `__SIZEOF_INT128__`.

**Rounding mode for integer egress: `round_to_zero` (C/C++ idiom).** This
diverges from the `resize` default (`round_to_even`) because user expectation
for `int(fp_value)` matches `(int)(double)fp_value` -- truncate toward zero.
Users wanting a different rounding mode for the integer conversion call
`resize<Integer-shape>(x, overflow_mode, round_mode)` first and then convert.

Bounds: returns the value of the source truncated-toward-zero if it fits in
`T`, otherwise throws `std::out_of_range`. Constant-source out-of-range folds
to compile error.

The `bool` operator is distinct: returns `*this != 0` and never throws.
Contextual conversions work as in C/C++.

### Float destinations

```cpp
explicit operator double            () const noexcept(/* elided */);
explicit operator float             () const noexcept(/* elided */);
explicit operator long double       () const noexcept(/* elided */);
```

Semantics: returns the exact rational value when the destination's mantissa
holds it; otherwise rounds to nearest-even (IEEE 754 default). `noexcept` is
unconditional -- float conversion never throws (overflow to infinity is the
defined IEEE behavior, not an exception).

The mantissa-fit predicate is:
- `double` (53 mantissa bits): exact iff `R.length() <= 53` and all bits
  of the source's magnitude fit within 53 contiguous bits anchored at the
  source's value. Always exact when `R.length() <= 53`.
- `float` (24 mantissa bits): exact iff `R.length() <= 24`.
- `long double`: platform-dependent; for x86-64 GCC/Clang (80-bit extended,
  64 mantissa bits) exact iff `R.length() <= 64`.

Implementations should mark the operator `noexcept` and elide the rounding
path when the mantissa-fit predicate holds at the static `R.length()`.

### Spelling

Any of `int(s)`, `(int)s`, `static_cast<int>(s)`, `double(s)` fires the
operator. Functional-cast parse limitations match Unsigned/Signed; use the
parenthesized cast for multi-word type names.

## Arithmetic

All arithmetic operators below `require R.direction == DOWNTO` on every
Sfixed/Ufixed operand.

### Width growth

Result widths follow VHDL fixed_pkg exactly for the kinds where VHDL's choice
is unambiguous; Ufixed - Ufixed diverges to produce Sfixed (consistent with
the divergence already taken for Unsigned).

| Op | `Ufixed<L1,R1> op Ufixed<L2,R2>` | `Sfixed<L1,R1> op Sfixed<L2,R2>` |
|----|----------------------------------|----------------------------------|
| `+` | `Ufixed<max(L1,L2)+1, min(R1,R2)>` | `Sfixed<max(L1,L2)+1, min(R1,R2)>` |
| `-` | `Sfixed<max(L1,L2)+1, min(R1,R2)>` (diverges from VHDL) | `Sfixed<max(L1,L2)+1, min(R1,R2)>` |
| `*` | `Ufixed<L1+L2+1, R1+R2>` | `Sfixed<L1+L2+1, R1+R2>` |
| `/` | `Ufixed<L1-R2, R1-L2-1>` | `Sfixed<L1-R2+1, R1-L2>` |
| `mod` | `Ufixed<min(L1,L2), min(R1,R2)>` | `Sfixed<L2, min(R1,R2)>` |
| `rem` | `Ufixed<min(L1,L2), min(R1,R2)>` | `Sfixed<min(L1,L2), min(R1,R2)>` |

All result `Range`s are DOWNTO.

The `*` rule grows the integer-side by 1: two max-magnitude Ufixed operands
multiply to a value that needs one more integer-bit than the operands had
combined. For `Ufixed<3,0>` (max 15) squared = 225, which needs 8 integer
bits (`Ufixed<7,0>`); `L1+L2+1 = 7`. Same +1 applies to Sfixed for the
two-negatives corner: `Sfixed<3,0>` (min -8) squared = 64, needs an integer
position holding `2^6`; `L1+L2+1 = 7`, so `Sfixed<7,0>` fits.

### Unary `-` and `+`

- `-Sfixed<L, R>` returns `Sfixed<L+1, R>` (grow-on-negate, matches binary
  subtract).
- `-Ufixed<L, R>` returns `Sfixed<L+1, R>` (mathematically negative result
  must be Sfixed; lossless because `-(2^(L+1) - 2^R)` fits in `Sfixed<L+1, R>`).
- `+Ufixed<L, R>` returns `Sfixed<L+1, R>` -- integral promotion; the
  explicit "make it signed" spelling for `auto` users.
- `+Sfixed<L, R>` is identity.

`abs(Sfixed<L, R>) -> Sfixed<L+1, R>` (free function).

### Division and modulus

`/`, `mod`, `rem` throw `std::domain_error` on zero divisor.

VHDL `mod` is the mathematical modulo (result has divisor's sign);
VHDL `rem` is the truncated remainder (result has dividend's sign).
Coconext follows VHDL semantics for both.

### Compound assignment and in/decrement

`+=`, `-=`, `*=`, `/=`, `%=`, `++`, `--` wrap at LHS range (LHS's `L` and `R`
unchanged; integer-overflow truncates high integer bits with two's-complement
wrap; fractional-narrowing rounds per a fixed `round_to_zero` -- compound
assignment cannot accept a per-call `round_mode` and must pick one). Same
carve-out as Unsigned/Signed.

`/=`, `%=` throw `std::domain_error` on zero divisor.

`++`/`--` increment by `1.0` (i.e. `2^0`). For a pure-fractional type
(`Sfixed<-1, -4>`, integer bits == 0), `1.0` overflows; `++` on such a type
saturates under default modes -- but more typically users do not `++` a pure-
fractional type. The operator is defined for uniformity.

### Mixed kinds

- **Ufixed op Sfixed** is allowed via the implicit `Ufixed<L,R> -> Sfixed<L+1,R>`
  promotion. The Ufixed operand lifts to Sfixed; the standard `Sfixed op
  Sfixed` rule applies. Result is always Sfixed.
- **Ufixed/Sfixed op Unsigned/Signed**: the Unsigned/Signed operand lifts to
  Ufixed/Sfixed via the implicit conversion (`Unsigned<W> -> Ufixed<W-1, 0>`,
  `Signed<W> -> Sfixed<W-1, 0>`), then standard mixed-kind rules apply.
- **Ufixed/Sfixed op native int**: NOT admitted in binary arithmetic. The user
  must construct an explicit `Sfixed`/`Ufixed`/`Signed`/`Unsigned` operand at
  the call site. Same rule as Unsigned/Signed.
- **Compound assignment with native int RHS IS allowed**: LHS drives the
  destination type unambiguously. Same rule as Unsigned/Signed.

### Shifts

`<<` and `>>` are **destructive**: return same `Sfixed<R>` / `Ufixed<R>`.
No growth. Same carve-out as Unsigned/Signed.

- RHS may be native `int`, `Unsigned`, or `Signed`. Sfixed/Ufixed RHS is
  ill-formed (a shift amount is dimensionless; a fixed-point shift amount
  would be a misuse).
- Negative shift amount: constant-expression rejected by `static_assert`;
  runtime rejected via `std::invalid_argument`.
- `Sfixed>>` is arithmetic (sign-extending). `Ufixed>>` is logical.
- `Sfixed<<` is logical (the sign-bit just shifts; the result's interpretation
  is whatever bits land in the sign position). Same for `Ufixed<<`.

Shift moves bits within the same `R` -- the radix point does **not** move.
A `Sfixed<3, -4> >> 1` keeps `Range{3 DOWNTO -4}`; the bit that was at
position 3 lands at position 2 (since position 2 is one step toward LSB in
the storage layout); the bit that was at position -4 falls off the bottom;
position 3 fills with the sign bit. Numerically this is *not* a division by
2 (which would adjust the radix point). For a div-by-2 with growth, write
the explicit `mul`/`div` with a `Sfixed<0, 0>(0.5)` operand, or use
`resize<...>(x * fixed_half)`.

This semantics matches `fixed_pkg`'s integer-only `sla`/`sra` operations
(shift amount in bits, not in scale). It diverges from the operators in
`fixed_pkg`'s `sfixed`/`ufixed`-specific shift functions that *do* move the
radix; the operator form here picks the unambiguous bit-shift meaning. Users
wanting scale-shift use multiplication.

### Bitwise operators

`&`, `|`, `^`, `~` are not defined on `Sfixed`/`Ufixed` directly. They fire
on the `LogicArrayType` constraint, which Sfixed/Ufixed match by being
`RangedSequence<Bit>`. Result is `BitArray<R>`. User recovers a fixed-point
type with `as<Sfixed<R>>(...)` or `as<Ufixed<R>>(...)`.

Length checks: same rule as Unsigned/Signed (compile-time for static-range
operands, runtime for dynamic).

This is intentionally asymmetric: numeric types route bit-level operations
through BitArray.

## Comparisons

Equality and ordering are **strict**: both operands must be the *exact same
type*, including the same `R`. No implicit conversion participates in
equality, including the BitArray upcast or the Ufixed -> Sfixed promotion.
Same rule as Unsigned/Signed.

All comparison operators `require R.direction == DOWNTO`.

- `Ufixed<R> == Ufixed<R>` -- well-formed; value equality (which for fixed-
  point is bit-pattern equality at the same R, since storage is canonical).
- `Ufixed<R1> == Ufixed<R2>` with `R1 != R2` -- ill-formed (deduction
  failure).
- `Ufixed == Sfixed` of any R -- ill-formed.
- `Ufixed == Unsigned` / `Sfixed == Signed` -- ill-formed (cross-kind).
- `Ufixed == BitArray<R>` -- ill-formed.
- `Ufixed == native int` -- ill-formed.
- `Ufixed == double` -- ill-formed (would require defining double-equality
  semantics for fixed-point, which has the usual float-equality pitfalls).

Ordering operators (`<`, `<=`, `>`, `>=`, `<=>`) have the same strictness.

`std::min`, `std::max` work via same-type comparison.

The strictness pairs with `std::hash`: equivalence class is per-(type, range);
no collision between siblings.

## `resize`

`resize` is a free function shared with Unsigned/Signed. The `overflow_mode`
and `round_mode` enums live in `cpp/include/coconext/types/resize_mode.hpp`.

### Mode enums

```cpp
namespace coconext::types {

enum class overflow_mode { wrap, saturate };

enum class round_mode {
    truncate,       // toward -infinity (VHDL fixed_truncate; aka floor)
    round,          // nearest, ties away from zero (VHDL fixed_round)
    round_to_even,  // nearest, ties to even (IEEE 754 default)
    round_to_zero,  // toward zero (C/C++ idiom)
    round_to_pos,   // toward +infinity (aka ceiling)
};

}  // namespace coconext::types
```

The `truncate` mode follows **VHDL semantics** (toward -infinity), not C++
semantics (which is toward zero). The names are aligned with VHDL `fixed_pkg`
for porting clarity; the IEEE-named modes are added for DSP/Float consumers.
The `truncate` vs `round_to_zero` split is the one footgun -- for Ufixed
they are identical (both drop low bits); for Sfixed they differ on negative
values (`truncate` is `floor`, `round_to_zero` is `(int)x`).

### Defaults

For Sfixed and Ufixed: `overflow_mode::saturate`, `round_mode::round_to_even`.

This diverges from Unsigned/Signed defaults (`wrap`, `truncate`). Rationale:
arithmetic on fixed-point values frequently grows the result range to where
the user's storage cannot hold all of it (e.g. `*` doubles the integer-bit
count), and a silent integer-truncation overflow loses high bits where the
user expected saturation; `round_to_even` is also the unbiased default for
DSP-shaped consumers. Unsigned/Signed defaults are tuned for the "modular
arithmetic at the LHS width" idiom where wrap is correct.

The asymmetry is intentional. Generic code can name the mode explicitly when
uniformity is needed.

### Two forms: spelled-target and deduced-target

Identical machinery to Unsigned/Signed. Spelled-target form takes the result
range as NTTP; deduced-target form returns `detail::auto_resized<T>` which
destination-context narrows.

```cpp
// Spelled-target form
template <auto... Args, /* Sfixed or Ufixed */ X>
constexpr /* Sfixed or Ufixed */ resize(X x);
template <auto... Args, X> constexpr ... resize(X x, overflow_mode);
template <auto... Args, X> constexpr ... resize(X x, overflow_mode, round_mode);

// Deduced-target form
template <typename T> constexpr detail::auto_resized<T const&> resize(T const& x);
template <typename T> constexpr detail::auto_resized<T> resize(T&& x);
// + 1-arg overflow_mode and 2-arg overflow_mode + round_mode trailing arguments
```

#### Spelled-target form

`Args...` accepts `<Range>`, `<L, R>`, `<L, D, R>`. The length-only `<N>`
sugar is **not** supported for Sfixed/Ufixed (length alone doesn't name the
radix point). Result range:
- Explicit `<Range>`, `<L, R>`, `<L, D, R>` -- honor the user's requested
  direction. Don't normalize.

#### Deduced-target form

Same wrapper machinery as Unsigned/Signed (`[[nodiscard]]`, move-only,
single-consume).

### Semantics

- **Widening** (`L' >= L && R' <= R`): modes ignored. Value preserved
  exactly. Sign-extends for Sfixed, zero-extends for Ufixed on the integer
  side; zero-pads the fractional side.
- **Narrowing on the integer side** with `wrap`: truncate high bits.
- **Narrowing on the integer side** with `saturate`: clamp to target's
  representable range.
- **Narrowing on the fractional side**: applies the named `round_mode` to
  round to the destination's resolution; then applies `overflow_mode` if
  rounding produced an out-of-range result (e.g. `round` of `0.99...` at the
  destination's max).

The cross-kind form `resize` between `Sfixed` and `Ufixed`, or between
`Sfixed`/`Ufixed` and `Signed`/`Unsigned`, is **not** supported. Use `as`
followed by `resize`, or `resize` followed by `as`.

`resize` `requires R.direction == DOWNTO` on both source and destination.

## Iteration, indexing, slicing

Identical to Unsigned/Signed. Forwarding members on Sfixed/Ufixed expose the
BitArray surface (read-only in v1; mutating access via BitProxy is deferred
with the BitArray packing rework).

Indexing follows the Range's convention: for `Sfixed<3, -4>` (DOWNTO),
`s[3]` is the sign bit, `s[-4]` is the fractional LSB. For `Sfixed<-4, TO,
3>` (TO), `s[3]` is still the sign bit (position-carries-weight; the user
addresses by *position*, not by ordinal index). The implementation walks
storage according to direction.

The integer ordinal-indexing helper `s.at_ordinal(0)` returns the leftmost-in-
storage bit; rarely needed at the user level. Defer to BitProxy work.

## Free functions

- `as<U>(x)` -- see [Reinterpret and reverse](#reversex-and-asux).
- `reverse(x)` -- see [Reinterpret and reverse](#reversex-and-asux).
- `resize<...>(x, ...)` -- see [`resize`](#resize).
- `abs(s)` for `Sfixed<L, R> -> Sfixed<L+1, R>`.
- `floor(x)`, `ceil(x)`, `round(x)`, `trunc(x)` -- return same kind, `R` =
  `{L DOWNTO 0}` (integer-only). These are `resize`-with-the-corresponding-
  rounding-mode wrappers, named for the std-math idiom.
- `std::min`, `std::max` -- strict same-type comparison.

Bitwise free functions (`~`, `&`, `|`, `^`), `concat`, `and_reduce`,
`or_reduce`, `xor_reduce` all bind on Sfixed/Ufixed through the
`LogicArrayType` constraint. Result types are BitArray; use `as` to recover
the numeric type.

### `concat` across the integer/fixed/bit family

Same machinery as Unsigned/Signed. Operands implicitly upcast to their
`BitArray<R_i>` views; result is `BitArray<R_concat>` with sum of input
lengths. No numeric semantics preserved across concat -- recover with
`as<...>(...)`.

## Hash

`std::hash<Sfixed<R>>` and `std::hash<Ufixed<R>>` are specialized
independently, mixing a per-type seed into the bit-pattern hash. Same
strict-equivalence-class rule as Unsigned/Signed: Sfixed/Ufixed/BitArray of
the same bit pattern hash to different values.

## Formatter

`std::formatter<Sfixed<R>>` and `std::formatter<Ufixed<R>>` are specialized.
Format specs:

- `{}` / `{:d}` -- rational decimal. E.g. `Sfixed[3 downto -4]{5.0625}`.
  Precision is `frac_bits()` digits past the point; the value is exact.
- `{:b}` -- binary, radix-point inserted at position 0. E.g.
  `Sfixed[3 downto -4]{"0101.0001"}`. The point is omitted when
  `int_bits() == 0` or `frac_bits() == 0`.

The `{:o}` and `{:x}` specs are not provided in v1 -- octal/hex digits don't
align with the binary radix point cleanly (4-bit hex digit may straddle the
point), and the design choice (insert a literal radix character, pre-/post-
pad with zero-quartets, or omit the radix) is non-obvious. Deferred until a
user asks.

Output style is `Type[range]{body}` matching the array formatter convention.

The decimal/rational format `requires R.direction == DOWNTO`. The binary
format works on both directions (bit-pattern formatting doesn't need the
weight convention).

## Typed literal helpers

There are no typed literal helpers (no `q8_8(...)`, no `sfixed<L,R>(...)`)
and no UDLs (no `_sf48`, etc.) for fixed-point types. The ctor is the single
spelling:

```cpp
auto a = Sfixed<3, -4>(0.625);
auto b = Ufixed<7, 0>(170);
auto c = Sfixed<7, -8>(-12.5);
```

The position-pair `<L, R>` carries too much information to bake into a
finite menu of named helpers, and a helper that takes `<L, R>` as template
arguments duplicates the ctor verbatim with no ergonomic gain. Q-format
helpers (`q8_8`, `uq4_28`, etc.) were considered and rejected as a partial
solution that legitimizes a particular sub-convention while leaving other
common shapes (e.g. `Sfixed<-1, -8>` pure-fractional) without a helper.

If a usage pattern emerges where a helper saves meaningful typing, add it
later -- this is an additive change.

## `to_string`

`to_string` is **not** provided on Sfixed/Ufixed (nor on Unsigned/Signed --
see the corresponding spec edit). For a bit-string representation use
`std::format("{:b}", x)`; for a decimal/rational use `std::format("{}", x)`
or `std::format("{:d}", x)`.

`to_string` remains on LogicVector/LogicArray/BitVector/BitArray -- those are
the types where a bit-string representation is the only natural string
representation, so the function name carries unambiguous meaning. Adding it
to the numeric types would invite confusion (decimal? bit-pattern?
hexadecimal?) and the answer is already available through `std::format`.

## Out of scope for this version

- Dynamic-range fixed-point types (no `DynSfixed`/`DynUfixed`).
- `std::numeric_limits` specializations. (`Sfixed<R>::min()`/`max()`/
  `resolution()` as static members may be added separately.)
- String / bit-literal direct construction (go through BitArray UDL +
  explicit ctor).
- Q-format / UDL / typed-literal helpers.
- Formatter specs beyond `{:d}` and `{:b}` (no `{:o}`, no `{:x}`, no
  width/fill).
- Float / decimal scientific (`{:e}`, `{:g}`) formatter specs.
- `pow` / `**`.
- `to_string`.
- `resize` between `Sfixed` and `Ufixed`, or between Sfixed/Ufixed and
  Signed/Unsigned (compose `as` + `resize`).
- Scale-shift operators (use multiplication by a `Sfixed<0, 0>` scalar).
- Mutable `BitProxy` machinery (designed once with BitArray packing rework).
- Sign-bit position accessor as a member (deferred with BitProxy work).
- `at_ordinal` indexing helper (deferred with BitProxy work).

## Open follow-ups

- An int-introspection facade if generic code needs min/max/digits without
  reaching for `std::numeric_limits`.
- Decision on `{:e}` / `{:g}` formatter specs for scientific display.
- Octal/hex formatter spec with a fixed radix-character convention.
- Q-format or named-shape helpers if a clear pattern emerges.
- The shared `Float<R>` type (separate spec; will use the same `Bits<W>`
  storage, the same `resize_mode.hpp` enums, and similar implicit-conversion
  shape).
