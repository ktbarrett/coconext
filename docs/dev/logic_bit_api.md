# `Logic` and `Bit` API Specification

This document specifies the API for the scalar HDL element types `Logic` and `Bit`. It is a design spec.

`Logic` and `Bit` are the core elements of arrays in HDL. `Logic` is a 9-value type that follows VHDL `std_ulogic`; SystemVerilog's 4-value type fits inside VHDL's 9-value type. Both support logical operators.

The cross-cutting formatting, hashing, and construction-vs-conversion conventions are specified in `conventions_api.md` and referenced here. `ResolveMethod` is also defined in `conventions_api.md`.

## Overview

- `Logic`  --  9-value HDL scalar (VHDL `std_ulogic`).
- `Bit`  --  2-value HDL scalar (`0`, `1`). `Bit` is a subtype of `Logic`: every `Bit` implicitly upcasts to `Logic` in mixed operations.
- `LogicType` (see [Concept](#is_logic--logictype)) is the concept that unifies both, and is the extension point for future related types like a `X01Z` 4-value scalar.

## Fulfils

* `LogicType`

## Header layout

```
cpp/include/coconext/types/
  logic.hpp      --  Logic, Bit, ResolveMethod, is_logic / LogicType,
                     UDLs, converting ctors, conversion operators, logical
                     operators, to_string, std::hash / std::formatter
  concepts.hpp   --  Integer / Character concepts consumed by Logic / Bit
                     converting ctors
```

Users include the umbrella `types.hpp` (or `logic.hpp` for the scalar surface alone). All public symbols live in `coconext::types`.

## The nine `Logic` values

`Logic` follows VHDL `std_ulogic` exactly. The nine values, in the order used by the internal `value_type` enum:

| Symbol | Meaning | Resolves under |
|--------|---------|----------------|
| `0` | Forcing 0 | any method |
| `1` | Forcing 1 | any method |
| `X` | Forcing unknown | `ZEROS` / `ONES` / `RANDOM` |
| `Z` | High impedance | `ZEROS` / `ONES` / `RANDOM` |
| `U` | Uninitialized | `ZEROS` / `ONES` / `RANDOM` |
| `W` | Weak unknown | `ZEROS` / `ONES` / `RANDOM` |
| `L` | Weak 0 | `WEAK` and above (maps to 0) |
| `H` | Weak 1 | `WEAK` and above (maps to 1) |
| `-` | Don't care | `ZEROS` / `ONES` / `RANDOM` |

SystemVerilog's 4-state model is a subset of VHDL's 9 state model (`0`, `1`, `X`, `Z`).
See `ResolveMethod` in `conventions_api.md` for the resolution rules.

## Construction

`Logic` and `Bit` are constructed via their nested `value_type` enum (exposed to the class scope with `using enum`), or via UDLs. The `value_type` ctor is `constexpr` and `noexcept`; the UDL is `consteval` (see [UDL rejection](#udl-rejection)).

```c++
Logic a;                 // default-constructs to Logic::_0
Logic b {Logic::X};      // named-value ctor
Logic c = Logic::_1;     // same
Bit   d {Bit::_1};

auto e = '0'_b;          // Bit   --  UDL, char literal
auto f = '0'_l;          // Logic
auto g = 'X'_l;          // Logic::X
auto h = '-'_l;          // Logic::DC (don't-care)
```

The UDL is defined only on `char` (`operator""_l(char)` / `operator""_b(char)`)  --  string-literal UDLs (`"01XZ"_l`) belong to `LogicArray` / `BitArray`, not to the scalar types. See `logic_array_bit_array_api.md`.

### UDL rejection

The UDLs are declared `consteval`:

```c++
consteval Logic operator""_l(char c);
consteval Bit   operator""_b(char c);
```

Both delegate to the corresponding converting ctor, which throws `std::invalid_argument` on an unrecognized character. Because the UDL is `consteval`, every invocation is required to be a constant expression, so the throw becomes a compile error rather than a runtime exception. Malformed literals like `'Q'_l` or `'2'_b` are rejected at compile time.

## Conversion

Conversions follow the convention in `conventions_api.md`: explicit converting ctors on the destination when the destination is one of our types, explicit conversion operators when the destination is a native or foreign type. `to_string` remains a free function for the reasons in `conventions_api.md` (return-type polymorphism: static `string_view` for scalars, allocated `string` for arrays).

All conversions are `constexpr`.

### Converting ctors on `Logic` / `Bit`

```c++
class Logic {
    template <Character C> constexpr explicit Logic(C c);              // char, any case; '-' allowed; throws on unknown
    constexpr explicit Logic(std::string_view s);                      // length must be 1; throws otherwise
    constexpr explicit Logic(char const* s);                           // string_view overload
    template <Integer I> constexpr explicit Logic(I v);                // 0 -> _0, 1 -> _1; throws otherwise
    constexpr explicit Logic(bool v) noexcept;                         // total
    constexpr Logic(Bit const& v) noexcept;                            // implicit upcast (see below)
    // ... value_type ctor, default ctor, etc.
};

class Bit {
    template <Character C> constexpr explicit Bit(C c);                // '0' / '1' only; throws otherwise
    constexpr explicit Bit(std::string_view s);
    constexpr explicit Bit(char const* s);
    template <Integer I> constexpr explicit Bit(I v);                  // 0 / 1 only
    constexpr explicit Bit(bool v) noexcept;                           // total
    constexpr explicit Bit(Logic const& v);                            // downcast; throws on non-0/1/L/H
    // ... value_type ctor, default ctor, etc.
};
```

`Integer` and `Character` are the concepts defined in `conventions_api.md`; they exclude each other, so the compiler picks the char overload for `Logic('0')` and the int overload for `Logic(0)`.

`Logic(bool)` and `Bit(bool)` are `explicit` and independent overloads, not silently reached through the `Integer` path. Their existence keeps `Logic(false)` unambiguous even though `bool` decays to `int` and `nullptr` in different contexts.

```c++
Logic('0');
Logic("0");
Logic(0);
Logic(false);
// These all work for Bit as well.
Bit(true);
Logic('X');
Logic("W");
Bit('X');                     // throws (non-resolvable)
```

The `Bit(Logic)` ctor is explicit and throws `std::invalid_argument` on any value other than `0`, `1`, `L`, or `H`  --  the downcast is lossy.

### Egress to native types

`Logic` and `Bit` expose `explicit` conversion operators for the native integer and character types they can meaningfully turn into. All are `constexpr`.

```c++
class Logic {
    template <Integer T>   constexpr explicit operator T() const;              // 0/L -> 0, 1/H -> 1, else throws
    template <Character C> constexpr explicit operator C() const noexcept;     // one of '0' '1' 'X' 'Z' 'U' 'W' 'L' 'H' '-'
};

class Bit {
    template <Integer T>   constexpr explicit operator T() const noexcept;     // 0 or 1
    template <Character C> constexpr explicit operator C() const noexcept;     // '0' or '1'
    // See [Implicit conversions] for `operator Logic`, `operator int`, `operator bool`.
};
```

Standard C++ cast spellings all fire the operator:

```c++
int(bit);                     // int, value 0 or 1
uint8_t(bit);                  // uint8_t, value 0 or 1
static_cast<std::size_t>(bit); // size_t, value 0 or 1  (useful for array indexing)
int(logic);                   // int, throws on U/X/Z/W/-
char(logic);                  // digit character '0' / '1' / 'X' / ...
```

No bounds check is required on the integer conversion: `Bit` and the resolvable subset of `Logic` fit losslessly in every predefined integer type. `Logic -> Integer` throws `std::invalid_argument` on non-`0`/`1`/`L`/`H` values; `Bit -> Integer` and both `-> Character` conversions are `noexcept`.

### Implicit conversions

`Bit` has three implicit conversion targets (the `Bit -> Integer<T>` explicit template above is separate and covers wider integers):

```c++
class Bit {
    constexpr operator Logic()  const noexcept;   // subtype upcast
    constexpr operator int()    const noexcept;   // 0 or 1
    explicit constexpr operator bool() const noexcept;
};
```

- **`Bit -> Logic`** is the subtype upcast (see [Ordering](#ordering) and [Logical operators](#logical-operators)). Implicit, `noexcept`, total. Implemented on `Bit` (not as a `Logic` converting ctor) so that the upcast doesn't require spelling `Logic(b)` at every mixed-type expression.
- **`Bit -> int`** is total and lossless (`Bit` is a 2-element numeric domain). Implicit so that `2 * bit` and `array[bit]` work without ceremony. Widening to other integer types goes through the explicit templated operator above.
- **`Bit -> bool`** is **`explicit`**. This is the same idiom as `std::optional` and `std::unique_ptr`: `if (b)` and `!b` still work (contextual conversion), but `b + 2` unambiguously picks `operator int()` rather than `operator bool()`, and `void f(bool); f(b);` requires spelling `f(bool(b))`.

`Logic` has none of these. Any egress from `Logic` to a native type goes through an `explicit` conversion operator that can throw on metavalues.

### `to_string`

```c++
constexpr std::string_view to_string(Logic const& v) noexcept;  // one of "0" "1" "X" "Z" "U" "W" "L" "H" "-"
constexpr std::string_view to_string(Bit const& v)   noexcept;  // "0" or "1"
```

`to_string` is a free function for the reason called out in `conventions_api.md`: different `to_string` overloads return different string-like types (`string_view` here vs `std::string` on `LogicArray`), and a conversion operator cannot express that.

`to_string` returns the terse one-character form suitable for concatenating into a bit string; the formatter (see [Formatting and hashing](#formatting-and-hashing)) produces the diagnostic form.

## Ordering

`Logic` and `Bit` only support `==` and `!=`. Total ordering is deliberately not provided  --  it is odd as soon as metavalues become involved (`'0'_l < 'X'_l` has no defensible answer).

Equality does not consider different types equal  --  a `Bit` is not `==` to a `Logic` even at the same value. Cross-type equality must be spelled explicitly (via the `Bit -> Logic` implicit upcast on one of the operands, if intended).

```c++
'1'_b == '1'_b;             // ok
'1'_b != '1'_l;             // does not compile (different types)
Logic('1'_b) == '1'_l;      // ok, upcast is explicit at the call site
```

Both operators are `constexpr noexcept`.

## Logical operators

`Logic` and `Bit` support the standard bitwise-logical operators `&`, `|`, `^`, `~`, and the compound-assignment forms `&=`, `|=`, `^=`. All are `constexpr noexcept`.

`Logic` is the wider supertype; `Bit` implicitly upcasts to `Logic` in mixed operations. The `Logic` truth tables follow VHDL `std_ulogic` for all nine input values.

```c++
auto a = 'x'_l;
auto b = '1'_b;

auto c = '0'_l | a;  // Logic
auto d = b & b;      // Bit
auto e = a ^ b;      // mixed results in Logic
auto f = ~b;

b != '1'_l;  // does not compile
b == '1'_b;
```

### Compound assignment

```c++
Logic  l; l &= '1'_l;   // Logic op= Logic
Logic  m; m |= '1'_b;   // Bit implicitly upcasts to Logic
Bit    p; p ^= '1'_b;   // Bit op= Bit

Bit    q; q &= '1'_l;   // ill-formed  --  Logic does not implicitly downcast to Bit
                        // (non-resolvable values can't be stored in a Bit)
```

C++ has no `~=` operator. The free function `inplace_not(v)` provides the in-place complement:

```c++
constexpr Logic& inplace_not(Logic& v) noexcept;
constexpr Bit&   inplace_not(Bit&   v) noexcept;
```

## Resolving

`Logic` and `Bit` support `resolve(ResolveMethod)` and a no-arg `resolve()` method. `resolve` resolves metavalues according to the `ResolveMethod` (see `conventions_api.md`) and always returns `std::optional<Bit>`. The optional is empty iff the value is not resolvable under the requested method.

```c++
std::optional<Bit> Logic::resolve(ResolveMethod method) const noexcept;
std::optional<Bit> Logic::resolve()                    const noexcept;  // defaults to WEAK

constexpr std::optional<Bit> Bit::resolve(ResolveMethod) const noexcept;   // always engaged
constexpr std::optional<Bit> Bit::resolve()              const noexcept;   // always engaged
```

The no-arg overload defaults to `ResolveMethod::WEAK` (accepts `0`, `1`, `L`, `H`; rejects the rest). This matches VHDL `std_logic`'s default resolution and is the most conservative loss-free choice.

`Bit::resolve` always returns an engaged optional, since `Bit` has no metavalues. It is kept for uniformity with `Logic::resolve` so that generic code over `LogicType` can treat both the same way.

```c++
if ('0'_l.resolve().value()) {
   // stuff
}

auto b = '-'_l.resolve(ResolveMethod::ZEROS);  // Bit::_0
auto c = '-'_l.resolve();                      // nullopt (WEAK rejects DC)
```

## Formatting and hashing

`Logic` and `Bit` are specialized under `std::hash` and `std::formatter`, following the conventions in `conventions_api.md`.

- `std::hash<Logic>` / `std::hash<Bit>` hash the underlying `value_type` enum. Both are `noexcept`.
- `std::formatter<Logic>` / `std::formatter<Bit>` take no format spec (an empty `{}` only) and produce `Logic{X}` / `Bit{0}` style output. Passing any spec character throws `std::format_error` at parse time.

`to_string` returns the terse one-character form (`"X"`, `"1"`) suitable for concatenating into a bit string; the formatter produces the diagnostic form. Both spellings exist for the reasons in `conventions_api.md`.

## `is_logic` / `LogicType`

```c++
template <typename T> struct is_logic : std::false_type {};
template <>           struct is_logic<Logic> : std::true_type {};
template <>           struct is_logic<Bit>   : std::true_type {};

template <typename T>
concept LogicType = is_logic<std::remove_cv_t<T>>::value;
```

`is_logic` is an opt-in trait, extendable by users specializing the primary template, that registers the given type as a `Logic`-like type. `LogicType` is the corresponding concept and strips top-level `const` / `volatile` before checking.

Currently `is_logic` is `true` only for `Logic` and `Bit`. There is space for other related types to be added in the future, e.g. an `X01Z` 4-value scalar. Users writing generic code over "any Logic-like scalar" template against `LogicType`.

## `constexpr` and `noexcept` contract

Every operation on `Logic` and `Bit` is marked `constexpr` and  --  with three exceptions  --  `noexcept`:

- The converting ctors that can fail (`Logic(char)` on unknown chars, `Logic(Integer)` on non-0/1, `Bit(char)` on non-`0`/`1`, `Bit(Integer)` on non-0/1, `Bit(Logic)` on non-`0`/`1`/`L`/`H`, and the `string_view` / `char const*` forms on length != 1 or unknown char) are **not** `noexcept`; they throw `std::invalid_argument`.
- `Logic`'s explicit `operator Integer<T>` is **not** `noexcept`; it throws `std::invalid_argument` on non-`0`/`1`/`L`/`H`. All other conversion operators (`Logic::operator Character`, `Bit::operator Integer`, `Bit::operator Character`, `Bit::operator Logic`, `Bit::operator int`, `Bit::operator bool`) are `noexcept`.
- The `std::formatter::parse` overload throws `std::format_error` on any non-empty spec, matching the standard formatter contract.

`Logic::resolve` is `noexcept`; failure is reported via an empty optional, not by throwing.

All storage, comparison, conversion-that-can't-fail, logical-operator, and hash paths are `noexcept constexpr`  --  so `Logic` and `Bit` are usable as NTTPs in `constexpr` contexts (subject to the standard's NTTP structural-type rules).
