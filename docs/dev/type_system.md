
# Type System Design

coconext adds a bunch of types for dealing with simulations.
A lot of these are inspired by cocotb's types which try to solve the same problem,
but we want to add more functionality.
C++ also offers us some features which can enhance these data types,
so a proper redesign for C++ is worth exploring.

## Formatting

C++ provides `std::formatter` for doing customizable string formatting.
This is the primary way this library handles printing values for diagnostics.

The type should be the first thing in the representation, e.g. `LogicArray` or `Sfixed`.

Ranged objects use `[0 to 7]` to describe the range.

There is usually a field to describe the value,
which should be a natural literal value,
e.g. a bit string for `LogicArray` (`LogicArray"10XZ"`) or an integer for unsigned (`00123`).
These should be inside of curly brackets like an initializer would.

Together here are some examples:

```
LogicArray[0 to 7]{"01XZULHW"}
Unsigned[3 downto 0]{12}
Sfixed[7 downto -2]{-67.25}
```

### As opposed to `to_string`

String conversion is converting the value from one type into a representation in a string type,
it is not for printing, generally.

For example, `LogicArray` has a string-typed representation: the bit string;
however, you want more information for printing.

```c++
to_string("0110"_l);         // 0110
std::format("{}", "0110"_l)  // LogicArray[3 downto 0]{"0110"}
```

There can only be *one* string representation of a value and it must be convertible both to and from the other type.

```c++
// Good.
to_logic_array("0110");
// I don't want to have to parse this...
to_logic_array("LogicArray[3 downto 0]{'0110'}")
```

## Hashing

Objects can be stored in hashed collections like `std::unordered_map`.
To do so they need to implement `std::hash`.

C++ requires that if two objects are equal their hashes must be equal.

We only need to implement hashability for owning collections as we can't store views like `ArraySlice` for indefinite amounts of time.

## Construction vs Conversions

Conversions are best done as free functions.
We should avoid using C++ constructors or conversion operators for type conversions.

Free functions are customization points where users (or coconext) can write generic code and adapt their own types to it.
C++ constructors/conversion operators are not extensible and have some confusing rules,
especially around implicit conversions.

One caveat to that rule is *implicit* conversions, which must be implemented as either an implicit constructor or conversion operator.
The only thing that should be implicitly convertible are loss-less transformations;
i.e. subtypes to supertypes.

The other side of the coin is construction which is about "building" values.
Composites like `Range` and `Array` are obvious examples.
But also we need to "build" non-composite value types like `Logic` or `BitArray`.
In those cases we should provide the minimal possible interface and prefer UDLs if they make sense.

```c++
Logic{0};  // BAD. we have UDLs: '0'_l

void print_inv(char c) {
   Logic a = c;  // BAD. This can throw, which makes it odd.
   Logic b{c};   // BAD. This only works because it's char. Not extensible to downstream types.
   auto d = to_logic(c);   // GOOD customizable by the downstream users.
   std::string e {'x'_l};  // BAD. Just odd, would need *every* string-like overload.
   auto f = to_string(b);  // GOOD nice and generic, can return one of any kind of string-like type.
   printf("%s\n", f.c_str());
}
```

### String conversions use `to_string`

String conversion is one motivating example as to why to use free functions.

There are several string-like representations: `std::string`, `std::string_view`, `const std::string&`, etc.
All of these are typically fine to return from a string conversion function:
all we care is that we can inspect the value and use it to build other strings,
i.e. use it in `string +=` or `std::string c = to_string(...)`.

This allows us to return different types in an optimal way,
e.g. `to_string(Logic)` returns a `std::string_view` of a static string (i.e. no allocation),
but `to_string(LogicArray)` has to build a `std::string`, so it might as well return that.

Conversion operators cannot do this, since you have to name a particular type:
```c++
// unnecessary copy
std::string('0'_l);
// illegal, this would be a view of a temporary
std::string_view("01010101"_l);
```

### User-Defined Literals

UDLs are one of the better ways to provide construction of an object, as opposed to converting constructors:

```c++
// converting constructor
Logic{0}
// UDL
'0'_l
```

* UDLs reduce syntactic noise.
* Malformed UDLs will fail at compile time.
   * Constructors do not have to be run at compile time, and even if they do will just cause an exception to be thrown at runtime.

