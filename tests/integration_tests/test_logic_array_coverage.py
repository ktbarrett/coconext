"""Pytest coverage for the LogicArray/BitArray nanobind bindings.

These exercise the binding-layer behavior (constructor dispatch, slice
translation, lenient __eq__, etc.) -- not the underlying C++ semantics which
have their own gtest suite.
"""

from __future__ import annotations

import copy

import pytest

from coconext.types import Bit, BitArray, Logic, LogicArray, Range

# -- Construction ----------------------------------------------------------


def test_construct_from_string() -> None:
    la = LogicArray("10X1")
    assert str(la) == "10X1"
    assert la.range == Range(3, "downto", 0)


def test_construct_from_string_with_underscores() -> None:
    la = LogicArray("10_X1")
    assert str(la) == "10X1"


def test_construct_from_string_case_insensitive() -> None:
    la = LogicArray("10xz")
    assert str(la) == "10XZ"


def test_construct_with_int_range() -> None:
    la = LogicArray("1010", 4)
    assert la.range == Range(3, "downto", 0)


def test_construct_with_explicit_range() -> None:
    la = LogicArray("1010", Range(0, "to", 3))
    assert la.range == Range(0, "to", 3)


def test_construct_string_range_length_mismatch() -> None:
    with pytest.raises(ValueError, match="length"):
        LogicArray("1010", 8)


def test_construct_from_iterable() -> None:
    la = LogicArray([Logic("1"), Logic("0"), Logic("X")])
    assert str(la) == "10X"


def test_construct_from_logic_array_copies() -> None:
    a = LogicArray("1010")
    b = LogicArray(a)
    assert a == b
    b[0] = Logic("1")
    assert a[0] == Logic("0")


def test_logic_array_neg_range():
    with pytest.raises(ValueError, match="range length must be non-negative"):
        LogicArray("1001", -2)


def test_logic_array_from_bytes():
    assert LogicArray.from_bytes(b"12", byteorder="little") == LogicArray(
        "0011001000110001"
    )


def test_logic_array_bytes_conversion_invalid_byte_order():
    with pytest.raises(ValueError):
        LogicArray.from_bytes(b"12", byteorder="foo")


def test_logic_array_to_bytes():
    assert LogicArray("").to_bytes(byteorder="big") == b""
    assert LogicArray("0011001000110001").to_bytes(byteorder="little") == b"12"
    with pytest.raises(ValueError, match="byteorder must be either 'big' or 'little'"):
        LogicArray("00101010").to_bytes(byteorder="foo")
    with pytest.raises(ValueError):
        LogicArray("XZX").to_bytes(byteorder="little")


def test_logic_array_deprecated_invalid():
    with (
        pytest.warns(DeprecationWarning),
        pytest.raises(
            ValueError, match="String length must match the LogicArray length"
        ),
    ):
        LogicArray("101").binstr = "0101"


def test_logic_array_invalid_slicing():
    with pytest.raises(IndexError):
        LogicArray("10001", Range(0, "to", 4))[4:2]
    with pytest.raises(IndexError):
        LogicArray("10001", Range(0, "to", 4))[4:2] = "10"


def test_index_invalid():
    r = LogicArray("0001101", Range(1, "to", 7))
    assert r.index(Logic("1"), 5, 7) == 5


def test_format():
    l = LogicArray("1010")
    assert f"{l:\0}" == "10"


# -- Properties ------------------------------------------------------------


def test_left_right_direction() -> None:
    la = LogicArray("0011")
    assert la.left == 3
    assert la.right == 0
    assert la.direction == "downto"


def test_range_setter_relabels() -> None:
    la = LogicArray("1010")
    la.range = Range(7, "downto", 4)
    assert la.left == 7
    assert la[7] == Logic("1")
    assert la[4] == Logic("0")


def test_range_setter_length_mismatch() -> None:
    la = LogicArray("1010")
    with pytest.raises(ValueError, match="Range size mismatch"):
        la.range = Range(0, "to", 7)


def test_is_resolvable() -> None:
    assert LogicArray("1010").is_resolvable
    assert not LogicArray("10X0").is_resolvable


