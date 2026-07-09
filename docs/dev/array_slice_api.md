# `ArraySlice` and `StaticArraySlice` API Specification

This document specifies the API for the non-owning slice views over `Array` and `Vector`: the runtime-bounded `ArraySlice` and the compile-time-bounded `StaticArraySlice`. It is a design spec.

Owning containers are specified in `array_vector_api.md`. The cross-cutting formatting, hashing, construction-vs-conversion, and static-vs-runtime-bounded conventions live in `conventions_api.md`. `Range` and `Direction` are in `range_direction_api.md`. `RangedSequence` and the cross-type binary-operation rules are in `ranged_sequence_api.md`.

## `ArraySlice`

A non-owning view of an `Array` or `Vector`. Slices carry a `Range` that is a subsequence of the parent object's `Range` and refer to the parent's data at those locations. They behave like `Array` and `Vector` on the read side, and additionally support write-through assignment to the parent's storage.

`ArraySlice<ArrayT>` is templated on the parent container type. `ArrayT`'s constness controls whether the slice can mutate elements: `ArraySlice<Vector<int>>` is a mutating view, `ArraySlice<Vector<int> const>` is a read-only view. `const ArraySlice<ArrayT>` fixes the pointer and range but element mutability still follows `ArrayT`.

### Fulfils

* `RangedSequence`
* `std::ranges::sized_range` (via random-access iteration)

### Construction

Not directly constructible by the user. Obtained by slicing an `Array` or `Vector` via `operator[](Range)` (and the C++23 multi-arg overloads).

### Assignment (write-through)

When the slice is over a non-`const` parent, it exposes assignment overloads that copy the RHS into the parent's storage element-by-element. These do not rebind the slice; they write through.

```c++
constexpr ArraySlice const& operator=(ArraySlice const& other) const
    requires(!std::is_const_v<ArrayT>);

template <sized_input_range R>
    requires(!std::is_const_v<ArrayT>) && std::convertible_to<range_value_t<R>, value_type>
constexpr ArraySlice const& operator=(R const& obj) const;

template <typename T>
    requires(!std::is_const_v<ArrayT>) && std::convertible_to<T, value_type>
constexpr ArraySlice const& operator=(std::initializer_list<T> init) const;
```

All three overloads throw `std::invalid_argument` on length mismatch. The RHS may be another slice, any `sized_range` (including foreign `std::vector` etc.), or a brace-enclosed initializer list.

Note the `const&` return and the trailing `const` qualifier: the slice itself is a reference-like handle that doesn't change; only the parent's data does.

```c++
Vector<int> v {1, 2, 3, 4, 5};
v[{1, 3}] = {10, 20, 30};      // writes elements 1..3 of v
v[{1, 3}] = v[{2, 4}];         // copies elements 2..4 into elements 1..3
```

### Conversion

None.

### Ordering

No `operator==` is defined on `ArraySlice`. Slice equality doesn't have a natural meaning: two slices could point at overlapping but non-identical storage, or at storage of different parents whose values happen to match.

Use `std::ranges::equal` to compare a slice element-wise against another `std::ranges::range` (an owning container, another slice, or a foreign `std::vector`).

### Hashing

Not hashable. Slices are non-owning; storing one in a `std::unordered_map` is a dangling-hash hazard.

### Indexing and slicing

Runtime indexing uses `operator[](Range::value_type)` and returns a reference to the parent's element. Runtime sub-slicing uses `operator[](Range)` and returns a fresh `ArraySlice<ArrayT>`. Under C++23, the multi-argument subscript forms `s[l, r]` and `s[l, dir, r]` are sugar for the corresponding `Range` construction.

Compile-time sub-slicing uses `slice<Range R>()` and returns a `StaticArraySlice<ArrayT, R>`. `ArraySlice` supports this despite being runtime-bounded  --  the runtime `is_subsequence_of` check fires at the call site, and afterwards further static slicing/indexing off the returned `StaticArraySlice` is compile-time-checked against its bounds.

Compile-time positional access uses `index<Range::value_type I>()`.

```c++
Vector<int> v {10, 20, 30, 40, 50};
auto s = v[{1, 3}];             // ArraySlice[1 to 3]{20, 30, 40}
s[2];                           // 30 (indexed in the slice's Range)
s[{1, 2}];                      // ArraySlice[1 to 2]{20, 30}
s.slice<Range{2, 3}>();         // StaticArraySlice[2 to 3]{30, 40}
s.index<2>();                   // 30
```

Element access bounds-checks against the slice's `Range`, not the owner's. Out-of-slice indices throw `std::out_of_range` even if they're in the owner's range.

### Iteration, `index_of` / `rindex_of`, `std::find`

