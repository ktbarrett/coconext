from __future__ import annotations

import pytest

from coconext.types import Unsigned


def test_constructors():
    a = Unsigned(4, 15)
    assert int(a) == 15
    assert len(a) == 4

    # Out of range / overflow checks
    with pytest.raises(OverflowError):
        Unsigned(4, 16)
    with pytest.raises(OverflowError):
        Unsigned(4, -1)

    large_val = Unsigned(8, 200)
    small_val = Unsigned(8, 10)

    narrow_fit = Unsigned(4, int(small_val))  # Emulating narrow_fit(small_val)
    assert int(narrow_fit) == 10

    with pytest.raises(OverflowError):
        Unsigned(4, int(large_val))


def test_explicit_native_casts():
    a = Unsigned(16, 42000)

    assert bool(a) is True
    assert bool(Unsigned(16, 0)) is False

    assert int(a) == 42000


def test_arithmetic_operators():
    a = Unsigned(8, 150)
    b = Unsigned(8, 50)

    sum_res = a + b
    assert type(sum_res) is Unsigned
    assert len(sum_res) == 9
    assert int(sum_res) == 200

    prod_res = a * b
    assert type(prod_res) is Unsigned
    assert len(prod_res) == 16
    assert int(prod_res) == 7500

    div_res = a / b
    assert int(div_res) == 3

    mod_res = a % Unsigned(4, 7)
    assert int(mod_res) == 150 % 7

    with pytest.raises(ValueError):
        _ = a // Unsigned(4, 0)

    with pytest.raises(ValueError):
        _ = a % Unsigned(8, 0)

    sub_pos = a - b
    sub_neg = b - a
    assert len(sub_pos) == 9
    assert len(sub_neg) == 9
    assert int(sub_pos) == 100
    assert int(sub_neg) == -100


# def test_compound_assignment_operators_mixed_signedness():
#     u1 = Unsigned(8, 15)
#     u1 += Signed(8, -5)
#     assert u1 == Unsigned(8, 10)

#     u2 = Unsigned(8, 250)
#     u2 += Signed(8, 10)
#     assert u2 == Unsigned(8, 4)

#     u3 = Unsigned(8, 5)
#     u3 -= Signed(8, 10)
#     assert u3 == Unsigned(8, 251)

#     u4 = Unsigned(8, 10)
#     u4 *= Signed(8, -3)
#     assert u4 == Unsigned(8, 226)

#     u5 = Unsigned(8, 20)
#     u5 //= Signed(8, -4)
#     assert u5 == Unsigned(8, 251)

#     u6 = Unsigned(8, 23)
#     u6 %= Signed(8, -7)
#     assert u6 == Unsigned(8, 2)

#     u7 = Unsigned(8, 50)
#     with pytest.raises(ValueError):
#         u7 //= Signed(8, 0)
#     with pytest.raises(ValueError):
#         u7 %= Signed(8, 0)


# def test_as_overloads():
#     arr_a = BitArray("01001")
#     a = Unsigned(5, arr_a)
#     arr_exp = BitArray(a)

#     assert type(a) is Unsigned
#     assert int(a) == 9
#     assert str(arr_a) == str(arr_exp)


# def test_resize_overloads():
#     small = Unsigned(8, 200)

#     wide = small.resize(16)
#     assert type(wide) is Unsigned
#     assert len(wide) == 16
#     assert int(wide) == 200

#     wide_val = Unsigned(16, 1000)

#     narrow_wrap = wide_val.resize(8)
#     assert type(narrow_wrap) is Unsigned
#     assert len(narrow_wrap) == 8
#     assert int(narrow_wrap) == 232

#     # Assuming overflow_mode="saturate" is valid in your Python bindings
#     narrow_sat = wide_val.resize(8, overflow_mode=OverflowMode.Saturate)
#     assert int(narrow_sat) == 255


def test_comparisons():
    a = Unsigned(8, 10)
    b = Unsigned(8, 10)
    c = Unsigned(8, 20)
    d = Unsigned(8, 5)

    assert a == b
    assert not (a == c)

    assert a != c
    assert not (a != b)

    assert d < a
    assert not (a < d)
    assert not (a < b)

    assert d <= a
    assert a <= b
    assert not (c <= a)

    assert c > a
    assert not (a > c)
    assert not (a > b)

    assert c >= a
    assert a >= b
    assert not (d >= a)


