# Type-system Conventions API Specification

This document specifies the cross-cutting conventions that the coconext type family shares: formatting, hashing, construction vs conversion, and the base type-classification customization points. It is a design spec; the implementation may only partially cover these decisions.

Motivation and scope: coconext adds a family of types for dealing with simulations, inspired by cocotb's types but extended and adapted to C++ conventions. This document captures the rules that govern how all of those types are formatted, hashed, constructed, and converted so that per-family API specs (`logic_bit_api.md`, `range_direction_api.md`, `array_vector_api.md`, `array_slice_api.md`, `logic_array_bit_array_api.md`, `ranged_sequence_api.md`, `unsigned_signed_api.md`, `sfixed_ufixed_api.md`, `integer_representation_api.md`) don't need to restate them.

## Formatting

C++ provides `std::formatter` for doing customizable string formatting. This is the primary way this library handles printing values for diagnostics.

The type should be the first thing in the representation, e.g. `LogicArray` or `Sfixed`.

Ranged objects use `[0 to 7]` to describe the range.

There is usually a field to describe the value, which should be a natural literal value, e.g. a bit string for `LogicArray` (`LogicArray"10XZ"`) or an integer for unsigned (`00123`). These should be inside of curly brackets like an initializer would.

Together, some examples:

```
LogicArray[0 to 7]{"01XZULHW"}
Unsigned[3 downto 0]{12}
Sfixed[7 downto -2]{-67.25}
```

### Formatting is distinct from `to_string`

String conversion is converting the value from one type into a representation in a string type; it is not for printing, generally.

For example, `LogicArray` has a string-typed representation: the bit string. But you want more information for printing.

```c++
to_string("0110"_l);         // 0110
std::format("{}", "0110"_l)  // LogicArray[3 downto 0]{"0110"}
```

There can only be *one* string representation of a value and it must be convertible both to and from the other type.

```c++
// Good.
LogicArray<4>{"0110"};
// I don't want to have to parse this...
LogicArray<4>{"LogicArray[3 downto 0]{'0110'}"};
```

## Hashing

Objects can be stored in hashed collections like `std::unordered_map`. To do so they need a `std::hash` specialization.

C++ requires that if two objects are equal their hashes must be equal.

Hashability is provided only for **owning** collections. Non-owning views (`ArraySlice`, `StaticArraySlice`) are not hashable, because they cannot be stored in a hashed collection for an indefinite duration.

### String conversions use `to_string`

String conversion is one motivating example for using free functions.

There are several string-like representations: `std::string`, `std::string_view`, `const std::string&`, etc. All of these are typically fine to return from a string conversion function: all we care is that we can inspect the value and use it to build other strings, i.e. use it in `string +=` or `std::string c = to_string(...)`.

This allows us to return different types in an optimal way. For example `to_string(Logic)` returns a `std::string_view` of a static string (i.e. no allocation), but `to_string(LogicArray)` has to build a `std::string`, so it might as well return that.

Conversion operators cannot do this, since you have to name a particular type:

```c++
// unnecessary copy
std::string('0'_l);
// illegal, this would be a view of a temporary
std::string_view("01010101"_l);
```

### User-defined literals

UDLs are a better ways to provide construction of an object, as opposed to converting constructors:

```c++
// converting constructor
Logic{0}
// UDL
'0'_l
```

* UDLs reduce syntactic noise.
* Malformed UDLs will fail at compile time.
   * Constructors do not have to be run at compile time, and even if they do, will just throw at runtime.

## `is_int` and `Integer`

`is_int` is an opt-in type trait, extendable by the user, that signals a type is an integer type. `Integer` is the associated concept.

The integer type traits defined in the standard tend to include character types like `char`. This type trait exists so that `Integer` and `Character` (defined below) are mutually exclusive sets of types.

The predefined integer types are:

* `unsigned char`
* `signed char`
* `unsigned short`
* `short`
* `unsigned int`
* `int`
* `unsigned long`
* `long`
* `unsigned long long`
* `long long`

Types that match `Integer` should also add an overload for `std::numeric_limits`, since there is an assumption they can be used together.

## `is_char` and `Character`

`is_char` is an opt-in type trait, extendable by the user, that signals a type is a character type. `Character` is the associated concept.

This is equivalent to the concept `CharT` in the STL, which isn't provided publicly.

The predefined character types are:

* `char`
* `wchar_t`
* `char8_t`
* `char16_t`
* `char32_t`

## `ResolveMethod`

`Logic` resolution is the resolution of "metavalues" (`U`, `X`, `Z`, `W`, and `-`) to `0` or `1` so that the resulting value can be converted to an integer or boolean, which require a 0/1 value.

There are five methods for resolving metavalues to 0/1:

* `ERROR`: everything but 0/1 causes a failure during a resolve.
* `WEAK`: `L` maps to 0 and `H` maps to 1; after that, everything but 0/1 causes a failure during resolve.
* `ZEROS`: `L` maps to 0 and `H` maps to 1; after that, all metavalues resolve to `0`.
* `ONES`: `L` maps to 0 and `H` maps to 1; after that, all metavalues resolve to `1`.
* `RANDOM`: `L` maps to 0 and `H` maps to 1; after that, all metavalues resolve randomly to either `0` or `1`.

`ResolveMethod` is consumed by the `resolve` API on `Logic`/`Bit` and on any `RangedSequence` of `Logic`/`Bit` (see `logic_bit_api.md` and `logic_array_bit_array_api.md`).

## Runtime-bounded vs compile-time-bounded types

You'll also hear these referred to as dynamic vs static bounds.

Many types in coconext contain widths or `Range` objects describing their size, supplied either at runtime or compile-time:

* **runtime-bounded**: the `Range` is supplied to the constructor as a normal argument and is stored on the object at runtime. This allows the `Range` to be supplied using information from runtime-only APIs like VPI's `vpi_get(vpiRangeLeft, obj)`. It is also how Python creates objects, since all values are naturally runtime in that language. Size mismatches and bounds checking must be checked at runtime.
* **compile-time-bounded**: the `Range` is supplied to the *type* as a non-type template argument. This object is part of the type and is not stored on the runtime object. This has the benefit of doing size-mismatch and static index/slicing checks at compile time. It also allows constant-folding of some operations, and lets adjacent data members in arrayed data occupy contiguous memory (auto-vectorization).

cocotb only provides `cocotb.types.Array` and `cocotb.types.LogicArray` as runtime-bounded, so those two types are the extent to which we care about runtime-bounded types in coconext for now. In the future all types may have a runtime-bounded version, but it's a lot of engineering work for a worse solution.