Random-access iteration over the slice's range. Free-function `index_of(slice, value)` / `rindex_of(slice, value)` from `ranged_sequence_api.md` return the HDL coordinate within the slice's range (not the parent's), or `nullopt`.

### Formatting

`ArraySlice` supports `std::formatter`, per the conventions in `conventions_api.md`. `std::formatter<ArraySlice>` produces `ArraySlice[range]{elem, elem, ...}`, e.g. `ArraySlice[1 to 3]{20, 30, 40}`. The parent container type is not included in the header; a slice prints as its own view over its own range. The specialization is available only when the element type is `Formattable`. Takes no format spec; any spec character throws `std::format_error` at parse time.

Slices whose element type is `Logic` or `Bit` get a specialized output that mirrors the owning bit-array containers, with the element type folded into the type name: `LogicArraySlice[3 downto 2]{"01"}`, `BitArraySlice[1 to 2]{"10"}`. Value form matches `LogicArray` / `BitArray` (bit-string, round-trippable via the `std::string_view` ctor after collecting into an owning container).

## `StaticArraySlice`

Conceptually the same thing as `ArraySlice`, but with compile-time bounds. The difference is a `static_range` static member variable (used by the `StaticRangedSequence` concept and for constant folding) alongside the matching `range()` member.

`StaticArraySlice<ArrayT, R>` is templated on both the parent container type and the compile-time `Range`. Constness rules are identical to `ArraySlice`.

### Fulfils

* `StaticRangedSequence`
* `RangedSequence`
* `std::ranges::sized_range`

### Construction

Not directly constructible by the user. Obtained via the compile-time `slice<Range>()` method on an owning container or on another slice. The parent's `is_subsequence_of` check is `static_assert`ed at compile time when the parent has a static range; when it doesn't (`Vector`, `ArraySlice`), the check is a runtime throw at the site of `slice<R>()`.

### Assignment (write-through)

Same shape as `ArraySlice`'s three assignment overloads. When both source and destination are `StaticRangedSequence`s, the length mismatch is a `static_assert` rather than a runtime throw.

### Conversion, ordering, hashing

Same as `ArraySlice`: no conversion, no `operator==`, not hashable.

### Indexing and sub-slicing

Same shape as `ArraySlice`, with the following upgrades:

- `slice<R2>()`: the `R2.is_subsequence_of(static_range)` check is a `static_assert` at compile time. Runtime `operator[](Range)` still exists and remains a runtime check.
- `index<I>()`: the in-range check is a `static_assert` at compile time.

Runtime `operator[](Range::value_type)` still throws `std::out_of_range` on an out-of-slice index (the index value isn't known at compile time in general).

### Iteration, `index_of` / `rindex_of`, `std::find`, formatting

Identical to `ArraySlice`. When the parent is also a `StaticRangedSequence`, `StaticArraySlice::begin()` folds the offset from parent to slice into a compile-time constant.

`std::formatter<StaticArraySlice>` produces `StaticArraySlice[range]{elem, elem, ...}`, e.g. `StaticArraySlice[6 to 7]{7, 8}`. Same rules as `ArraySlice`: parent type omitted, requires a `Formattable` element type, no format spec.

Same `Logic` / `Bit` specialization applies, with the element type folded into the type name: `LogicStaticArraySlice[3 downto 2]{"01"}`, `BitStaticArraySlice[1 to 2]{"10"}`.

## `constexpr` and `noexcept` contract

Slice construction, copy, move, and destruction are `constexpr noexcept`  --  slices are pointer-and-`Range` handles that don't touch element storage on their own.

Throw sites:

- `operator[](Range::value_type)` throws `std::out_of_range` on an out-of-slice coordinate.
- `operator[](Range)` (runtime sub-slice) throws `std::invalid_argument` if the argument is not a subsequence of the slice's `Range`.
- `slice<Range>()` on `ArraySlice` throws `std::invalid_argument` if the compile-time `Range` isn't a subsequence of the (runtime) parent range; on `StaticArraySlice` this becomes a `static_assert`.
- Assignment overloads throw `std::invalid_argument` on runtime length mismatch. When both source and destination are `StaticRangedSequence`s, the mismatch is a `static_assert`.
- `std::formatter::parse` throws `std::format_error` on any non-empty spec.

`noexcept`-ness of element access, iteration, and assignment tracks the underlying element type's operations (dereference, copy-assign). Nothing that touches elements is unconditionally `noexcept`.

## Out of scope for this version

- User-constructible slice views (by design; slices come from their parent container).
- Hashing on slice types  --  hashability is provided only for owning collections (see `conventions_api.md`).
- `operator==` on slice types  --  ambiguous semantics for a non-owning view.