def test_compound_assignment():
    a = Unsigned(8, 10)
    b = Unsigned(4, 5)

    a += b
    assert int(a) == 15

    # a -= Unsigned(8, 3)
    # assert int(a) == 12

    # a *= Unsigned(2, 2)
    # assert int(a) == 24

    # a //= Unsigned(4, 4)
    # assert int(a) == 6

    # a %= Unsigned(4, 4)
    # assert int(a) == 2

    # a += 10
    # assert int(a) == 12

    # with pytest.raises(ZeroDivisionError):
    #     a //= 0


# def test_increment_decrement():
#     # Python has no ++a or a++, translating to += 1 and -= 1
#     a = Unsigned(8, 10)

#     a += 1
#     assert int(a) == 11

#     a -= 1
#     assert int(a) == 10

#     max_val = Unsigned(4, 15)
#     max_val += 1
#     assert int(max_val) == 0

#     min_val = Unsigned(4, 0)
#     min_val -= 1
#     assert int(min_val) == 15


# def test_shift_operators():
#     a = Unsigned(8, 5)

#     sl = a << 2
#     assert int(sl) == 20

#     sr = a >> 1
#     assert int(sr) == 2

#     assert int(a << 8) == 0
#     assert int(a >> 10) == 0

#     a <<= 3
#     assert int(a) == 40
#     a >>= 2
#     assert int(a) == 10

#     with pytest.raises(ValueError):
#         _ = a << -1
#     with pytest.raises(ValueError):
#         _ = a >> -2

#     shift_amt = Unsigned(4, 2)
#     assert int(a << shift_amt) == 40


# def test_iterators():
#     a = Unsigned(4, 8)  # 1000

#     bit_vals = [1 if bool(bit) else 0 for bit in a]
#     assert len(bit_vals) == 4
#     assert bit_vals == [1, 0, 0, 0]

#     rbit_vals = [1 if bool(bit) else 0 for bit in reversed(a)]
#     expected_rvals = [0, 0, 0, 1]
#     assert rbit_vals == expected_rvals

#     # Ensure reversing rbit_vals brings it back to bit_vals
#     assert list(reversed(rbit_vals)) == bit_vals


# def test_index_operator():
#     a = Unsigned(4, 2)  # 0010

#     assert not bool(a[3])
#     assert not bool(a[2])
#     assert bool(a[1])
#     assert not bool(a[0])

#     with pytest.raises(IndexError):
#         _ = a[4]


# def test_bitwise_ops():
#     a = Unsigned(8, 10)
#     b = Unsigned(8, 10)

#     and_result = a & b
#     # Depending on your binding, returning BitArray or Unsigned
#     assert str(and_result) == str(BitArray("00001010"))


# def test_const_udl_equivalents():
#     # Validating standard widths and boundaries dynamically
#     assert int(Unsigned(8, 0)) == 0
#     assert int(Unsigned(8, 5)) == 5
#     assert int(Unsigned(8, 255)) == 255

#     assert int(Unsigned(16, 0)) == 0
#     assert int(Unsigned(16, 65535)) == 65535

#     assert int(Unsigned(32, 0)) == 0
#     assert int(Unsigned(32, 4294967295)) == 4294967295

#     assert int(Unsigned(64, 0)) == 0
#     assert int(Unsigned(64, 18446744073709551615)) == 18446744073709551615

#     assert Unsigned(8, 42) == Unsigned(8, 42)
#     assert Unsigned(16, 1024) == Unsigned(16, 1024)


# def test_formatter():
#     small = Unsigned(10, 102)
#     mid = Unsigned(39, 0x0AFFFE9001)

#     chunk1 = Unsigned(139, 0x0AFFFE9001)
#     chunk2 = Unsigned(139, 0x0AFFFE9001)
#     chunk3 = Unsigned(139, 0x0AFFFE9001)
#     chunk4 = Unsigned(139, 0xFFFFF)

#     very_large = (((chunk1 << 100) | (chunk2 << 60)) | (chunk3 << 20)) | chunk4

#     assert format(small, "b") == "Unsigned[9 downto 0]{0001100110}"
#     assert format(mid, "b") == "Unsigned[38 downto 0]{000101011111111111111101001000000000001}"
#     assert format(very_large, "b") == "Unsigned[138 downto 0]{0001010111111111111111010010000000000010000101011111111111111101001000000000001000010101111111111111101001000000000011111111111111111111}"

