from __future__ import annotations

import pytest

from coconext.types import Signed, Unsigned


def test_constructors():
    # 4-bit Signed range: -8 to 7
    a = Signed(4, 7)
    assert int(a) == 7
    assert len(a) == 4

    b = Signed(4, -8)
    assert int(b) == -8
    assert len(b) == 4

    with pytest.raises(OverflowError):
        Signed(4, 8)
    with pytest.raises(OverflowError):
        Signed(4, -9)

    large_pos = Signed(8, 120)
    large_neg = Signed(8, -100)
    small_pos = Signed(8, 5)
    small_neg = Signed(8, -4)

    narrow_fit_pos = Signed(4, int(small_pos))
    assert int(narrow_fit_pos) == 5

    narrow_fit_neg = Signed(4, int(small_neg))
    assert int(narrow_fit_neg) == -4

    with pytest.raises(OverflowError):
        Signed(4, int(large_pos))
    with pytest.raises(OverflowError):
        Signed(4, int(large_neg))


def test_explicit_native_casts():
    a = Signed(16, -32000)

    assert bool(a) is True
    assert bool(Signed(16, 0)) is False

    assert int(a) == -32000


def test_arithmetic_operators():
    a = Signed(8, 100)
    b = Signed(8, -50)

    sum_res = a + b
    assert type(sum_res) is Signed
    assert len(sum_res) == 9
    assert int(sum_res) == 50

    prod_res = a * b
    assert type(prod_res) is Signed
    assert len(prod_res) == 16
    assert int(prod_res) == -5000

    div_res = a / b
    assert int(div_res) == -2

    mod_res = a % Signed(4, 7)
    assert int(mod_res) == 100 % 7

    with pytest.raises(ValueError):
        _ = a // Signed(4, 0)

    with pytest.raises(ValueError):
        _ = a % Signed(8, 0)

    sub_pos = a - b
    sub_neg = b - a

    assert len(sub_pos) == 9
    assert len(sub_neg) == 9
    assert int(sub_pos) == 150
    assert int(sub_neg) == -150


def test_arithmetic_operators_edge_cases():
    s_narrow = Signed(4, -5)
    s_wide = Signed(32, 1000000)

    sum_nw = s_narrow + s_wide
    assert len(sum_nw) == 33  # max(4, 32) + 1
    assert int(sum_nw) == 999995

    sum_wn = s_wide + s_narrow
    assert len(sum_wn) == 33
    assert int(sum_wn) == 999995

    sub_zero = s_narrow - Signed(8, -5)
    assert len(sub_zero) == 9  # max(4, 8) + 1
    assert int(sub_zero) == 0

    sub_wn = s_wide - s_narrow
    assert len(sub_wn) == 33
    assert int(sub_wn) == 1000005

    sub_nw = s_narrow - s_wide
    assert len(sub_nw) == 33
    assert int(sub_nw) == -1000005

    s_zero = Signed(8, 0)
    s_neg_one = Signed(1, -1)

    prod_zero = s_narrow * s_zero
    assert len(prod_zero) == 12  # 4 + 8
    assert int(prod_zero) == 0

    prod_neg_one = s_wide * s_neg_one
    assert len(prod_neg_one) == 33  # 32 + 1
    assert int(prod_neg_one) == -1000000

    prod_max = s_narrow * s_narrow
    assert len(prod_max) == 8  # 4 + 4
    assert int(prod_max) == 25  # -5 * -5

    div_large = s_narrow / s_wide
    assert int(div_large) == -1  # -5 // 1000000 is -1 in Python

    mod_large = s_narrow % s_wide
    assert int(mod_large) == 999995  # -5 % 1000000 = 999995

    mod_one = s_wide % s_neg_one
    assert int(mod_one) == 0


def test_compound_assignment():
    a = Signed(8, 10)

    a += Signed(4, -5)
    assert len(a) == 8
    assert int(a) == 5

    a -= Signed(5, -3)
    assert len(a) == 8
    assert int(a) == 8

    a *= Signed(2, -2)
    assert len(a) == 8
    assert int(a) == -16

    a -= Signed(9, 32)
    assert len(a) == 8
    assert int(a) == -48

    a /= Signed(4, 4)
    assert len(a) == 8
    assert int(a) == -12

    a //= Signed(4, 2)
    assert len(a) == 8
    assert int(a) == -6

    a %= Signed(4, 4)
    assert len(a) == 8
    assert int(a) == 2


