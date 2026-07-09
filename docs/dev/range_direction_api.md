# `Direction` and `Range` API Specification

This document specifies the API for the `Direction` enum and the `Range` integer-sequence type in coconext. It is a design spec.

`Range` describes the indexing scheme of HDL array-like objects (arrays of `Logic`, `Unsigned` values, etc.) and is the shared shape descriptor used across the coconext type family. `Direction` names its orientation.

The cross-cutting formatting, hashing, and construction-vs-conversion conventions are in `conventions_api.md`.

## Header layout

```
cpp/include/coconext/types/
  direction.hpp       --  Direction enum, to_string,
                          std::formatter specialization
  range.hpp           --  Range, its iterator, reverse(),
                          std::hash / std::formatter specializations
  count_iterator.hpp  --  CountIterator<value_type> that Range uses [internal]
  hash.hpp            --  hash_combine helper used by std::hash<Range> [internal]
```

`range.hpp` includes `direction.hpp`. Users include the umbrella `types.hpp`, or `range.hpp` for the shape-descriptor surface. All public symbols live in `coconext::types`.

## `Direction`

```c++
enum class Direction : bool {
    TO = 0,
    DOWNTO = 1,
};
```

`Direction` is an `enum class` with underlying type `bool`. Only the two named values are representable  --  the compiler will reject any attempt to construct a `Direction` from an out-of-range integer, so `Direction` never carries an invalid value.

- `TO`  --  ascending range.
- `DOWNTO`  --  descending range.

### Conversion

The only conversion is a one-way `to_string` for formatting purposes. Users construct `Direction::TO` or `Direction::DOWNTO` directly; there is no string-to-`Direction` parser in the public surface.

```c++
constexpr std::string_view to_string(Direction d) noexcept;   // "to" or "downto"
```

`to_string` returns a static string ("to" for `TO`, "downto" for `DOWNTO`); `noexcept`. It exists to feed `std::formatter<Direction>` and the `Range` formatter.

```c++
to_string(Direction::TO);      // "to"
to_string(Direction::DOWNTO);  // "downto"
```

### Ordering

Supports `==` and `!=`, as any typical enum does.

### Formatting and hashing

`std::formatter<Direction>` produces `Direction{to}` / `Direction{downto}`. Takes no format spec; any spec character throws `std::format_error` at parse time.

`std::hash<Direction>` uses the default `enum class` hash (over the `bool` underlying value). It exists so that `std::hash<Range>` can mix `direction` in via `detail::hash_combine`; `noexcept`.

## `Range`

`Range` is a lazily-evaluated integer sequence described with inclusive bounds on each side. Ranges may be described as descending (`4 downto 1`) or ascending (`-2 to 8`).

Ranges may be **null** if there are no values in the iteration. For example, `0 to -2` is null because there is no way to count from 0 upward and end at -2. Null ranges have length zero.

Ranges are typically used to describe the indexing scheme of array-like objects in HDLs, such as arrays of `Logic` or `Unsigned` values.

### Fulfils

* `std::ranges::random_access_range`
* `std::ranges::sized_range`

### Type members

```c++
struct Range {
    using value_type = int64_t;
    using iterator   = CountIterator<value_type>;

    value_type left      = 0;
    value_type right     = -1;
    Direction  direction = Direction::TO;
    // ...
};
```

- `value_type` is `int64_t`. This is the type of `left`, `right`, `operator[]` return, and iterator dereference.
- `iterator` is `CountIterator<int64_t>`  --  a random-access iterator over a counting sequence in the given direction. `Range` is not templated on iterator type.
- Default construction produces the canonical null range `Range{0, TO, -1}` with length 0.