## `Logic` and `Bit`

These are the core elements of arrays in HDL.
`Logic` is a 9-value type that follows VHDL.
SystemVerilog's 4-value type fits inside of VHDL's 9-value type.
They support logical operators.

### Fulfils

* `LogicType`

### Construction

These only provide UDLs.
`_l` suffixed char literals are Logic.
`_b` suffixed are Bit.

```c++
'0'_b;  // Bit
'0'_l;  // Logic
```

### Conversion

These can be converted to and from `char`, strings, integers, and bool.
`Bit` has an implicit `int` and `bool` conversion since it is substitutable with 0/1 integers and true/false.
`Bit` can be implicitly upcast to `Logic` for the same reason: `Bit` is a subtype of `Logic`.

Trying to convert a non-0/1/L/H `Logic` to int or bool will result in an exception.

```c++
to_logic('0');
to_logic("0");
to_logic(0);
to_logic(false);
// These all work for to_bit as well
to_int('0'_l);
to_string('0'_l);
to_int('X'_l);      // throws
to_string('W'_l);   // throws
```

### Operations

These support `std::hash` and `std::formatter`.

For ordering, these only support `==` and `!=`.
Total ordering is odd as soon as meta-values become involved (`'0'_l < 'X'_l`).
Equality does not consider different types equal.

These support logical operations.
Logic is the wider supertype that Bit will implicitly upcast to in mixed operations.

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

There should be space for other related types to be added, e.g. `X01Z` which is just the 4 SystemVerilog values.
There's no need for it now, but maybe in the future.

## `is_logic` / `LogicType`

`is_logic` is an opt-in trait that registers the given type as a `Logic`-like type.
`LogicType` is the corresponding concept.
Currently limited to `Logic` and `Bit`.

## `Range`

This is a lazily-evaluated integer sequence that is described with inclusive bounds on each side.
Ranges can be described as descending `4 downto 1` or ascending `-2 to 8`.

Ranges can be null if there are no values in the iteration.
For example `0 to -2` is null because there is no value counting from 0 up to -2;
there is no way to count in that way.

Ranges are typically used as ways to describe the indexing scheme of Array-like objects in HDLs, such as arrays of `Logic` or `Unsigned` values.

### Fulfils

* `std::ranges::sized_range`

### Construction

Ranges can be constructed in a 1-argument, 2-argument, or 3-argument form.
The 1-argument form is `explicit`; the others are implicit.
* 1-arg form: this is just a length, the direction is inferred to be `TO` and the left is `0` and the right is `{arg}-1`.
* 2-arg form: this is `left` and `right` with an inferred direction so that the range is never null.
* 3-arg form: this is `left`, `direction`, `right`.

```c++
auto a = Range{1, 8};  // direction inferred
a.length();  // 8
a.left       // 1
a.right      // 8
a.direction  // Direction::TO

auto b = Range{0, Direction::TO, -2};  // null range
b.length()   // 0

auto c = Range{1000}
c.length();   // 1000
c.left;       // 0
c.direction;  // Direction::TO
c.right;      // 999
```

### Invalid values

There are two invalid Range values.
These two values have a `length()` that exceeds the precision of the `length()` result datatype.
These are *not* checked at construction of a `Range`,
and are generally not checked consistently through the rest of the APIs.
Don't do anything stupid.

```
Range{std::numeric_limits<int64_t>::min, Direction::TO, std::numeric_limits<int64_t>::max};
Range{std::numeric_limits<int64_t>::max, Direction::DOWNTO, std::numeric_limits<int64_t>::min};
```

### Conversion

None.

### Operations

Ranges support `std::hash` and `std::formatter`.

Ranges allow users to access the three elements and mutate them as they see fit.
This allows users to change mutable Ranges at runtime.
This was decided to not be an issue as downstream types in this repo (`Array`, `Vector`, etc.) have precautions in place to prevent this from becoming an issue.
This could be an issue if a user mutates the Range into one of the invalid Ranges.

```c++
Range r {-10, Direction::TO, 10};
r.left = 90;
r.length();   // 0
r.direction = Direction::DOWNTO;
r.length();   // 81
```

### Subsequence

Ranges have the concept of "subsequence."
A Range is a subsequence of another if all values in a Range are present in another in the same order.