# -- Container protocol ----------------------------------------------------


def test_len_and_iter() -> None:
    la = LogicArray("10X1")
    assert len(la) == 4
    assert [str(b) for b in la] == ["1", "0", "X", "1"]


def test_reversed() -> None:
    la = LogicArray("10X1")
    assert [str(b) for b in reversed(la)] == ["1", "X", "0", "1"]


def test_contains() -> None:
    la = LogicArray("10X1")
    assert Logic("X") in la
    assert Logic("Z") not in la


def test_getitem_int() -> None:
    la = LogicArray("10X1")  # DOWNTO {3..0}: la[3]='1', la[0]='1'
    assert la[3] == Logic("1")
    assert la[2] == Logic("0")
    assert la[1] == Logic("X")
    assert la[0] == Logic("1")


def test_getitem_slice() -> None:
    la = LogicArray("10X1")
    sub = la[2:1]  # DOWNTO slice covering HDL coords 2, 1
    assert str(sub) == "0X"
    assert sub.range == Range(2, "downto", 1)


def test_getitem_slice_step_rejected() -> None:
    la = LogicArray("10X1")
    with pytest.raises(IndexError, match="step"):
        la[3:1:2]


def test_setitem_int() -> None:
    la = LogicArray("0000")
    la[3] = Logic("1")
    assert str(la) == "1000"


def test_setitem_int_accepts_string() -> None:
    la = LogicArray("0000")
    la[3] = "X"
    assert str(la) == "X000"


def test_setitem_slice() -> None:
    la = LogicArray("0000")
    la[3:2] = "11"
    assert str(la) == "1100"


# -- index / count ---------------------------------------------------------


def test_index_found() -> None:
    la = LogicArray("10X1")
    # DOWNTO {3..0}: first '1' in iteration is at HDL 3
    assert la.index(Logic("1")) == 3


def test_index_not_found_raises() -> None:
    la = LogicArray("1010")
    with pytest.raises(ValueError):
        la.index(Logic("Z"))


def test_count_logic_array() -> None:
    la = LogicArray("10X1")
    assert la.count(Logic("1")) == 2
    assert la.count(Logic("X")) == 1
    assert la.count(Logic("Z")) == 0


# -- Bitwise ---------------------------------------------------------------


def test_and() -> None:
    a = LogicArray("1100")
    b = LogicArray("1010")
    assert str(a & b) == "1000"


def test_or() -> None:
    a = LogicArray("1100")
    b = LogicArray("1010")
    assert str(a | b) == "1110"


def test_xor() -> None:
    a = LogicArray("1100")
    b = LogicArray("1010")
    assert str(a ^ b) == "0110"


def test_invert() -> None:
    a = LogicArray("10X1")
    assert str(~a) == "01X0"


def test_bitwise_length_mismatch() -> None:
    a = LogicArray("10")
    b = LogicArray("100")
    with pytest.raises(ValueError):
        a & b


# -- Comparison ------------------------------------------------------------


def test_eq_logic_array() -> None:
    assert LogicArray("1010") == LogicArray("1010")
    assert LogicArray("1010") != LogicArray("1011")


def test_eq_string() -> None:
    assert LogicArray("1010") == "1010"
    assert LogicArray("10X1") == "10x1"  # case-insensitive on RHS
    assert LogicArray("1010") != "1011"


def test_eq_list() -> None:
    assert LogicArray("10X1") == [Logic("1"), Logic("0"), Logic("X"), Logic("1")]
    assert LogicArray("10X1") != [Logic("0"), Logic("0"), Logic("X"), Logic("1")]


def test_eq_tuple() -> None:
    assert LogicArray("10") == (Logic("1"), Logic("0"))


# -- Resolution ------------------------------------------------------------


def test_resolve_zeros() -> None:
    la = LogicArray("1X1")
    assert str(la.resolve("zeros")) == "101"


def test_resolve_invalid() -> None:
    la = LogicArray("1X1")
    with pytest.raises(ValueError):
        la.resolve("bogus")
    with pytest.raises(ValueError):
        la.resolve("error")

    assert LogicArray("101001").resolve("error") == LogicArray("101001")


