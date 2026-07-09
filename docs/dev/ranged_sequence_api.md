# `RangedSequence` and `StaticRangedSequence` API Specification

This document specifies the `RangedSequence` and `StaticRangedSequence` concepts, and the cross-type binary-operation rules that follow from them. It is a design spec.

`RangedSequence` is the shared shape concept for every coconext container that indexes by a `Range`  --  `Array`, `Vector`, `ArraySlice`, `StaticArraySlice`, `LogicArray`, `BitArray`, `LogicVector`, `BitVector`, `Unsigned`, `Signed`, `Ufixed`, `Sfixed`, and the future `Float`. `StaticRangedSequence` narrows to those whose `Range` is a compile-time constant.

The cross-cutting formatting, hashing, and construction-vs-conversion conventions live in `conventions_api.md`. `Range` and `Direction` are specified in `range_direction_api.md`. The runtime-bounded vs compile-time-bounded distinction is explained in `conventions_api.md`.

## `RangedSequence` and `StaticRangedSequence`

Both concepts are refinements of `std::ranges::random_access_range` that additionally require the type to expose its `Range`. The two forms differ in whether that `Range` is a runtime value or a compile-time constant.

```c++
template <typename T, typename Elem = void>
concept RangedSequence =
    std::ranges::random_access_range<T> &&
    requires(T const& t) {
        { t.range() } -> std::convertible_to<Range>;
    } &&
    (std::is_void_v<Elem> || std::same_as<std::ranges::range_value_t<T>, Elem>);

template <typename T, typename Elem = void>
concept StaticRangedSequence =
    RangedSequence<T, Elem> &&
    requires { /* T::static_range is a compile-time constant */ };
```

- **Random-access, not just range** -- `RangedSequence` refines `std::ranges::random_access_range`, so `size()`, `begin()`, `end()`, `rbegin()`, `rend()`, and O(1) random-access iteration are all available on any `RangedSequence`.
- **Dynamic-range access** -- `t.range()` must be callable on a `const T&` and return something `std::convertible_to<Range>`. This is the runtime shape descriptor.
- **Static-range access** -- `StaticRangedSequence` additionally requires `T::static_range` to be a compile-time constant of type `Range`.
- **Optional element constraint** -- the concept takes a second template parameter `Elem` (defaulted to `void`) that pins the element type. `RangedSequence<T>` matches any element type; `RangedSequence<T, Logic>` matches only sequences of `Logic`. This is used at binding sites for element-typed generic code (e.g. bitwise operators over `LogicType`-sequenced containers).

Static ranges are subtypes of dynamic ranges: `StaticRangedSequence` refines `RangedSequence`, so anything that satisfies `StaticRangedSequence` also satisfies `RangedSequence` and exposes `range()`.

The intended use case is to write generic code against `RangedSequence`, then narrow with `StaticRangedSequence` for constant folding where valuable:

```c++
template <typename LHS, typename RHS, typename Op>
constexpr void logic_inplace_array(LHS& lhs, RHS const& rhs, Op op) {
    // When both sides have compile-time-known ranges, fold the length check
    // into a static_assert -- mismatch becomes a compile error instead of a
    // runtime throw, and the runtime branch drops out of generated code.
    if constexpr (StaticRangedSequence<LHS> && StaticRangedSequence<RHS>) {
        static_assert(
            std::remove_cvref_t<LHS>::static_range.length()
                == std::remove_cvref_t<RHS>::static_range.length(),
            "Bitwise compound assignment requires arrays of equal length"
        );
    } else if (lhs.range().length() != rhs.range().length()) {
        throw std::invalid_argument(
            "Bitwise compound assignment requires arrays of equal length, got "
            + std::to_string(lhs.range().length()) + " and "
            + std::to_string(rhs.range().length())
        );
    }
    auto it = std::ranges::begin(rhs);
    for (auto& v : lhs) {
        v = op(v, *it++);
    }
}
```

## `std::find` on `RangedSequence`

`RangedSequence`s are `std::ranges::random_access_range`s, so they support `std::find` and every other stdlib range algorithm.

## `index_of` and `rindex_of` on `RangedSequence`

Free functions constrained to `RangedSequence`. They return the HDL coordinate (i.e. the position in the sequence's `Range`) where the given value is found, or `nullopt` if the value is absent.

```c++
template <RangedSequence S>
constexpr std::optional<Range::value_type> index_of(
    S const& s, std::ranges::range_value_t<S> const& v
);

template <RangedSequence S>
constexpr std::optional<Range::value_type> rindex_of(
    S const& s, std::ranges::range_value_t<S> const& v
);
```

- `index_of` returns the coordinate of the first matching element in iteration order (leftmost in HDL terms).
- `rindex_of` returns the coordinate of the last matching element (rightmost).

Being free functions constrained by the concept, they impose no per-type surface: any `RangedSequence` automatically supports the value-lookup shape without having to add `index` / `rindex` members.

```c++
Array<int, Range{3, 0}> a {3, 1, 2, 3};
auto b = rindex_of(a, 3);  // 0 (right-most)
auto c = index_of(a, 0);   // nullopt
```

## Operations between static-bounded and runtime-bounded `RangedSequence`s

Downstream API specs (e.g. `logic_array_bit_array_api.md`) define binary operations between `RangedSequence`s. These binary operations may take any combination of `Array`, `Vector`, `ArraySlice`, or `StaticArraySlice` operands. They typically result in a new object which is the closest subtype of the two operands.

Remember that `StaticRangedSequence` is a subtype of `RangedSequence`.

The rules:

* Both operands are `StaticRangedSequence` (`Array` or `StaticArraySlice`).
  The result is an `Array`.
* At least one operand is not a `StaticRangedSequence`, but is a `RangedSequence` (`Vector` or `ArraySlice`).
  The result is a `Vector`.

## Binary operations between `RangedSequence`s of unrelated types

When doing binary operations where the two operands are `RangedSequence`s, the result is the appropriate `RangedSequence` type (per the section above) with an element type that is the resulting type of the combination of the two element types in the operation.

For example:

```c++
Array<int, 4> a;
Array<float, 4> b;
auto c = a + b;  // c is Array<float, 4>

Vector<Logic> d;
Vector<Bit> e;
auto f = d & e;  // f is Vector<Logic>
```