* [0 to 7] is a subsequence of [-100 to 100]
* [0 to 7] is not a subsequence of [100 downto -100]
   * wrong order
* [0 to 7] is not a subsequence of [8 to 100]
   * no overlap
* [0 to 7] is not a subsequence of [4 to 9]
   * 0 to 3 is not in 4 to 9
* [0 downto 10] is a subsequence of [-1 downto -4]
   * the null range is the empty sequence [], which is a subsequence of any other sequence, e.g. [-1, -2, -3, -4]

You can check this with the `is_subsequence_of` method.

```c++
Range r {0, 10};
Range q {3, 4};
q.is_subsequence_of(r);
!r.is_subsequence_of(Range{10, 0});
```

### `std::find`

Finally, as a `std::ranges::range` it can be used with `std::find`.
A more efficient overload for this is provided.

### Ordering/Equality

Ranges only support `==` and `!=`.

### Iteration

Ranges are also `std::ranges::range` so you can iterate over them using ranged for loops,
or by manually calling `begin()` and `end()`.
Reverse ranges are also available via `rbegin()` and `rend()`.
Iterators are `random access iterators`.
This implies they also fulfil `sized_range` and work with `std::size` and `std::distance`.

```c++
for (auto a : Range{10, 20}) {
   std::cout << a << " ";
}
// 10 11 12 13 14 15 16 17 18 19 20
```

## Runtime-bounded vs compile-time bounded types

You'll also hear these referred to as dynamic vs static bounds.

Many of the remaining types contain widths or `Range` objects that describe their size which are supplied either at runtime or compile-time:

* runtime-bounded:
   The `Range` is supplied to the constructor as a normal argument and this is stored on the object at runtime.
   This allows the `Range` to be supplied using information from runtime-only APIs like VPI's `vpi_get(vpiRangeLeft, obj)`.
   This is also how Python will be able to create objects since all values are naturally runtime in that language.
   Size mismatches and bounds checking must be checked at runtime.
* compile-time-bounded:
   The `Range` is supplied to the *type* as a non-type template argument.
   This object is part of the type and is not stored on the runtime object.
   This has the benefit of doing size mismatch and static index/slicing checking at compile-time.
   It also allows the constant-folding of some operations and not storing the `Range` object at runtime,
   saving space and allowing for adjacent data members in arrayed data (auto-vectorization).

cocotb only provides `cocotb.types.Array` and `cocotb.types.LogicArray` as runtime-bounded,
so those two types are the extent we care about runtime-bounded types in coconext for now.
Maybe in the future all types can have a runtime-bounded version,
but it's a lot of engineering work for a worse solution.

## `Array`

Type-generic array-like collections.
Indexable, iterable, sliceable.
The only difference between this and a C++ array is that this uses a `Range` to describe what the index of each of the elements is.

The `Range` maps indexes left-to-right to elements in the array left-to-right.
So if an `Array` has 4 elements and the `Range` is `0 to 3`,
the left-most element is at index 0 and the right-most element is at index 3.
Likewise if an `Array` has 123 elements and the Range is `78 downto -54`,
the left-most element is at index 78 and the right-most element is at index -54.

`Array` uses local storage (`std::array`),
so it will likely be passed around as an allocated object: `unique_ptr<Array<int, 8>>`.
Or it may be included as part of a struct, making the struct larger or smaller accordingly.

### Fulfils

* `StaticRangedSequence`
* `RangedSequence`
* `std::ranges::sized_range`

### Construction

`Array<T, ...>` always takes the element type first; the remaining template arguments describe its shape and can take 1, 2, or 3 forms:

* 1 shape arg as a length: deduces Range to `0 to length-1`.
* 1 shape arg as a `Range`: use the given Range directly.
* 2 shape args, `left` and `right`: deduces direction so the Array is non-null (similar to Verilog).
* 3 shape args, `left`, `direction`, `right`: directly constructs a Range.

```c++
Array<int, 8>;
Array<Array<int, 8>, Range{-10, Direction::TO, 10}>;
Array<std::string, 12, 10>;
Array<MyStruct, -4, Direction::TO, 3>;
```

Once the type is instantiated the object can be either default constructed,
constructed from an initializer list, or from some `sized_range`.
This is very similar to `std::array`.