# -- Repr / format / copy --------------------------------------------------


def test_repr() -> None:
    la = LogicArray("1010")
    assert repr(la) == "LogicArray('1010', Range(3, 'downto', 0))"


def test_format_empty_spec() -> None:
    la = LogicArray("1010")
    assert f"{la}" == "1010"


def test_copy_not_supported() -> None:
    la = LogicArray("1010")
    with pytest.raises(NotImplementedError):
        copy.copy(la)


def test_deepcopy_works() -> None:
    la = LogicArray("1010")
    other = copy.deepcopy(la)
    assert la == other
    other[3] = Logic("0")
    assert la[3] == Logic("1")  # independent storage


# -- BitArray smoke --------------------------------------------------------


def test_bit_array_str_construction():
    assert BitArray("1010", Range(0, "to", 3)) == BitArray("1010")
    assert BitArray("1010", 4) == BitArray("1010")
    assert BitArray("1010", range=Range(0, "to", 3)) == BitArray("1010")

    with pytest.raises(ValueError):
        BitArray("101010", Range(0, "to", 0))

    assert BitArray("1010_1101") == BitArray("10101101")
    assert BitArray("10_____10") == BitArray("1010")
    assert BitArray("_0_") == BitArray("0")
    assert BitArray("___") == BitArray("")


def test_bit_array_rejects_logic_string() -> None:
    with pytest.raises(ValueError):
        BitArray("10X1")  # X not a valid Bit


def test_bit_array_iterable_construction():
    assert BitArray(["1", False, 1, "0", Bit("1"), True]) == BitArray("101011")
    assert BitArray((1, 0, 1, 0), Range(0, "to", 3)) == BitArray("1010")
    assert BitArray((1, 0, 1, 0), 4) == BitArray("1010")
    assert BitArray((1, 0, 1, 0), range=Range(0, "to", 3)) == BitArray("1010")

    def gen():
        yield True
        yield False
        yield "0"
        yield Logic("1")

    assert BitArray(gen()) == BitArray("1001")

    with pytest.raises(ValueError):
        BitArray([1, 0, 1, 0], Range(1, "downto", 0))
    with pytest.raises(ValueError):
        BitArray(["l", "o", "l"])
    with pytest.raises(TypeError):
        BitArray([object()])


def test_bit_array_int_construction():
    with pytest.raises(TypeError):
        BitArray(10)  # refuse temptation to guess

    assert BitArray(10, Range(5, "downto", 0)) == BitArray("001010")
    assert BitArray(10, 6) == BitArray("001010")
    assert BitArray(10, range=Range(5, "downto", 0)) == BitArray("001010")

    assert BitArray(-2, Range(5, "downto", 0)) == BitArray("111110")
    assert BitArray(-2, 6) == BitArray("111110")
    assert BitArray(-2, range=Range(5, "downto", 0)) == BitArray("111110")

    with pytest.raises(ValueError):
        BitArray(10, Range(1, "to", 3))
    with pytest.raises(ValueError):
        BitArray(-10, Range(3, "downto", 0))


def test_bit_array_copy_construction():
    l = BitArray("01101", Range(4, "downto", 0))
    l2 = BitArray(l)
    assert l2 == l
    assert l2.range == l.range
    l3 = BitArray(l, Range(7, "downto", 3))
    assert l3 == l
    assert l3.range == Range(7, "downto", 3)
    l4 = BitArray(l, 5)
    assert l4 == l
    assert l4.range == Range(4, "downto", 0)

    with pytest.raises(ValueError):
        BitArray(l, Range(1, "to", 0))


def test_bit_array_bad_construction():
    with pytest.raises(TypeError):
        BitArray(object())
    with pytest.raises(TypeError):
        BitArray("1010", {})
    with pytest.raises(TypeError):
        BitArray(range={})
    with pytest.raises(TypeError):
        BitArray()


def test_bit_array_setattr():
    l = BitArray("0000")
    l[1] = "1"
    assert l == BitArray("0010")
    with pytest.raises(TypeError):
        l[object()] = "0"


def test_bit_array_repr():
    l = BitArray("110110")
    assert eval(repr(l)) == l


