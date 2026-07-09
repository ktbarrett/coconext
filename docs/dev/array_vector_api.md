# `Array<T, ...>` and `Vector<T>` API Specification

This document specifies the API for the two type-generic owning array-like containers in coconext: the compile-time-bounded `Array<T, ...>` and the runtime-bounded `Vector<T>`. It is a design spec.

The cross-cutting formatting, hashing, construction-vs-conversion, and static-vs-runtime-bounded conventions live in `conventions_api.md`. `Range` and `Direction` are specified in `range_direction_api.md`. `RangedSequence` and the cross-type binary-operation rules are in `ranged_sequence_api.md`. Non-owning slice views are specified in `array_slice_api.md`.

## `Array`

Type-generic array-like collection. Indexable, iterable, sliceable. The only difference between `Array` and a C++ array is that `Array` uses a `Range` to describe the index of each element.

The `Range` maps indexes left-to-right to elements in the array left-to-right. So if an `Array` has 4 elements and the `Range` is `0 to 3`, the left-most element is at index 0 and the right-most element is at index 3. Likewise, if an `Array` has 123 elements and the `Range` is `78 downto -54`, the left-most element is at index 78 and the right-most element is at index -54.

`Array` uses local storage (`std::array`), so it will likely be passed around as an allocated object: `unique_ptr<Array<int, 8>>`. Or it may be included as part of a struct, making the struct larger or smaller accordingly.

### Fulfils

* `StaticRangedSequence`
* `RangedSequence`
* `std::ranges::sized_range`

### Construction

`Array<T, ...>` always takes the element type first; the remaining template arguments describe its shape and can take 1, 2, or 3 forms:

* **1 shape arg as a length**  --  deduces `Range` to `0 to length-1`. `LogicArray<N>` and `BitArray<N>` invert this to `N-1 downto 0` to match HDL convention; see `logic_array_bit_array_api.md`.
* **1 shape arg as a `Range`**  --  uses the given `Range` directly.
* **2 shape args**, `left` and `right`  --  deduces direction so the `Array` is non-null (similar to Verilog).
* **3 shape args**, `left`, `direction`, `right`  --  directly constructs a `Range`.

```c++
Array<int, 8>;
Array<Array<int, 8>, Range{-10, Direction::TO, 10}>;
Array<std::string, 12, 10>;
Array<MyStruct, -4, Direction::TO, 3>;
```

Once the type is instantiated, the object can be either default-constructed, constructed from an initializer list, or constructed from any `sized_range`. This is very similar to `std::array`.

```c++
// default initializes
Array<int, 4> a;
// initializer list (length must match the Range)
Array<int, 8> b = {1, 2, 3, 4, 5, 6, 7, 8};
// from any sized_range whose elements are constructible into the Array's element type
std::vector<int> v {1, 1, 2, 3, 5, 8};
Array<int, 6> c (v);
```

The sized-range ctor is `explicit` and requires `std::constructible_from<T, std::ranges::range_reference_t<R>>`  --  i.e. the source's elements have to construct into the destination's element type, which may involve any (implicit or explicit) converting ctor on `T`. Throws `std::invalid_argument` on length mismatch, and whatever `T`'s converting ctor throws (typically `std::invalid_argument`) if an element fails to convert. This subsumes cross-container widening / narrowing: e.g. an `Array<Logic, ...>` from a `BitArray` uses the implicit `Bit -> Logic` at each element; an `Array<Bit, ...>` from a `LogicArray` uses the explicit `Bit(Logic)` and throws on unresolvable values. `Array`s can be constructed from other `Array`s, `Vector`s, slices, or any foreign `sized_range`.

### Conversion

None.

### Ordering

Only supports `==` and `!=`. Substitutability requirements of hashable equatable objects mean this is strict: two `Array`s are equal iff both have the same value and the same range. Cross-container (`Array` vs `Vector`) and cross-element-type (`Array<int, ...>` vs `Array<float, ...>`) comparisons are compile errors.

Use `std::ranges::equal` to compare element-wise against another `std::ranges::range`, e.g. an Array with a Vector, or an ArraySlice with an Array.

### Iteration

Implements `std::ranges::range`, so it has `begin()` and `end()` methods. It also has reverse iterators `rbegin()` and `rend()`. The iterators are `random access iterators`, which implies `Array` also fulfils `std::ranges::sized_range` and works with `std::size` and `std::distance`.

### Indexing and slicing

Runtime indexing and slicing use `operator[]`. Indexes take `Range::value_type` and return a reference to an element. Slicing takes a `Range` and returns an `ArraySlice`. Under C++23 the slicing form also accepts multi-argument subscripts `a[l, r]` and `a[l, dir, r]` as sugar for `a[Range{l, r}]` / `a[Range{l, dir, r}]`.