```c++
// default initializes
Array<int, 4> a;
// initializer list (length must match the Range)
Array<int, 8> b = {1, 2, 3, 4, 5, 6, 7, 8};
// from any sized_range with convertible element type
std::vector<int> v {1, 1, 2, 3, 5, 8};
Array<int, 6> c (v);
```

Arrays can also be constructed from any `sized_range` including other `Array`s and subtypes thereof.

### Conversion

None.

### Ordering

Only supports `==` and `!=`.
Substitutability requirements of hashable equatable objects mean that this is strict:
two Arrays are equal iff both have the same value and range.

### Iteration

Implements `std::ranges::range`, so it has a `begin()` and `end()` method.
Also has a reverse iterator `rbegin()` and `rend()`.
The iterators returned are `random access iterators`.
This implies it also fulfils `std::ranges::sized_range` and works with `std::size` and `std::distance`.

### `index()` and `rindex()`

These return the index in the `Range` where the given value is found (or `nullopt`).

```c++
Array<int, Range{3, 0}> a {3, 1, 2, 3};
auto b = a.rindex(3);  // 0 (right-most)
auto c = a.index(0);   // nullopt
```

### `std::find()`

This is a `std::ranges::range` so find works for STL-like flows.

### Indexing and Slicing

Runtime indexing and slicing is accomplished via `operator[]`.
Indexes take `Range::value_type` and return a reference to an element.
Slicing takes a `Range` as the argument and returns an `ArraySlice`.

Static indexing and slicing is accomplished via the `index<int64_t>()` and `slice<Range>()` methods.
Slicing returns a `StaticArraySlice`.

```c++
Array<int, Range{0, 7}> a {1, 2, 3, 4, 5, 6, 7, 8};
a[0];          // 1
a[{3, 5}];     // ArraySlice[3 to 5]{4, 5, 6}
a.index<6>();            // 7
a.slice<Range{6, 7}>();  // StaticArraySlice[6 to 7]{7, 8}
```

## Vector

Much the same as `Array`, but it uses a runtime `Range` instead.
`Vector` uses heap storage (`T[]`).
`Vector` won't change the size of structs it's included in
and will be passed around by value/move like `std::vector`.

There is no `static_range` static member variable.

`slice<R>()` still returns a `StaticArraySlice`.
The slice type is based on how the value was sliced, not the kind of object that was sliced.

### Construction

The template only includes the element type, which is required.
The constructor takes a `Range` (for the shape), data, or both.
When given both, the data comes first then the `Range`.

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

## `RangedSequence` and `StaticRangedSequence`

Basically a `range` concept from the stdlib, but requires it to have a `Range`.
Dynamic ranges are provided by the `range()` member function.
Static ranges are provided by the `static_range` static member variable.

Static ranges are subtypes of dynamic ranges: they also have to have the `range()` member function.

The intended use case is to write generic code against `RangedSequence`, then narrow with `StaticRangedSequence` for constant folding if valuable:

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

## `ArraySlice`

A non-owning view of an `Array` or `Vector`.
Slices have a subsequence of the Range of the parent object, and correspondingly refer to the data at those locations.
They are designed to work very similarly to `Array` or `Vector`.

### Construction

Not constructible by the user.

### Conversion

None.

### Ordering

None.

Not hashable, so not orderable.

Use `std::ranges::equal` to compare element-wise against another `std::ranges::range`.

### Indexing and Slicing

Basically the same as `Array` and `Vector`.

This supports the static `slice<R>()` despite it being a runtime-bounded slice.
That requires a runtime-check to construct, but afterwards bounds-checking with further static slicing/indexing is compile-time checked.

### Iteration, `index()` and `rindex()`, `std::find()`

Exactly the same as `Array` or `Vector`.

## `StaticArraySlice`

Conceptually the same thing as `ArraySlice` but uses compile-time bounds.
The only difference is this has a `static_range` static member variable,
which is used for compile-time bounds checks and constant folding.

## `LogicArray` and `LogicVector`

## `LogicArraySlice` and `StaticLogicArraySlice`

Same as `ArraySlice` and `StaticArraySlice`, but support logical operators like `LogicArray`.

* `&`, `|`, `^`, `~`
* `and_reduce`, `or_reduce`, `xor_reduce`

## `BitArray`

## `Unsigned` and `Signed`

## `Ufixed` and `Sfixed`

## `Float`

## Type Hierarchy
