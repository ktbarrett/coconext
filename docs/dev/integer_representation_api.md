# Integer representation architecture

`detail::UInt<W>` and `detail::SInt<W>` are aliases of the owning
`detail::Int<W, Signed>` bounded integer representation beneath the Range-based
`Unsigned<R>` and `Signed<R>` user types. `DynUInt` and `DynSInt` similarly
alias `detail::DynInt<Signed>` when the width is a runtime value.

There is deliberately no sign-agnostic integer value type and no separate
storage-owner type. `Int` and `DynInt` directly own their storage, maintain the
signed or unsigned invariant, and implement the integer algorithms. Only the
LLVM APInt-derived primitive kernels are shared through non-owning
`WordSpan`/`WordConstSpan` views.

## Layers

```text
Unsigned / Signed / future Ufixed / Sfixed
                    |
        Int / DynInt owning representations
                    |
          non-owning word-span kernels
```

`BitArray<R>` uses `UInt<R.length()>` as its packed representation. Range
direction controls HDL indexing while the representation supplies compact bit
storage, iteration, and bitwise operations.

## Representation invariant

Physical storage follows the native/word-array tier rather than ending at the
logical width `W`:

- `UInt<W>` contains zeroes above bit `W - 1`.
- `SInt<W>` contains copies of bit `W - 1` above the logical width.

The invariant is restored only at boundaries that can invalidate it:
construction, narrowing/conversion, exact-width wrapping arithmetic, left
shift, and sign-bit mutation. Growing arithmetic consumes and produces
canonical storage directly.

## Arithmetic

Signedness is selected by the operand type:

```cpp
UInt<8> u;
SInt<8> s;

auto uu = u + u;  // UInt<9>
auto ss = s + s;  // SInt<9>
auto us = u - u;  // SInt<9>
```

Ordinary operators grow to represent their complete result:

- addition and subtraction: `max(Wa, Wb) + 1`;
- multiplication: `Wa + Wb`;
- division quotient: `Wa + 1`;
- remainder/modulo: `Wb`;
- negation and absolute value: `W + 1`.

`divrem` performs truncating division in one pass. `divmod` and `mod` name the
distinct floor-division semantics. `exact_add`, `exact_sub`, and `exact_mul`
are explicitly named because they wrap to the operand width.

## Comparison and shifts

Signedness selects the meaning of the ordinary operators. `UInt` comparisons
are unsigned and `SInt` comparisons are signed. `UInt >> n` is a logical right
shift, while `SInt >> n` is an arithmetic right shift. There are no parallel
`ult`/`slt` or `srl`/`sra` member APIs.

Fixed-width comparison is type-strict: both operands have the same width and
signedness. This preserves the exact-type comparison contract of the
Range-based `Unsigned<R>` and `Signed<R>` types. `DynInt` equality includes its
runtime width; ordering values with different runtime widths throws
`std::invalid_argument`. Width-extending comparison remains a private
implementation detail used by saturation.

## Integrated storage

`Int<W, Signed>` selects the smallest native unsigned integer capable of
holding its physical width, using `__uint128_t` where available and an owned
`std::array<uint64_t, N>` beyond that. `DynInt<Signed>` contains its small
buffer directly and switches to heap storage beyond the inline capacity.

Both representation classes have direct access to their storage. Their
comparisons, arithmetic, division, bit access, and formatting live on the same
class; there is no storage wrapper or algorithm-dispatch class between the
representation and its words. The kernels in `bigint.hpp` operate only on
temporary spans supplied by `Int` or `DynInt` and remain shared between the two
storage strategies.

Fixed representations stay in native arithmetic whenever every participating
operand and result uses a native tier. Construction, conversion, comparison,
counting, bitwise operations, shifts, exact arithmetic, growing arithmetic,
and division all operate directly on the scalar storage. A native value is
adapted to a word span only when an operation also contains genuinely wide
storage; for example, mixed-width growing arithmetic or a wide division with a
native-width remainder.

## Zero width

`UInt<0>` and `SInt<0>` represent an empty bit sequence rather than the integer
zero. Bit operations and iteration are empty. Growing arithmetic may produce a
non-empty zero result; division by a zero-width divisor is division by zero.
