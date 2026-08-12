# `detail::Bits<W>` API Specification

This document specifies the storage primitive `coconext::types::detail::Bits<W>`. `Bits<W>` is the shared backing for `BitArray<R>`, `Unsigned<R>`, `Signed<R>`, `Ufixed<R>`, `Sfixed<R>`, and future `Float<R>`. It lives in `detail::` and is not a user-facing extension point; the closed set of user types is its only consumer.

## Scope

`Bits<W>` is a fixed-width container of `W` bits, sign-agnostic in its representation and in those operations whose result bit-pattern does not depend on the operand interpretation. Operations whose result does depend on sign interpretation are exposed under sign-named forms (`add_signed`/`add_unsigned`, `compare_signed`/`compare_unsigned`, `zero_extend`/`sign_extend`, etc.). The user type names the sign at the call site.

`Bits<W>` is closed: it ships with exactly the operations the six user types need. Every op in this spec has a named consumer among those types. Ops with no consumer are not provided.

## Storage

### Layout

Storage selection is a function of `W`:

| `W` range | Storage |
|-----------|---------|
| `W == 0` | `EmptyStorage` (empty tag type, no data members) |
| `1 <= W <= 8` | `uint8_t` |
| `9 <= W <= 16` | `uint16_t` |
| `17 <= W <= 32` | `uint32_t` |
| `33 <= W <= 64` | `uint64_t` |
| `65 <= W <= 128` | `__uint128_t` (GCC/Clang only; conditional on `__SIZEOF_INT128__`) |
| `W >= 129` | `std::array<uint64_t, ceil(W / 64)>` owned directly by `Bits<W>` |

The native tier is the smallest unsigned native that holds `W` bits. This makes `Bits<8>` exactly one byte and lets arrays of `Unsigned<8>` auto-vectorize.

The storage member is declared `[[no_unique_address]]` so `Bits<0>` collapses when composed into an enclosing type.

On the wide tier, `Bits<W>` directly owns a `std::array<uint64_t, ceil(W / 64)>`
(LSB-first word order). Word-array kernels ported from LLVM's APInt operate on
that storage. The same kernels implement bit access, width changes, saturation,
formatting, arithmetic, and division for `DynBits`; the owning types only
select storage, account for result widths, and provide division scratch.
`Bits<W>` supplies fixed-size stack scratch while `DynBits` allocates scratch
at runtime. There is no separate owning big-integer type.

### Default value

Zero (all bits zero) for every `W`, including `W == 0`.

## Constexpr contract

All members and free-function operations are marked `constexpr`. The SBO path (`W <= 64`, or `W <= 128` where `__int128_t` is available) is fully constexpr-callable.

The wide path is fully constexpr: the kernels are loops over `std::array<uint64_t, N>`, proven by a compile-time division-identity test.

Operations that throw (division/modulus by zero) only become non-constant-expression on the specific call that fires the throw. This is the standard "constant-source overflow becomes a compile error" mechanism.

## `noexcept` policy

Uniformly `noexcept` except for the ops explicitly listed as throwing (division/remainder/modulus on a zero divisor, and the string constructor on a literal too wide for the type). No path allocates, so the SBO and wide surfaces are API-identical.

Full noexcept table:

| Category | noexcept |
|----------|----------|
| Default ctor, `Bits(IntType)` | yes |
| `get`, `set`, `operator[]`, iteration, `popcount`/`any`/`none`/`all` | yes |
| `raw()`, comparisons, bitwise, shifts, `reverse`, `concat` | yes |
| `zero_extend`, `sign_extend`, `truncate`, `saturate_*` | yes |
| Growing add/sub/mul, `negate_signed`, `abs_signed` | yes |
| `div_*`, `rem_*`, `mod_signed` | no (throws `std::domain_error` on /0) |
| Slicing (`slice(...)`) | yes |

## Invariants

