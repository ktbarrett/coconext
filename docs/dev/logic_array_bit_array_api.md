# `LogicArray` / `BitArray` and `LogicVector` / `BitVector` API Specification

This document specifies the API for the `Logic`- and `Bit`-element specializations of `Array` and `Vector`, along with the bitwise-logical operators that dispatch across the whole `RangedSequence<LogicType>` family. It is a design spec.

The cross-cutting formatting, hashing, construction-vs-conversion, and runtime-vs-compile-time-bounded conventions live in `conventions_api.md`. `Logic`, `Bit`, and the `LogicType` concept are specified in `logic_bit_api.md`. `Range` and `Direction` are in `range_direction_api.md`. `Array`, `Vector`, and their slice types are in `array_vector_api.md` and `array_slice_api.md`. `RangedSequence` / `StaticRangedSequence` and the cross-type binary-operation result rules are in `ranged_sequence_api.md`.

## `LogicArray` and `BitArray`

`Array<Logic>`- and `Array<Bit>`-shaped types, spelled `LogicArray` and `BitArray`. They share `Array`'s storage layout and API surface, add overloads for bitwise-logical operations, and default the length-only shape argument to `N-1 downto 0` (HDL convention) instead of `Array`'s `0 to N-1`.

### Fulfils

* `StaticRangedSequence`
* `RangedSequence`
* `std::ranges::sized_range`

### Constructors

Very similar to `Array`, `LogicArray` and `BitArray` use a variadic template for specifying the `Range`; the element type is hardcoded to `Logic` or `Bit`. The template accepts the same four shape-arg forms as `Array`, with one deliberate difference in the length-only default:

* **1 shape arg as a length**  --  deduces `Range` to `length-1 downto 0` (HDL convention; differs from `Array`'s `0 to length-1`).
* **1 shape arg as a `Range`**  --  uses the given `Range` directly.
* **2 shape args**, `left` and `right`  --  deduces direction so the `Range` is non-null.
* **3 shape args**, `left`, `direction`, `right`  --  directly constructs a `Range`.

```c++
// just length
LogicArray<8> a;
// left and right with inferred direction
LogicArray<10, 0> b;
// left, direction, and right
LogicArray<1, Direction::TO, 8> c;
// Range object
Ufixed<10, 0> d;
LogicArray<d.static_range> e;
```

Just like `Array`, `LogicArray` and `BitArray` can be constructed from initializer lists, `std::ranges::sized_range`s, or defaulted.

```c++
// initializer list
LogicArray<4> a {'0'_l, '1'_b, 'X'_l, '-'_l};
// sized_range
Vector<Bit> b {'0'_b, '1'_b, '0'_b, '1'_b};
LogicArray<4> c (b);
// defaulted
BitArray<7, 0> d;
// runtime string, length must equal N; throws std::invalid_argument on mismatch or bad char
LogicArray<4> e (std::string_view{"01XZ"});
BitArray<4>   f (std::string_view{"0110"});
```

The `std::string_view` ctor is `explicit`. Each character is validated with the same rules as the scalar `Logic(char)` / `Bit(char)` ctors; unrecognized characters throw `std::invalid_argument`. Length mismatch against the compile-time `N` also throws `std::invalid_argument`. Leftmost character maps to the leftmost storage slot (MSB under the default `N-1 downto 0`).

More ergonomic than the constructors are the UDLs. They use the same `_l` and `_b` suffixes as `Logic` and `Bit`, but are specified with strings instead of individual `char` literals. The `Range` is inferred to be `length-1 downto 0`.

```c++
auto a = "01XZ"_l;   // LogicArray[3 downto 0]{"01XZ"}
auto b = "0110"_b;   // BitArray[3 downto 0]{"0110"}
auto c = "01XZ"_b;   // compile error: X/Z/U/W/L/H/- not representable as Bit
```

The `_b` UDL accepts only `'0'` and `'1'`; metavalue characters are a compile error. See [String UDL rejection](#string-udl-rejection) below.

### String UDL rejection

The string-literal UDLs are declared `consteval`:

```c++
template <StringLiteral S> consteval auto operator""_l();   // LogicArray<S.size()>
template <StringLiteral S> consteval auto operator""_b();   // BitArray<S.size()>
```

Each returns a `LogicArray` / `BitArray` whose length equals the literal's length, and validates every character at compile time. `_l` rejects anything outside the nine `std_ulogic` characters; `_b` further restricts to `'0'` and `'1'`. Because the UDL is `consteval`, invalid input becomes a compile error rather than a runtime exception: malformed literals like `"Q01"_l` or `"01X"_b` fail at the call site.

### Cross-element construction

Cross-element construction (`LogicArray` from `BitArray`, `BitArray` from `LogicArray`) goes through the sized-range ctor inherited from `Array` / `Vector`. That ctor is `explicit` and requires `std::constructible_from<element_type, range_value_t<R>>`, which picks up:

- `LogicArray<N>(bit_arr)` via the implicit `Bit -> Logic` upcast at each element.
- `BitArray<N>(logic_arr)` via the explicit `Bit(Logic)` ctor  --  throws `std::invalid_argument` at the first element that isn't `0`, `1`, `L`, or `H`.

```c++
BitArray<4>   b {"0110"_b};
LogicArray<4> l (b);      // ok, elementwise Bit -> Logic
LogicVector   lv (b);     // ok, range copied from b

LogicArray<4> l2 = b;     // ill-formed: sized-range ctor is explicit

LogicArray<4> l3 {"01XZ"_l};
BitArray<4>   b2 (l3);    // throws std::invalid_argument: X not resolvable to Bit
```

No dedicated cross-container converting ctor is needed  --  the sized-range path covers every combination of static/dynamic bounds and `Logic`/`Bit` element types.

### Conversion

`to_string` is the one-way egress:

```c++
to_string("01XZ"_l);     // "01XZ"
to_string("010101"_b);   // "010101"
```

There is no `to_logic_array` / `to_bit_array` free function. Compile-time construction from a string literal uses the UDL (`"01XZ"_l`, `"010101"_b`); runtime construction from a `std::string_view` uses the `explicit` string ctor (see [Constructors](#constructors)).

### Iteration

These types are at their core still `Array<Logic>` and `Array<Bit>`, and thus are `sized` `random_access` `range`s of `Logic` and `Bit`.

### Ordering

`LogicArray` and `BitArray` support `==` and `!=`. Equality is strict on the full type: same container kind, same element type, same `Range` (compile-time-equal), same values. `LogicArray` compared against `LogicVector`, or `LogicArray` against `BitArray`, is a compile error  --  matching the scalar rule that `Bit == Logic` doesn't compile. Use `std::ranges::equal` for element-wise comparison across container kinds or element types.

These are still essentially `Array`s of `Logic` or `Bit`; they don't have numeric semantics, so there is no total ordering.

### `std::find`, `index_of`, and `rindex_of`

Same as `Array`.

### Formatting and hashing

`LogicArray` and `BitArray` support `std::hash` and `std::formatter`, per the conventions in `conventions_api.md`. `std::hash` hashes only the elements  --  the `Range` is part of the type.

`std::formatter` produces `LogicArray[range]{"bitstring"}` / `BitArray[range]{"bitstring"}`, e.g. `LogicArray[3 downto 0]{"01XZ"}`, `BitArray[7 downto 0]{"01010101"}`. The bit-string form matches the UDL and `to_string` representation and can be round-tripped through the `std::string_view` ctor. Takes no format spec; any spec character throws `std::format_error` at parse time.

## `LogicVector` and `BitVector`

`Vector<Logic>`- and `Vector<Bit>`-shaped types, spelled `LogicVector` and `BitVector`. They share `Vector`'s storage layout and API surface, add the same bitwise-logical overloads as `LogicArray` / `BitArray`, and default the length-only shape argument to `N-1 downto 0`.

### Fulfils

* `RangedSequence`
* `std::ranges::sized_range`

### Constructors

`LogicVector` and `BitVector` use the same constructors as `Vector`, while being restricted to elements of `Logic` and `Bit`, respectively.

***Inferred direction from "length"-only arguments is `length-1 downto 0`, instead of `0 to length-1` like `Vector`.***

Notably, they can also be initialized with `LogicArray` and `BitArray` UDLs.

```c++
// Just the Range, default constructs the elements.
LogicVector a (Range{10, 20});
// Just the value, either an initializer list or sized range.
// Deduces a Range length-1 downto 0.
LogicVector b {'0'_l, '1'_l, 'X'_l, '-'_l};
LogicVector c {"UX01ZWLH-"_l};
std::vector<Logic> d {'0'_l, '1'_l, 'X'_l, '-'_l};
LogicVector e (c);
// Both the value and Range; data first, then Range.
LogicVector f (c, Range{100, 103});
LogicVector g ({'0'_l, '1'_l, 'X'_l, '-'_l}, Range{0, Direction::DOWNTO, -3});
LogicVector h ("01XZ"_l, Range{116, 119});
// runtime string; range inferred to length-1 downto 0
LogicVector i (std::string_view{"01XZ"});
BitVector   j (std::string_view{"0110"});
// runtime string with an explicit Range; length must match, throws on mismatch or bad char
LogicVector k (std::string_view{"01XZ"}, Range{116, 119});
```

The `std::string_view` ctor is `explicit`. Character validation matches `Logic(char)` / `Bit(char)`; unrecognized characters throw `std::invalid_argument`. With no `Range`, the `Range` is inferred to `length-1 downto 0`. With an explicit `Range`, length mismatch throws `std::invalid_argument`. Leftmost character maps to the leftmost storage slot.

### Ordering

Same rule as `LogicArray` / `BitArray`: `==` is strict on the full type. Cross-container (`LogicVector` vs `LogicArray`) and cross-element (`LogicVector` vs `BitVector`) comparisons are compile errors. Use `std::ranges::equal` when a value-only comparison is intended.

### Formatting and hashing

`LogicVector` and `BitVector` support `std::hash` and `std::formatter`, per the conventions in `conventions_api.md`. `std::hash` mixes the runtime `Range` in alongside the elements  --  same reasoning as `Vector`.

`std::formatter` produces `LogicVector[range]{"bitstring"}` / `BitVector[range]{"bitstring"}`, matching the `LogicArray` / `BitArray` shape. Takes no format spec.

## `RangedSequence<LogicType>` binary bitwise-logic operations

All `RangedSequence`s of `LogicType` support bitwise-logical operators with other `RangedSequence`s of `LogicType`. This includes `LogicArray`, `LogicVector`, `BitArray`, `BitVector`, and `StaticArraySlice` / `ArraySlice` over any of those.

The supported binary bitwise-logical operators:
* `&`  --  logical AND.
* `|`  --  logical OR.
* `^`  --  logical XOR.

The result type is determined by two rules from [`ranged_sequence_api.md`](ranged_sequence_api.md):

1. **Bound kind.** If both operands are `StaticRangedSequence` (owning `LogicArray` / `BitArray`, or `StaticArraySlice` over any parent), the result is compile-time-bounded (`LogicArray` / `BitArray`). If either operand is runtime-bounded (`LogicVector` / `BitVector`, or `ArraySlice` over any parent), the result is runtime-bounded (`LogicVector` / `BitVector`).
2. **Element type.** If either operand has element type `Logic`, the result element type is `Logic`. Otherwise (both `Bit`) the result element type is `Bit`.

Both operands must be the same length. When both are `StaticRangedSequence`s the mismatch is a `static_assert`; otherwise it is a runtime `std::invalid_argument`.

The result's `Range` is always normalized to `[N-1 downto 0]` regardless of the operands' ranges  --  the operands may have different (possibly incompatible) ranges of the same length, so there is no non-arbitrary way to inherit from one side, and the HDL convention for a freshly-computed bit-array is `downto 0`.

### Scalar broadcast

Either operand may be a `LogicType` scalar (`Logic` or `Bit`) instead of a `RangedSequence`. The scalar is broadcast against every element of the sequence, and the result follows the container-kind and element-type rules with the sequence operand as the container source:

```c++
auto a = "01XZ"_l & '1'_l;   // LogicArray[3 downto 0]{"01XZ"}  (AND with '1' is identity on Logic)
auto b = '0'_b | "1010"_b;   // BitArray[3 downto 0]{"1010"}
```

```c++
auto a = "0101"_l;
auto b = "0110"_l;
auto c = a & b;       // LogicArray[3 downto 0]{"0100"}
auto d = a | b;       // LogicArray[3 downto 0]{"0111"}
auto e = a ^ b;       // LogicArray[3 downto 0]{"0011"}
"0010"_b & "1111"_l;  // LogicArray[3 downto 0]{"0010"}
"1010"_b | "0101"_b;  // BitArray[3 downto 0]{"1111"}
```

Canonical result-type examples:

| LHS                            | RHS                            | Result       |
|--------------------------------|--------------------------------|--------------|
| `LogicArray`                   | `LogicArray`                   | `LogicArray` |
| `LogicArray`                   | `BitArray`                     | `LogicArray` |
| `LogicArray`                   | `LogicVector`                  | `LogicVector`|
| `BitArray`                     | `BitVector`                    | `BitVector`  |
| `StaticArraySlice<LogicArray>` | `StaticArraySlice<BitArray>`   | `LogicArray` |
| `ArraySlice<BitVector>`        | `ArraySlice<BitVector>`        | `BitVector`  |

## `RangedSequence<LogicType>` compound assignment

`&=`, `|=`, `^=` are defined on any mutating `RangedSequence<LogicType>` and take either a `RangedSequence<LogicType>` or a `LogicType` scalar as the RHS. Assignment is elementwise; when the RHS is a `RangedSequence` it must have the same length as the LHS. When the RHS is a scalar it is broadcast against every element.

- The `Logic` / `Bit` downcast rule from the scalar layer lifts: an LHS of `Bit` element type only accepts an RHS whose (scalar or elementwise) type is `Bit`; `logic_bit_arr &= bit_arr` is fine (elementwise `Bit -> Logic` upcast), but `bit_arr &= logic_arr` and `bit_arr &= 'X'_l` are ill-formed at compile time.
- Length match: when both sides are `StaticRangedSequence`, mismatch is a `static_assert`; otherwise it is a runtime `std::invalid_argument`.
- These work on mutating slices (`ArraySlice<...>` / `StaticArraySlice<...>` over a non-`const` parent) and write through to the parent's storage.

```c++
LogicVector a {"01XZ"_l};
a &= "0110"_l;                     // elementwise AND, a becomes "010X" (via Logic truth table)

BitArray<4> b {"0110"_b};
BitArray<4> c {"1010"_b};
b ^= c;                            // b becomes "1100"

b |= "0X10"_l;                     // ill-formed: Bit LHS cannot take Logic RHS

Vector<Bit> v {'0'_b, '1'_b, '0'_b, '1'_b};
v[{1, 2}] &= "10"_b;               // writes through slice into v: v is now "0001"

LogicVector d {"01XZ"_l};
d |= '1'_l;                        // broadcast: d is now "1111" ('1' dominates in Logic OR)
```

## `RangedSequence<LogicType>` unary `~`

Unary `~` returns an owning container. The `Range` of the result matches the operand's `Range`. The container kind is determined by the operand:

- Owning operands (`LogicArray`, `BitArray`, `LogicVector`, `BitVector`) return the same type.
- `StaticArraySlice` operands (compile-time-bounded slice) return the corresponding `Array`-family container: over `Logic` -> `LogicArray`, over `Bit` -> `BitArray`.
- `ArraySlice` operands (runtime-bounded slice) return the corresponding `Vector`-family container: over `Logic` -> `LogicVector`, over `Bit` -> `BitVector`.

```c++
auto a = ~"0101"_b;                   // BitArray[3 downto 0]{"1010"}
auto b = ~a[{3, 2}];                  // BitVector[3 downto 2]{"01"}   (runtime slice)
auto c = ~a.slice<Range{3, 2}>();     // BitArray[3 downto 2]{"01"}    (static slice)
```

`inplace_not(arr)` is the in-place counterpart (C++ has no `~=`), matching the scalar `inplace_not` in `logic_bit_api.md`. It complements every element in place; on a mutating slice it writes through to the parent. `constexpr noexcept`.

```c++
BitArray<4> a {"0110"_b};
inplace_not(a);                       // a is now "1001"

LogicVector v {"01XZ"_l};             // range 3 downto 0
inplace_not(v[{2, 1}]);               // ~v[2]='1'->'0', ~v[1]='X'->'X'; v is now "00XZ"
```

## `concat`

Free function that concatenates a variadic list of `LogicType` scalars and `RangedSequence<LogicType>` operands into a single owning container:

```c++
template <typename... Args>
    requires (sizeof...(Args) >= 1) && (... && (LogicType<Args> || RangedSequence<Args, Logic> || RangedSequence<Args, Bit>))
auto concat(Args const&... args);
```

- The first argument occupies the high bits (MSB); the last argument occupies the low bits (LSB). Within each `RangedSequence` operand, elements are taken in iteration order (`begin()` to `end()`) regardless of the operand's direction.
- Element type is `Logic` if any operand's element is `Logic`, else `Bit`.
- Result is a `LogicArray<N>` / `BitArray<N>` when every operand has a compile-time size (a scalar, or a `StaticRangedSequence`). Otherwise it is a `LogicVector` / `BitVector`. Result range is `N-1 downto 0`.

```c++
auto a = concat('1'_l, "01X"_b);       // LogicArray[3 downto 0]{"101X"}
auto b = concat("1010"_b, "0011"_b);   // BitArray[7 downto 0]{"10100011"}

LogicVector v {"01"_l};
auto c = concat('1'_l, v);             // LogicVector[2 downto 0]{"101"} (runtime because v is)
```

## Reduction and resolution free functions

Free functions defined on any `RangedSequence` of `Logic` / `Bit`:

- `resolve(arr, method)` / `resolve(arr)`  --  element-wise resolve of metavalues; see `ResolveMethod` in `conventions_api.md`. `resolve(arr)` uses the default `WEAK` method.
- `and_reduce(arr)`  --  logical AND across all elements.
- `or_reduce(arr)`  --  logical OR across all elements.
- `xor_reduce(arr)`  --  logical XOR across all elements.

### `resolve` return type

`resolve` returns a `std::optional` of the corresponding `Bit`-array container: `optional<BitArray<...>>` for a `LogicArray`, `optional<BitVector>` for a `LogicVector`, and so on. The optional is engaged only if *every* element resolved under the requested method; a single unresolvable element makes the whole result `nullopt`.

For `Bit`-element inputs the return type still uses `optional` for shape uniformity, but the optional is always engaged  --  `Bit` has no metavalues.

```c++
auto a = resolve("011L"_l);                       // engaged: BitArray[3 downto 0]{"0110"}
auto b = resolve("01XZ"_l);                       // nullopt: X, Z not resolvable under WEAK
auto c = resolve("01XZ"_l, ResolveMethod::ONES);  // engaged: BitArray[3 downto 0]{"0111"}
auto d = resolve("0110"_b);                       // engaged: BitArray[3 downto 0]{"0110"}
```

For per-element engagement (i.e. "which positions failed?"), iterate and call `Logic::resolve` on each element yourself; the container-level API is deliberately all-or-nothing.

### Reduction return type

Each reduction returns the element type of the input: `and_reduce(logic_arr) -> Logic`, `and_reduce(bit_arr) -> Bit`. AND/OR/XOR on `Bit` inputs stay in `Bit`; on `Logic` inputs the truth table may produce metavalues.

Empty inputs return the operator's identity  --  no throws:

- `and_reduce(empty)` -> `'1'`
- `or_reduce(empty)`  -> `'0'`
- `xor_reduce(empty)` -> `'0'`

## `constexpr` and `noexcept` contract

`LogicArray` / `BitArray` inherit the `Array<T>` contract in `array_vector_api.md`, and `LogicVector` / `BitVector` inherit the `Vector<T>` contract. `Logic` and `Bit` are `constexpr`-friendly structural types with `constexpr noexcept` copy/move, so every `Array<Logic>` / `Array<Bit>` operation the base contract makes conditional on `T` is `constexpr` here; the `noexcept`-ness of the elementwise operations tracks the corresponding `Logic` / `Bit` ops (the throwing ones in `logic_bit_api.md`).

Additional throw sites specific to this layer:

- The `std::string_view` ctors on `LogicArray` / `BitArray` / `LogicVector` / `BitVector` throw `std::invalid_argument` on unrecognized characters, and (for the static forms or the runtime forms given an explicit `Range`) on length mismatch.
- The `""_l` and `""_b` string-literal UDLs are `consteval` and validate at compile time; malformed literals are compile errors, not runtime throws.
- Bitwise operators (`&`, `|`, `^`) throw `std::invalid_argument` on runtime length mismatch. When both operands are `StaticRangedSequence`s the mismatch is a `static_assert`.
- Reduction free functions (`and_reduce`, `or_reduce`, `xor_reduce`) and `resolve` are `noexcept` on well-formed input; they don't validate anything the element type wouldn't already.

## Out of scope for this version

- Numeric semantics on `LogicArray`/`BitArray` (see `unsigned_signed_api.md` and `sfixed_ufixed_api.md` for the numeric families that share this bit-array surface).
- Total ordering on `LogicArray` / `BitArray` (bit-arrays alone have no numeric interpretation).
- User-extensible customization of the result-type dispatch matrix above.