def test_bit_array_and():
    l = BitArray("001111")
    p = BitArray("011010")
    assert (l & p) == BitArray("001010")
    with pytest.raises(TypeError):
        l & object()
    with pytest.raises(TypeError):
        object() & l
    with pytest.raises(ValueError):
        BitArray("") & BitArray("01")


def test_bit_array_or():
    l = BitArray("001100")
    p = BitArray("011010", Range(-9, "downto", -14))
    assert (l | p) == BitArray("011110")
    with pytest.raises(TypeError):
        l | object()
    with pytest.raises(TypeError):
        object() | l
    with pytest.raises(ValueError):
        BitArray("") | BitArray("01")


def test_bit_array_xor():
    l = BitArray("001101")
    p = BitArray("011010")
    assert (l ^ p) == BitArray("010111")
    with pytest.raises(TypeError):
        l ^ object()
    with pytest.raises(TypeError):
        object() ^ l
    with pytest.raises(ValueError):
        BitArray("") ^ BitArray("01")


def test_bit_array_invert():
    assert ~BitArray("0110") == BitArray("1001")


def test_bit_array_literal_casts():
    assert str(BitArray("0101010")) == "0101010"


def test_equality():
    # fmt: off
    # cross product of all impls
    assert BitArray("0101", Range(0, 'to', 3)) == BitArray("0101", Range(0, 'to', 3))
    assert BitArray("0101", Range(0, 'to', 3)) == BitArray(0b0101, Range(0, 'to', 3))
    assert BitArray("0101", Range(0, 'to', 3)) == BitArray([0, 1, 0, 1], Range(0, 'to', 3))
    assert BitArray(0b0101, Range(0, 'to', 3)) == BitArray("0101", Range(0, 'to', 3))
    assert BitArray(0b0101, Range(0, 'to', 3)) == BitArray(0b0101, Range(0, 'to', 3))
    assert BitArray(0b0101, Range(0, 'to', 3)) == BitArray([0, 1, 0, 1], Range(0, 'to', 3))
    assert BitArray([0, 1, 0, 1], Range(0, 'to', 3)) == BitArray("0101", Range(0, 'to', 3))
    assert BitArray([0, 1, 0, 1], Range(0, 'to', 3)) == BitArray(0b0101, Range(0, 'to', 3))
    assert BitArray([0, 1, 0, 1], Range(0, 'to', 3)) == BitArray([0, 1, 0, 1], Range(0, 'to', 3))
    assert BitArray("0101", Range(0, 'to', 3)) == BitArray("0101", Range(7, 'downto', 4)) # equality works regardless of range
    assert BitArray("0101", Range(0, 'to', 3)) != BitArray("1010", Range(0, 'to', 3))  # wrong value same lengths
    assert BitArray("0101", Range(0, 'to', 3)) != BitArray("010101")  # different lengths
    # fmt: on
    assert BitArray("0101") == "0101"
    assert BitArray("0101") == [0, 1, 0, 1]
    BitArray("101") != object()


def test_repr_eval():
    r = BitArray("0011")
    assert eval(repr(r)) == r


def test_iter():
    val = [Bit(0), Bit(1), Bit("0"), Bit("1")]
    assert all(isinstance(v, Bit) for v in val)
    a = BitArray(val)
    assert list(a) == val


def test_reversed_bit_array():
    val = [Bit(0), Bit(1), Bit("1"), Bit("0")]
    a = BitArray(val)
    assert list(reversed(a)) == list(reversed(val))


def test_contains_bit_array():
    a = BitArray("00")
    assert Bit("0") in a
    assert Bit("1") not in a


def test_index():
    r = BitArray("0001101", Range(7, "downto", 1))
    assert r.index(Bit("1")) == 4
    assert r.index(Bit("1"), 2, 0) == 1
    with pytest.raises(ValueError):
        r.index(object())


def test_count_bit_array():
    assert BitArray("011010").count(Bit("1")) == 3