- **Canonical storage**: bits `[W, storage_bit_width)` are always zero. `raw()` returns storage in this canonical form. Operations that could dirty high bits mask before storing. Any callsite that mutates through `raw()` is responsible for re-establishing this invariant before the object is observed externally.
- **Bit ordering**: `get(0)` is the LSB. On wide storage, word 0 of the underlying `std::array<uint64_t, N>` holds bits `[0, 64)`, word 1 holds `[64, 128)`, etc. Platform byte-order does not affect this — the layout is defined against the abstract bit index, not against memory endianness.
- **Moved-from state**: valid but unspecified. No tier allocates, so a move is a copy in practice; the spec does not promise it.

## Type-level introspection

- `Bits<W>::width` — the width `W` as a `size_t`.
- `Bits<W>::IntType` — the chosen storage type (see table). Useful for `static_assert`s in `detail::` code and for the SBO/wide branch in generic code.
- `Bits<W>::is_wide` — `false` iff the storage is a native integer (including `__uint128_t`); `true` iff it is a directly owned word array.
- `Bits<W>::RawType` — what `raw()` hands back: `IntType` on the native tier, a non-owning `WordConstSpan` on the wide tier.

## Construction

```cpp
constexpr Bits() noexcept;                        // value-init to 0
constexpr explicit Bits(IntType val) noexcept;    // raw storage value
```

Both ctors are `explicit`. Cross-width construction is provided by the width-changing member templates (`zero_extend`/`sign_extend`/`truncate`/`saturate_*`), which return the destination width directly.

## Raw access

```cpp
constexpr IntType&       raw()       noexcept;
constexpr IntType const& raw() const noexcept;
```

Returns the canonical storage value. The mutable overload allows other `detail::` code to twiddle bits in place (e.g. during arithmetic implementation); such callers must re-establish the canonical invariant before the object is observed externally.

## Bit-level access

```cpp
constexpr bool get(size_t i) const noexcept;
constexpr void set(size_t i, bool value) noexcept;
constexpr size_t popcount() const noexcept;
constexpr bool any() const noexcept;    // popcount() > 0
constexpr bool none() const noexcept;   // popcount() == 0
constexpr bool all() const noexcept;    // popcount() == W
```

Indexing is LSB-relative: `get(0)` is bit `2^0`, `get(W-1)` is bit `2^(W-1)`. User types map their `Range`-direction-aware indexing onto this convention.

`get(i)` / `set(i, .)` with `i >= W` are undefined behavior. Bounds checks live at the user-type layer; `Bits<W>` trusts its callers.

## Indexing and iteration

`Bits<W>` supports mutable per-bit access through a `BitProxy` and offers random-access iteration in LSB-to-MSB order. User-type iteration wraps this and remaps to the `Range` direction.

### `BitProxy`

```cpp
class BitProxy {
public:
    constexpr operator bool() const noexcept;
    constexpr BitProxy& operator=(bool v) noexcept;
    constexpr BitProxy& operator=(BitProxy const&) noexcept;
    constexpr BitProxy& flip() noexcept;
    constexpr BitProxy& operator&=(bool v) noexcept;
    constexpr BitProxy& operator|=(bool v) noexcept;
    constexpr BitProxy& operator^=(bool v) noexcept;
};
```

Holds a `Bits<W>*` and a bit index; reads and writes forward to `get`/`set` on the parent.

### Subscripting

```cpp
constexpr BitProxy operator[](size_t i)       noexcept;
constexpr bool     operator[](size_t i) const noexcept;
```

### Iterators

```cpp
class iterator;         // dereferences to BitProxy
class const_iterator;   // dereferences to bool

constexpr iterator                begin()  noexcept;
constexpr iterator                end()    noexcept;
constexpr const_iterator          begin()  const noexcept;
constexpr const_iterator          end()    const noexcept;
constexpr const_iterator          cbegin() const noexcept;
constexpr const_iterator          cend()   const noexcept;
constexpr reverse_iterator        rbegin() noexcept;
constexpr reverse_iterator        rend()   noexcept;
constexpr const_reverse_iterator  rbegin() const noexcept;
constexpr const_reverse_iterator  rend()   const noexcept;
```