Static indexing and slicing use the `index<int64_t>()` and `slice<Range>()` methods. Slicing returns a `StaticArraySlice`.

```c++
Array<int, Range{0, 7}> a {1, 2, 3, 4, 5, 6, 7, 8};
a[0];          // 1
a[{3, 5}];     // ArraySlice[3 to 5]{4, 5, 6}
a[3, 5];       // same, C++23
a.index<6>();            // 7
a.slice<Range{6, 7}>();  // StaticArraySlice[6 to 7]{7, 8}
```

### Value lookup

`index_of(seq, value)` and `rindex_of(seq, value)` (free functions constrained to `RangedSequence`) return the HDL coordinate at which `value` first / last appears in the sequence, or `nullopt`. See `ranged_sequence_api.md`.

### Formatting and hashing

`Array` supports `std::hash` and `std::formatter`, per the conventions in `conventions_api.md`. `std::hash<Array>` hashes only the elements: the `Range` is part of the type, so it does not need to feed the hash.

`std::formatter<Array>` produces `Array[range]{elem, elem, ...}`, e.g. `Array[0 to 3]{1, 2, 3, 4}`. The element type is not included in the header  --  it is compile-time information that isn't recoverable from a formatter and doesn't round-trip through any parser. Element values are formatted via `std::formatter<T>`; the specialization is available only when `T` is itself `Formattable`. Takes no format spec; any spec character throws `std::format_error` at parse time.

## `Vector`

Much the same as `Array`, but it uses a runtime `Range` instead. `Vector` uses heap storage (`T[]`). `Vector` won't change the size of structs it's included in, and will be passed around by value/move like `std::vector`.

There is no `static_range` static member variable.

`slice<R>()` still returns a `StaticArraySlice`. The slice type is based on how the value was sliced, not on the kind of object that was sliced.

### Fulfils

* `RangedSequence`
* `std::ranges::sized_range`

### Construction

The template only includes the element type, which is required. The constructor takes a `Range` (for the shape), data, or both. When given both, the data comes first, then the `Range`.

```c++
// Just the Range, default constructs the elements.
Vector<int> a (Range{10, 20});
// Just the value, deduces a Range 0 to length-1.
Vector<int> b {1, 2, 3, 4, 5};
std::vector<int> c {1, 2, 3, 4};
Vector<int> d (c);
// Both the value and Range; data first, then Range.
Vector<int> e (c, Range{100, 103});
Vector<int> f ({1, 2, 3, 4}, Range{0, Direction::DOWNTO, -3});
```

The sized-range ctors are `explicit` and use the same `std::constructible_from<T, ...>` requirement as `Array`'s. Cross-container / cross-element construction goes through this path.

### Conversion

None.

### Ordering

Same as `Array`: only `==` and `!=`, and equality is strict (same value, same range, same container kind, same element type). Cross-container and cross-element comparisons are compile errors.

### Iteration

Same as `Array`: `std::ranges::range` with random-access iterators and reverse iterators.

### Indexing and slicing

Same as `Array`. Runtime indexing/slicing via `operator[]` (including the C++23 multi-argument forms); static slicing via `slice<R>()`, which returns a `StaticArraySlice`.

### Value lookup

Same as `Array`: use the free `index_of` / `rindex_of`.

### Formatting and hashing

`Vector` supports `std::hash` and `std::formatter`, per the conventions in `conventions_api.md`. Unlike `Array`, `std::hash<Vector>` mixes the runtime `Range` into the hash alongside the elements  --  the range is not part of the type here, and strict equality requires both to match.

`std::formatter<Vector>` produces `Vector[range]{elem, elem, ...}`, e.g. `Vector[100 downto 97]{1, 2, 3, 4}`. Element type omitted, same reasoning as `Array`. Specialization available only when the element is `Formattable`. Takes no format spec; any spec character throws `std::format_error` at parse time.

## `constexpr` and `noexcept` contract

Every operation on `Array` and `Vector` is `constexpr` conditional on the element type `T`  --  the container propagates whatever `constexpr`-ness `T`'s copy, move, and destruction expose. `Array` is usable as an NTTP when `T` is a structural type; `Vector` never is (heap storage).

Throw sites:

- `Array<T>` initializer-list and sized-range ctors throw `std::invalid_argument` on length mismatch against the compile-time `Range`.
- `Vector<T>` ctors additionally allocate and may throw `std::bad_alloc`.
- `operator[](Range::value_type)` throws `std::out_of_range` on an out-of-range coordinate.
- `operator[](Range)` throws `std::invalid_argument` if the slicing `Range` is not a subsequence of the container's.
- `std::formatter::parse` throws `std::format_error` on any non-empty spec.

`noexcept`-ness of everything else (copy, move, iteration, `range()`, `size()`, formatter `format()`) tracks `T`'s corresponding operations. Nothing is unconditionally `noexcept` because element construction runs `T`'s ctors.