#     assert format(small) == "Unsigned[9 downto 0]{102}"
#     assert format(mid) == "Unsigned[38 downto 0]{47244546049}"
#     assert format(very_large) == "Unsigned[138 downto 0]{59889577156579543121862034195167783682047}"

#     assert format(small, "o") == "Unsigned[9 downto 0]{0146}"
#     assert format(mid, "o") == "Unsigned[38 downto 0]{0537777510001}"
#     assert format(very_large, "o") == "Unsigned[138 downto 0]{01277777220002053777751000102577776440007777777}"

#     assert format(small, "x") == "Unsigned[9 downto 0]{066}"
#     assert format(mid, "x") == "Unsigned[38 downto 0]{0afffe9001}"
#     assert format(very_large, "x") == "Unsigned[138 downto 0]{0afffe90010afffe90010afffe9001fffff}"


# def test_hash_determinism_and_collisions():
#     a = Unsigned(10, 102)
#     b = Unsigned(10, 102)
#     val2 = Unsigned(10, 103)
#     val3 = Unsigned(20, 102)

#     assert hash(a) == hash(a)
#     assert hash(a) == hash(b)

#     assert hash(a) != hash(val2)
#     assert hash(a) != hash(val3)


# def test_hash_unordered_set_integration():
#     a = Unsigned(10, 10)
#     b = Unsigned(10, 20)
#     a_copy = Unsigned(10, 10)

#     hash_set = {a, b, a_copy}

#     assert len(hash_set) == 2
#     assert a in hash_set
#     assert b in hash_set

#     c = Unsigned(10, 30)
#     assert c not in hash_set


# def test_unary_ops():
#     a = Unsigned(8, 150)
#     neg_a = -a

#     assert type(neg_a) is Signed
#     assert len(neg_a) == 9
#     assert int(neg_a) == -150

#     b = Unsigned(4, 5)
#     neg_b = -b
#     assert type(neg_b) is Signed
#     assert len(neg_b) == 5
#     assert int(neg_b) == -5

#     pos_a = +a
#     assert type(pos_a) is Signed
#     assert len(pos_a) == 9
#     assert int(pos_a) == 150


# def test_zero_width():
#     a = Unsigned(0, 0)
#     b = Unsigned(0, 0)

#     assert a == b
#     assert not (a != b)

#     assert bool(a) is False

#     c = Unsigned(0, 0)
#     c += 1
#     assert c == Unsigned(0, 0)
#     c -= 1
#     assert c == Unsigned(0, 0)
#     c += 5
#     assert c == Unsigned(0, 0)

#     sum_res = a + b
#     assert type(sum_res) is Unsigned
#     assert len(sum_res) == 1
#     assert int(sum_res) == 0

#     assert len(list(a)) == 0
#     assert len(a) == 0

#     assert format(a, "b") == "Unsigned[-1 downto 0]{}"
#     assert format(a, "d") == "Unsigned[-1 downto 0]{}"


# def test_resize_across_the_tier_boundary():
#     wide = Unsigned(200, 12345)

#     assert int(wide.resize(32)) == 12345
#     assert int(wide.resize(16, overflow_mode=OverflowMode.Wrap)) == 12345

#     narrow = Unsigned(8, 200)
#     grown = narrow.resize(200)
#     assert format(grown, "d") == "Unsigned[199 downto 0]{200}"

#     assert int(narrow.resize(200).resize(8)) == 200


# def test_saturation_clamps_from_the_wide_tier():
#     big = Unsigned(200, int("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFF", 16))
#     assert int(big.resize(8, overflow_mode=OverflowMode.Saturate)) == 255
#     assert int(big.resize(16, overflow_mode=OverflowMode.Saturate)) == 65535

#     small = Unsigned(200, 42)
#     assert int(small.resize(8, overflow_mode=OverflowMode.Saturate)) == 42

#     v = Unsigned(200, int("0x123456789ABCDEF0123456789", 16))
#     assert int(v.resize(16, overflow_mode=OverflowMode.Wrap)) == 26505


# def test_wide_arithmetic_still_grows():
#     a = Unsigned(104, int("0xFEDCBA98765432100123456789", 16))
#     b = Unsigned(104, 1000)

#     sum_res = a + b
#     assert type(sum_res) is Unsigned
#     assert len(sum_res) == 105
#     assert format(sum_res, "d") == "Unsigned[104 downto 0]{20192265560968774111035004382065}"