def test_compound_assignment_harsh():
    # Wrap-around boundary tests
    a = Signed(4, 7)
    a += Signed(8, 2)  # 7 + 2 = 9. 4-bit signed wraps to -7
    assert len(a) == 4
    assert int(a) == -7

    b = Signed(8, -125)
    b -= Signed(16, 10)  # -125 - 10 = -135. 8-bit signed wraps to 121
    assert int(b) == 121
    assert len(b) == 8

    z = Signed(5, -5)
    z *= Signed(16, 3)  # -15. Fits in 5-bit signed (-16 to 15)
    assert len(z) == 5
    assert int(z) == -15

    c = Signed(4, -5)
    c *= Signed(16, 3)  # -15. Wraps around in 4-bit signed (-8 to 7)
    assert len(c) == 4
    assert int(c) == 1  # -15 % 16 = 1

    d = Signed(32, -100)
    d += Signed(4, -1)
    assert int(d) == -101
    assert len(d) == 32

    e = Signed(8, -128)
    e *= Signed(4, 0)
    assert int(e) == 0

    e += Signed(8, 127)  # 0 + 127
    assert int(e) == 127

    e /= Signed(2, -1)
    assert int(e) == -127

    e %= Signed(8, 127)
    assert int(e) == 0

    f = Signed(8, 120)
    f += 10  # 130 wraps to -126 in 8-bit signed
    assert int(f) == -126

    f -= 10  # -126 - 10 = -136 wraps to 120
    assert int(f) == 120

    f *= -2  # -240 wraps to 16
    assert int(f) == 16

    with pytest.raises(ValueError):
        f /= 0

    with pytest.raises(ValueError):
        f %= 0

    f += -20  # Valid for signed
    assert int(f) == -4

    with pytest.raises(ValueError):
        f //= 0


def test_compound_assignment_operators_mixed_signedness():
    s1 = Signed(8, -5)
    s1 += Unsigned(8, 15)
    assert s1 == Signed(8, 10)

    s2 = Signed(8, 120)
    s2 += Unsigned(8, 10)  # 130 wraps to -126
    assert s2 == Signed(8, -126)

    s3 = Signed(8, 5)
    s3 -= Unsigned(8, 10)
    assert s3 == Signed(8, -5)

    s4 = Signed(8, -10)
    s4 *= Unsigned(8, 3)
    assert s4 == Signed(8, -30)

    s5 = Signed(8, -20)
    s5 //= Unsigned(8, 4)
    assert len(s5) == 8
    assert int(s5) == -5

    s6 = Signed(8, -23)
    s6 %= Unsigned(8, 7)
    assert len(s6) == 8
    assert int(s6) == 5

    s7 = Signed(8, -50)
    with pytest.raises(ValueError):
        s7 //= Unsigned(8, 0)
    with pytest.raises(ValueError):
        s7 %= Unsigned(8, 0)


def test_compound_assignment_mixed_signedness_harsh():
    s_narrow = Signed(4, -5)
    s_narrow += Unsigned(32, 20)  # -5 + 20 = 15. 4-bit signed wraps to -1
    assert int(s_narrow) == -1
    assert len(s_narrow) == 4

    s_wide = Signed(32, -10)
    s_wide -= Unsigned(4, 15)  # -10 - 15 = -25
    assert int(s_wide) == -25
    assert len(s_wide) == 32

    s_mult = Signed(8, -10)
    # -10 * 500 = -5000. In 8-bit signed: -5000 % 256 = 120
    s_mult *= Unsigned(32, 500)
    assert int(s_mult) == 120

    s_div = Signed(8, -120)
    s_div //= Unsigned(16, 60)
    assert int(s_div) == -2

    s_mod = Signed(8, -120)
    # Dividend is negative, Divisor is positive -> Remainder is positive
    s_mod %= Unsigned(16, 60)
    assert int(s_mod) == 0

    s_zero = Signed(8, 100)
    with pytest.raises(ValueError):
        s_zero //= Unsigned(32, 0)
    with pytest.raises(ValueError):
        s_zero %= Unsigned(32, 0)


def test_comparisons():
    a = Signed(8, -10)
    b = Signed(8, -10)
    c = Signed(8, 20)
    d = Signed(8, -20)

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


def test_shift_operators():
    a = Signed(8, -5)

    sl = a << 2
    assert int(sl) == -20

    sr = a >> 1
    assert int(sr) == -3  # Arithmetic shift: -5 // 2 = -3

    # Left shift clears out, right shift replicates sign bit
    assert int(a << 8) == 0
    assert int(a >> 10) == -1

    a <<= 3
    assert int(a) == -40
    a >>= 2
    assert int(a) == -10

    with pytest.raises(TypeError):
        _ = a << -1
    with pytest.raises(TypeError):
        _ = a >> -2

    shift_amt = Unsigned(4, 2)
    assert int(a << shift_amt) == -40