`const_iterator` satisfies `std::random_access_iterator`. `iterator` satisfies random-access traversal and `std::indirectly_writable` but not `LegacyRandomAccessIterator` (proxy reference; same trade `std::vector<bool>` makes).

## Slicing

Two view types, mirroring `ArraySlice` / `StaticArraySlice` from the type-system spec. Both are templated on `Parent = Bits<W>` or `Bits<W> const`; the const-ness flows through the parameter to gate mutating members.

```cpp
template <typename Parent>
class BitsSlice;                          // runtime lo/len

template <typename Parent, size_t Lo, size_t Len>
class StaticBitsSlice;                    // compile-time Lo/Len
```

Each holds a `Parent*` + LSB offset + length. Both act as read/write windows over packed bits — same shape as `ArraySlice<BitArray>`:

- Read: `operator[](i) -> bool` (const parent) / `BitProxy` (mutable parent); iteration (const → `bool`, mutable → `BitProxy`); `size()`, `get(i)`, `popcount`, `any`, `none`, `all`
- Write (mutable-parent only): `operator[]` returning `BitProxy`; `set(i, bool)`; mutating iterators
- Bitwise ops bind at the user-type layer through the `LogicArrayType` constraint by virtue of being `RangedSequence<Bit>`-shaped; `BitsSlice`/`StaticBitsSlice` do not declare `&|^~` themselves.
- No arithmetic, no shifts, no comparison against `Bits<Wm>`. Users who want numeric ops recover a numeric type via `as<...>(slice)` at the user-type layer.

### `Bits<W>` slice members

```cpp
constexpr BitsSlice<Bits<W>>       slice(size_t lo, size_t len)       noexcept;
constexpr BitsSlice<Bits<W> const> slice(size_t lo, size_t len) const noexcept;

template <size_t Lo, size_t Len>
    requires (Lo + Len <= W)
constexpr StaticBitsSlice<Bits<W>, Lo, Len>       slice()       noexcept;
template <size_t Lo, size_t Len>
    requires (Lo + Len <= W)
constexpr StaticBitsSlice<Bits<W> const, Lo, Len> slice() const noexcept;
```