def test_indexing():
    a = BitArray("0101", Range(8, "to", 11))
    assert a[8] == "0"
    with pytest.raises(IndexError):
        a[0]
    a[11] = "0"
    assert a[11] == "0"

    b = BitArray("0011", Range(10, "downto", 7))
    assert b[8] == 1
    with pytest.raises(IndexError):
        b[-2]
    b[8] = 0
    assert b[8] == 0


def test_bad_indexing():
    with pytest.raises(TypeError):
        BitArray("0110")[[]]
    with pytest.raises(TypeError):
        BitArray("1010")[object()] = 9


def test_slicing():
    a = BitArray("01101100")
    b = a[5:1]
    assert b.left == 5
    assert b.right == 1
    assert b == BitArray("10110")
    a[3:0] = "0000"
    assert a == BitArray("01100000")
    a[7:4] = 0b1010
    assert a == BitArray("10100000")


def test_slicing_infered_start_stop():
    a = BitArray("1011")
    assert a[:] == a
    a[:] = "1010"
    assert a == BitArray("1010")


def test_dont_specify_step():
    with pytest.raises(IndexError):
        BitArray("1010")[::1]
    with pytest.raises(IndexError):
        BitArray("1010")[1:2:1] = [1, 2]


def test_set_slice_wrong_length():
    a = BitArray("000000")
    with pytest.raises(ValueError):
        a[4:2] = "0000000000000"


def test_slice_correct_infered():
    a = BitArray("1111")
    b = a[:3]
    assert b.right == 3


def test_null_vector():
    null_range = Range(-1, "downto", 0)
    assert len(null_range) == 0

    # test construction doesn't fail
    BitArray("")
    BitArray("", null_range)
    BitArray([])
    BitArray([], null_range)
    with pytest.raises(ValueError):
        BitArray(0, null_range)

    null_vector = BitArray("")

    # test attributes
    assert len(null_vector) == 0
    assert list(null_vector) == []
    assert str(null_vector) == ""

    # test comparison
    assert null_vector == BitArray("")
    assert null_vector == BitArray("", null_range)
    assert null_vector == BitArray([])
    assert null_vector == BitArray([], null_range)
    assert null_vector != 0
    assert null_vector != 1
    assert null_vector != -1
    assert null_vector == ""
    assert null_vector == []


def test_resolve():
    assert BitArray("01101").resolve("error") == BitArray("01101")
    assert BitArray("01101").resolve("weak") == BitArray("01101")
    assert BitArray("000100010").resolve("zeros") == BitArray("000100010")
    assert BitArray("000100010").resolve("ones") == BitArray("000100010")
    assert BitArray("000100010").resolve("random") == BitArray("000100010")
    array = BitArray("000100010").resolve("random")
    assert all(elem in (Logic("0"), Logic("1")) for elem in array)


def test_copy() -> None:
    l = BitArray("0011", Range(-2, "to", 1))

    c = copy.copy(l)
    assert l == c
    assert l.range == c.range

    d = copy.deepcopy(l)
    assert l == d
    assert l.range == d.range


def test_format_bit_array():
    l = BitArray("0110")
    assert f"{l}" == "0110"
    assert f"{l!s}" == "0110"
    assert f"{l!r}" == "BitArray('0110', Range(3, 'downto', 0))"

    l = BitArray("1010")
    assert f"{l:\0}" == "10"
    assert f"{l:d}" == "10"
    assert f"{l:b}" == "1010"
    assert f"{l:x}" == "a"
    assert f"{l:X}" == "A"
    assert f"{l:o}" == "12"

    with pytest.raises(ValueError):
        f"{l:Q}"

    l = BitArray("00001101001")
    assert f"{l:#_b}" == "0b000_0110_1001"
    assert f"{l:#x}" == "0x069"
    assert f"{l:#_X}" == "0X069"
    assert f"{l:#_o}" == "0o0_0151"
    assert f"{l:#,d}" == "0d0,105"


def test_direction_string():
    assert BitArray("1001").direction == "downto"


def test_index_invalid_bit_array():
    r = BitArray("0001101", Range(1, "to", 7))
    assert r.index(Logic("1"), 5, 7) == 5

    with pytest.raises(ValueError):
        r.index(Logic("1"), 5, 5)