def test_shift_operators_harsh_edge_cases():
    s_8 = Signed(8, -86)  # 0b10101010

    assert int(s_8 << 7) == 0  # LSB was 0 -> shifted to MSB -> 0
    s_8_b = Signed(8, -127)  # 0b10000001
    assert int(s_8_b << 7) == -128  # LSB was 1 -> shifted to MSB -> -128

    assert int(s_8 << 8) == 0
    assert int(s_8 >> 8) == -1  # Sign extension replicated

    assert int(s_8 << 1000) == 0
    assert int(s_8 >> 1000) == -1

    shift_s_pos = Signed(8, 3)
    assert int(s_8 << shift_s_pos) == 80  # 0b10101010 << 3 = 01010000 = 80
    assert int(s_8 >> shift_s_pos) == -11  # -86 // 8 = -11

    shift_s_huge = Signed(32, 50000)
    assert int(s_8 << shift_s_huge) == 0

    shift_s_neg = Signed(8, -2)
    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = s_8 << shift_s_neg
    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = s_8 >> shift_s_neg

    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = s_8 << Signed(32, -100)

    shift_u_huge = Unsigned(64, 9999999)
    assert int(s_8 << shift_u_huge) == 0
    assert int(s_8 >> shift_u_huge) == -1

    shift_u_zero = Unsigned(4, 0)
    shift_s_zero = Signed(4, 0)
    assert int(s_8 << shift_u_zero) == -86
    assert int(s_8 >> shift_s_zero) == -86
    assert int(s_8 << 0) == -86

    s_comp = Signed(4, -5)  # 1011 (binary)
    s_comp <<= 2  # 101100 -> truncated to 4 bits -> 1100 (-4)
    assert int(s_comp) == -4
    assert len(s_comp) == 4  # Width MUST NOT grow

    s_comp <<= Signed(16, 2)  # 110000 -> truncated to 4 bits -> 0000 (0)
    assert int(s_comp) == 0
    assert len(s_comp) == 4

    s_comp2 = Signed(8, -1)  # 11111111 (-1)
    s_comp2 >>= Unsigned(8, 4)  # Sign extended right shift leaves it as -1
    assert int(s_comp2) == -1
    assert len(s_comp2) == 8

    s_128 = Signed(128, -550059)

    s_128 >>= Unsigned(40, 500)
    assert int(s_128) == -1

    with pytest.raises(TypeError):
        _ = s_8 << -1
    with pytest.raises(TypeError):
        s_8 >>= -10


def test_index_operator():
    a = Signed(4, -6)  # 1010 in 2's complement

    assert bool(a[3])  # Sign bit is 1
    assert not bool(a[2])
    assert bool(a[1])
    assert not bool(a[0])

    with pytest.raises(IndexError):
        _ = a[4]


def test_formatter():
    # Negative value tests 2's complement representation
    small = Signed(10, -102)  # 1024 - 102 = 922 -> 1110011010
    mid = Signed(39, 0x0AFFFE9001)

    assert format(small, "b") == "Signed[9 downto 0]{1110011010}"
    assert (
        format(mid, "b")
        == "Signed[38 downto 0]{000101011111111111111101001000000000001}"
    )

    assert format(small) == "Signed[9 downto 0]{-102}"
    assert format(mid) == "Signed[38 downto 0]{47244546049}"

    assert format(small, "o") == "Signed[9 downto 0]{1632}"
    assert format(mid, "o") == "Signed[38 downto 0]{0537777510001}"

    assert format(small, "x") == "Signed[9 downto 0]{39a}"
    assert format(mid, "x") == "Signed[38 downto 0]{0afffe9001}"


def test_unary_ops():
    a = Signed(8, -100)
    neg_a = -a

    assert type(neg_a) is Signed
    assert len(neg_a) == 9
    assert int(neg_a) == 100

    b = Signed(4, 5)
    neg_b = -b
    assert type(neg_b) is Signed
    assert len(neg_b) == 5
    assert int(neg_b) == -5

    pos_a = +a
    assert type(pos_a) is Signed
    assert len(pos_a) == 8
    assert int(pos_a) == -100


def test_zero_width():
    with pytest.raises(ValueError):
        _ = Signed(0, 0)