The three fields are public and mutable. See [Fields and mutation](#fields-and-mutation).

### Construction

`Range` provides three converting constructors, all `constexpr`:

```c++
constexpr Range() noexcept;                                        // canonical null
constexpr explicit Range(size_t length);                           // 1-arg, throws on overflow
constexpr Range(value_type left, value_type right) noexcept;       // 2-arg, direction inferred
constexpr Range(value_type left, Direction, value_type right) noexcept;  // 3-arg
```

- **1-arg form**  --  `explicit`. The direction is `TO`; `left = 0`, `right = length - 1`. Throws `std::length_error` if `length` exceeds `numeric_limits<value_type>::max()`. Because it takes `size_t`, negative literals are ill-formed at the call site. Note that `LogicArray<N>` and `BitArray<N>` invert this default to `N-1 downto 0` to match HDL convention; see `logic_array_bit_array_api.md`.
- **2-arg form**  --  implicit. Direction is inferred: `DOWNTO` when `left > right`, `TO` otherwise. The 2-arg form therefore never produces a null range.
- **3-arg form**  --  implicit. `left`, `direction`, `right` are stored verbatim; this is the only path that can construct a null range or an invalid one (see [Invalid values](#invalid-values)).

```c++
auto a = Range{1, 8};                        // TO, length 8
a.length();  // 8
a.left       // 1
a.right      // 8
a.direction  // Direction::TO

auto b = Range{0, Direction::TO, -2};        // null range
b.length()   // 0

auto c = Range{1000};                        // TO, [0, 999]
c.length();   // 1000
c.left;       // 0
c.direction;  // Direction::TO
c.right;      // 999
```

### Invalid values

Two `Range` values span the full `value_type` domain and cannot be represented as a `size_t` length:

```
Range{numeric_limits<int64_t>::min, Direction::TO,     numeric_limits<int64_t>::max}
Range{numeric_limits<int64_t>::max, Direction::DOWNTO, numeric_limits<int64_t>::min}
```

These can be constructed via the 3-arg ctor (which is `noexcept` and does not check). Because the fields are public, they can also be mutated into after any construction.

Any operation on such a `Range` is **undefined behavior**. There is no runtime check anywhere in the `Range` API. `length()`, `operator[]`, iteration, and everything downstream trust their input; passing a full-domain span produces unspecified results (`length()` wraps to 0, `operator[]` returns a nonsense value, iteration doesn't terminate, etc., depending on implementation details that may change).

Don't do anything stupid.

### Conversion

None. `Range` is a structural value type, not a converter of anything else.

### Fields and mutation

`Range` exposes `left`, `direction`, `right` as public data members. This is a consequence of the NTTP requirement: `Range` must be a structural type so that `Array<T, Range{...}>` and the rest of the compile-time-bounded family can carry a `Range` as a non-type template argument, and structural types must expose their data members publicly. Mutability falls out of that  --  a structural class with private fields is not a thing C++ supports.

This is deliberately allowed. Downstream types (`Array`, `Vector`, etc.) capture the `Range` by value and protect their own storage against post-hoc mutation, so a runtime tweak of a caller's `Range` won't compromise their invariants. Range is the only type in the coconext family whose shape descriptor is exposed this way; everything downstream copies it in.

The one concrete hazard is mutating a `Range` into one of the two invalid full-domain spans described above; subsequent operations are undefined behavior (see [Invalid values](#invalid-values)).

```c++
Range r {-10, Direction::TO, 10};
r.left = 90;
r.length();   // 0
r.direction = Direction::DOWNTO;
r.length();   // 81
```

### `operator[]`

```c++
constexpr value_type operator[](size_t index) const;   // throws std::out_of_range
```

Returns the `index`-th value of the sequence, counting from `left` in the given direction. `Range{1, 8}[0]` is `1`, `Range{1, 8}[7]` is `8`. `Range{5, 3}[0]` is `5`, `Range{5, 3}[2]` is `3`.

Throws `std::out_of_range` if `index >= length()`. Behavior on a full-domain invalid `Range` is undefined (see [Invalid values](#invalid-values)).

O(1)  --  no iteration.

### Size

```c++
constexpr size_t length() const noexcept;
constexpr size_t size()   const noexcept;   // alias for length()
```

`length()` is the primary spelling. `size()` is the stdlib-shaped alias that satisfies `std::ranges::sized_range` and lets `std::size(r)` and `std::distance(begin, end)` work. Both are `constexpr noexcept` and O(1); they produce unspecified results on the two full-domain invalid values (see [Invalid values](#invalid-values)).

### Subsequence

Ranges have the concept of *subsequence*. A `Range` is a subsequence of another if all of its values appear (in order) in the parent.

The rule collapses cleanly by length:
- **length 0**: always a subsequence of any parent.
- **length 1**: the single value must exist in the parent; the child's direction is irrelevant.
- **length 2+**: the child's direction must match the parent's, and both endpoints must exist in the parent (interior values then follow by construction).

Examples:
* `[0 to 7]` is a subsequence of `[-100 to 100]`.
* `[0 to 7]` is not a subsequence of `[100 downto -100]`  --  wrong order.
* `[0 to 7]` is not a subsequence of `[8 to 100]`  --  no overlap.
* `[0 to 7]` is not a subsequence of `[4 to 9]`  --  0 to 3 aren't in 4 to 9.
* `[0 downto 10]` is a subsequence of `[-1 downto -4]`  --  the null range is the empty sequence `[]`, which is a subsequence of any other sequence.
* Any non-empty range is *not* a subsequence of a null parent  --  the single-element existence check fails.

```c++
constexpr bool is_subsequence_of(Range parent) const noexcept;
```

`constexpr`-evaluable and `noexcept`, so callers can `static_assert` it at compile time as well as use it as a runtime predicate.

```c++
Range r {0, 10};
Range q {3, 4};
q.is_subsequence_of(r);            // true
r.is_subsequence_of(Range{10, 0}); // false
```

### `reverse`

Free function:

```c++
constexpr Range reverse(Range const& r) noexcept;
```

Returns `Range{r.right, opposite_direction, r.left}`: the values are the same set in the reverse order.

### Ordering / equality

`Range` only supports `==` and `!=`, and equality is strict on all three fields (`left`, `right`, `direction`). `Range{5, 5}` (TO) and `Range{5, DOWNTO, 5}` compare `!=` even though they iterate to the same one-element sequence  --  the direction is part of the identity so that hashing and equality agree (see `conventions_api.md`).

Total ordering is not provided (no natural definition across arbitrary directions).

### Iteration

```c++
constexpr iterator begin()  const noexcept;
constexpr iterator end()    const noexcept;
constexpr iterator rbegin() const noexcept;
constexpr iterator rend()   const noexcept;
```

`Range` is a `std::ranges::random_access_range`, so it can be iterated using ranged for loops or by manually calling `begin()` / `end()`. Reverse iteration is available via `rbegin()` / `rend()`. All four accessors are `constexpr noexcept` and O(1); the iterator type is `CountIterator<int64_t>`.

```c++
for (auto a : Range{10, 20}) {
   std::cout << a << " ";
}
// 10 11 12 13 14 15 16 17 18 19 20
```

### Formatting and hashing

`Range` supports `std::hash` and `std::formatter`. `std::hash<Range>` mixes `left`, `right`, and `direction` via the shared `detail::hash_combine`; `noexcept`. `std::formatter<Range>` produces `[left dir right]` output (e.g. `[3 downto 0]`), takes no format spec, and throws `std::format_error` on any non-empty spec.

## `constexpr` and `noexcept` contract

Every `Direction` and `Range` operation is marked `constexpr`, and every operation except the listed throw sites is `noexcept`:

- `Range(size_t length)` throws `std::length_error` on overflow.
- `Range::operator[]` throws `std::out_of_range` on out-of-bounds index.
- The two `std::formatter::parse` overloads throw `std::format_error` on any non-empty spec.

`Range::length()` itself is `noexcept`; it produces unspecified results on the two full-domain invalid values (see [Invalid values](#invalid-values)).

Everything else  --  copy, comparison, iteration, `is_subsequence_of`, `reverse`, `to_string(Direction)`, `std::hash`  --  is `constexpr noexcept`. `Range` is therefore usable as an NTTP in `constexpr` contexts (subject to the standard's NTTP structural-type rules), which is how `Array<T, Range{...}>` and the compile-time-bounded family carry their shape.