Runtime `slice` trusts its arguments (bounds checks are the user-type layer's job); static `slice<Lo, Len>` bounds-checks at compile time via the `requires` clause.

### Copy out to owned Bits

```cpp
template <size_t Wm> constexpr explicit Bits<Wm>(BitsSlice<...> const&);
template <size_t Wm> constexpr explicit Bits<Wm>(StaticBitsSlice<..., Lo, Len> const&);
```

The runtime form throws `std::invalid_argument` on `Wm != slice.length()`. The static form `static_assert`s on `Wm != Len`.

## Equality

```cpp
constexpr bool operator==(Bits<W> const&) const noexcept;
constexpr bool operator!=(Bits<W> const&) const noexcept;
```

Bit-pattern equality, sign-agnostic. Backs `BitArray<R>::operator==` and the user types' equality (which combine it with per-type strictness).

## Ordering

Ordering is sign-sensitive, exposed as three-way compare in sign-named forms:

```cpp
constexpr std::strong_ordering compare_unsigned(Bits<W> const&) const noexcept;
constexpr std::strong_ordering compare_signed  (Bits<W> const&) const noexcept;
```

There is no `operator<=>` on `Bits<W>`. User-type `operator<=>` dispatches to one of these two depending on the type's interpretation.

## Bitwise

```cpp
constexpr Bits operator&(Bits<W> const&) const noexcept;
constexpr Bits operator|(Bits<W> const&) const noexcept;
constexpr Bits operator^(Bits<W> const&) const noexcept;
constexpr Bits operator~() const noexcept;
```

Sign-agnostic. Result is canonicalized (top bits masked).

## Shifts

Shifts are destructive (same-width result), matching the user-type spec's "shifts have a single universally-recognized HDL meaning."

```cpp
constexpr Bits shl(size_t amount) const noexcept;   // logical shift left
constexpr Bits srl(size_t amount) const noexcept;   // logical shift right
constexpr Bits sra(size_t amount) const noexcept;   // arithmetic shift right
```

- `amount == 0`: identity.
- `amount >= W`: collapses to 0 for `shl`/`srl`; sign-extends (all 0 or all 1) for `sra`.
- Amount is `size_t`; negative amounts are the user type's responsibility to reject.

There is no `operator<<` / `operator>>` on `Bits<W>` itself, for symmetry with the ordering decision: shift meaning is sign-named at the storage layer.

## Width-changing operations

Member templates producing a `Bits<Wm>`. These are the entry points for the user types' implicit widening, `resize`, and `as` paths, and are the only cross-width construction path (no cross-width converting ctor).

```cpp
template <size_t Wm> requires (Wm >= W)
constexpr Bits<Wm> zero_extend() const noexcept;

template <size_t Wm> requires (Wm >= W)
constexpr Bits<Wm> sign_extend() const noexcept;

template <size_t Wm> requires (Wm <= W)
constexpr Bits<Wm> truncate() const noexcept;

template <size_t Wm>
constexpr Bits<Wm> saturate_unsigned() const noexcept;

template <size_t Wm>
constexpr Bits<Wm> saturate_signed() const noexcept;
```

- `zero_extend<Wm>`: fill high bits with 0. `Wm == W` is identity.
- `sign_extend<Wm>`: fill high bits with a copy of bit `W-1`. `Wm == W` is identity. On `Bits<0>` source, fills with 0 (no bit W-1 exists).
- `truncate<Wm>`: drop bits `[Wm, W)`. `Wm == W` is identity.
- `saturate_unsigned<Wm>`: interpret source as unsigned. `Wm >= W` degenerates to `zero_extend<Wm>()`. `Wm < W` clamps to `[0, 2^Wm - 1]`.
- `saturate_signed<Wm>`: interpret source as signed. `Wm >= W` degenerates to `sign_extend<Wm>()`. `Wm < W` clamps to `[-2^(Wm-1), 2^(Wm-1) - 1]`.

`Bits<W>` is radix-unaware. Rounding for Sfixed/Ufixed fractional-side narrowing lives at the user-type layer, which applies the `round_mode` before calling these bit-level primitives.

## Growing arithmetic

Free functions in `coconext::types::detail`. The cross-width result type is expressed in the signature directly; ADL works cleanly since `Bits<W>` lives in `detail::` (matches the `concat` shape).

```cpp
// Additive: result grows by 1 to hold the carry-out / new sign bit
template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> add_unsigned(Bits<Wa> const&, Bits<Wb> const&) noexcept;
template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> add_signed  (Bits<Wa> const&, Bits<Wb> const&) noexcept;
template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> sub_unsigned(Bits<Wa> const&, Bits<Wb> const&) noexcept;
template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> sub_signed  (Bits<Wa> const&, Bits<Wb> const&) noexcept;

// Multiplicative: sum widths (holds any product without loss)
template <size_t Wa, size_t Wb>
constexpr Bits<Wa + Wb> mul_unsigned(Bits<Wa> const&, Bits<Wb> const&) noexcept;
template <size_t Wa, size_t Wb>
constexpr Bits<Wa + Wb> mul_signed  (Bits<Wa> const&, Bits<Wb> const&) noexcept;

// Division: quotient grows by 1 to hold signed_min / -1 losslessly
template <size_t Wa, size_t Wb>
constexpr Bits<Wa + 1> div_unsigned(Bits<Wa> const&, Bits<Wb> const&);
template <size_t Wa, size_t Wb>
constexpr Bits<Wa + 1> div_signed  (Bits<Wa> const&, Bits<Wb> const&);

// Remainder / modulo: bounded by divisor
template <size_t Wa, size_t Wb>
constexpr Bits<Wb> rem_unsigned(Bits<Wa> const&, Bits<Wb> const&);
template <size_t Wa, size_t Wb>
constexpr Bits<Wb> rem_signed  (Bits<Wa> const&, Bits<Wb> const&);
template <size_t Wa, size_t Wb>
constexpr Bits<Wb> mod_signed  (Bits<Wa> const&, Bits<Wb> const&);

// Unary (grow to hold the sign bit)
template <size_t W>
constexpr Bits<W + 1> negate_signed(Bits<W> const&) noexcept;
template <size_t W>
constexpr Bits<W + 1> abs_signed(Bits<W> const&) noexcept;
```

Notes:

- `add`/`sub` are sign-named because the grown top bit is filled differently: carry-out for unsigned, sign bit for signed.
- `rem_signed` is C-style (result sign follows dividend); `mod_signed` is VHDL/Python (result sign follows divisor). User-type `%` dispatches to `rem_signed`; user-type `mod` free function dispatches to `mod_signed`. Unsigned operands have no `mod` variant — `rem_unsigned` covers it.
- `div_*`/`rem_*`/`mod_signed` throw `std::domain_error` on zero divisor. When either operand is a constant expression and the divisor is zero, the throw makes the call a non-constant-expression → compile error at that call site.
- Result widths match the user-type spec's [Width growth table](unsigned_signed_api.md#width-growth) and the [Sfixed/Ufixed table](sfixed_ufixed_api.md#width-growth).

## Reverse

```cpp
constexpr Bits<W> reverse() const noexcept;
```

Bit-order reverse: `result.get(i) == this->get(W - 1 - i)`. Consumers: the user-type `reverse(x)` on all six types. For Sfixed/Ufixed this preserves numeric value across a `Range`-direction flip; for Unsigned/Signed/BitArray it reverses the bits.

## Concat

Variadic free function in `coconext::types::detail`:

```cpp
template <size_t... Ws>
constexpr Bits<(Ws + ...)> concat(Bits<Ws> const&... parts) noexcept;
```

Leftmost operand occupies the high bits, rightmost occupies the low bits. Empty pack yields `Bits<0>{}`. `Bits<0>` operands act as identity.

Single allocation for the wide path; no intermediate `Bits<partial-sum>` values.

## Hash

```cpp
namespace std {
    template <size_t W>
    struct hash<coconext::types::detail::Bits<W>> {
        constexpr size_t operator()(coconext::types::detail::Bits<W> const&) const noexcept;
    };
}
```

Hashes the canonical bit pattern via `raw()`. For SBO storage this is `std::hash<IntType>{}(b.raw())`; for wide storage, hash the underlying `std::array<uint64_t, N>` word-by-word.

Consumers (`std::hash<BitArray<R>>`, `std::hash<Unsigned<R>>`, `std::hash<Signed<R>>`, `std::hash<Sfixed<R>>`, `std::hash<Ufixed<R>>`) seed with their own per-type tag and mix in this hash to obtain the strict per-(type, range) hash equivalence class.

## `Bits<0>` corners

Storage is `EmptyStorage`, an empty tag type with no data members. The storage member on `Bits<0>` is declared `[[no_unique_address]]` so composition with `[[no_unique_address]]` at the user-type layer collapses it away.

Op-by-op behavior (falls out of the invariants; listed so consumers don't re-derive):

- `Bits<0>{} == Bits<0>{}` → `true`.
- `compare_unsigned` / `compare_signed` → `std::strong_ordering::equal`.
- `operator& | ^ ~` → `Bits<0>{}`.
- `shl(_)` / `srl(_)` / `sra(_)` → `Bits<0>{}`.
- `reverse()` → `Bits<0>{}`.
- `zero_extend<Wm>()` / `sign_extend<Wm>()` from `Bits<0>` → `Bits<Wm>{0}` (`sign_extend` fills with 0 — no bit W-1 exists).
- `truncate<0>()` from any `Bits` → `Bits<0>{}`.
- `saturate_unsigned<0>` / `saturate_signed<0>` from any `Bits` → `Bits<0>{}`.
- `add_unsigned(Bits<0>, Bits<0>)` / `add_signed(...)` → `Bits<1>{0}`.
- `mul_unsigned(Bits<0>, Bits<Wb>)` → `Bits<Wb>{0}` (`Wa + Wb == Wb`).
- `div_*(Bits<0>, Bits<Wb>)` — throws iff divisor is zero, else `Bits<1>{0}` (unsigned) / same (signed).
- `div_*(x, Bits<0>)` — throws (divisor is zero).
- `rem_*(Bits<0>, Bits<Wb>)` / `mod_signed(...)` — throws iff divisor is zero, else `Bits<Wb>{0}`.
- `negate_signed(Bits<0>{})` / `abs_signed(Bits<0>{})` → `Bits<1>{0}`.
- `concat(...)` with a `Bits<0>{}` operand → identity on the other operands. Empty pack → `Bits<0>{}`.
- `slice(0, 0)` / `slice<0, 0>()` — returns an empty slice.
- Iterators: `begin() == end()`. `operator[]` unreachable (any index is out of range).
- `popcount / any / none / all` → `0 / false / true / true`.

## What `Bits<W>` does not provide

The following are user-type concerns and are deliberately absent from `Bits<W>`:

- **No conversion to / from native integer**. `Unsigned<R>` / `Signed<R>` / `Sfixed<R>` / `Ufixed<R>` provide their own `explicit operator T()` for native integers.
- **No `is_negative` / sign accessor**. Sign is a property of the interpretation, not the storage; the user type that knows its interpretation derives this from `raw()` or `get(W - 1)`.
- **No named factories** (`all_zeros`, `all_ones`, `signed_min`, etc.). Callers that need those spell them directly.
- **No public same-width `+ - *` or `udiv` / `sdiv` / `umod` / `smod`**. Every path a user type walks uses either the growing free functions or `resize + as` composition, so a same-width wrapping op has no consumer on the public surface. The primitives still exist privately, since the growing forms are built from them; `detail::same_width` is the only way in, and only the growing functions and the division-kernel tests use it.
- **No compound assignment on `Bits<W>`** (`&= |= ^= += -= *=`). The non-compound forms cover every internal call site.
- **No `slice<Hi, Lo>()` returning an owned narrower `Bits`**. The slice views are the primitive; copy-out is a `Bits<Wm>` ctor from a slice.
- **No rotate**. No consumer.
- **No cross-width comparison / cross-width converting ctor**. User types widen through `zero_extend`/`sign_extend` first.

### Where the implementation diverges from this document

The following are specified above but **not implemented**, and no consumer needs them yet: slicing (`BitsSlice` / `StaticBitsSlice` and the `slice()` members), `concat`, `reverse()`, `std::hash<Bits<W>>`, `any` / `none` / `all`, and the `compare_unsigned` / `compare_signed` three-way forms (the named `ult` / `slt` families are what exists).

Conversely, `Bits<W>` **does** provide several things this document lists as absent, because they turned out to have consumers: `Bits(std::string_view)` at every width, the `to_binary_string` / `to_decimal_string` / `to_hexadecimal_string` / `to_octal_string` family (used by the `Unsigned` / `Signed` formatters), `operator<<` / `srl` / `sra`, `count_leading_zeros` / `count_trailing_zeros` (used by `index_of` / `rindex_of` in `bit_array.hpp`), and `get_word`-style word access on the wide tier via the `raw()` view.

## Friend access

`Bits<W>` is a `detail::` type. The intended consumers are:

- `coconext::types::BitArray<R>`
- `coconext::types::Unsigned<R>`
- `coconext::types::Signed<R>`
- `coconext::types::Ufixed<R>`
- `coconext::types::Sfixed<R>`
- `coconext::types::Float<R>` (future)

These reach into `Bits<W>` directly. Other `detail::` code (`concat`, the `resize` machinery, the growing-arithmetic free functions) also uses `Bits<W>` directly. The interface above is intentionally minimal: it supports exactly what those callers need.
