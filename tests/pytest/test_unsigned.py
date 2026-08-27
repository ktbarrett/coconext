from __future__ import annotations

import pytest

from coconext.types import Signed, Unsigned


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

    narrow_fit = Unsigned(4, int(small_val))
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


def test_arithmetic_operators_edge_cases():
    u_narrow = Unsigned(4, 15)  # Max 4-bit (1111)
    u_wide = Unsigned(32, 1000000)  # 32-bit

    sum_nw = u_narrow + u_wide
    assert len(sum_nw) == 33  # max(4, 32) + 1
    assert int(sum_nw) == 1000015

    sum_wn = u_wide + u_narrow
    assert len(sum_wn) == 33
    assert int(sum_wn) == 1000015

    sub_zero = u_narrow - Unsigned(8, 15)
    assert len(sub_zero) == 9  # max(4, 8) + 1
    assert int(sub_zero) == 0

    sub_wn = u_wide - u_narrow
    assert len(sub_wn) == 33
    assert int(sub_wn) == 999985

    sub_nw = u_narrow - u_wide
    assert len(sub_nw) == 33
    assert int(sub_nw) == 15 - 1000000

    u_zero = Unsigned(8, 0)
    u_one = Unsigned(1, 1)

    prod_zero = u_narrow * u_zero
    assert len(prod_zero) == 12  # 4 + 8
    assert int(prod_zero) == 0

    prod_one = u_wide * u_one
    assert len(prod_one) == 33  # 32 + 1
    assert int(prod_one) == 1000000

    prod_max = u_narrow * u_narrow
    assert len(prod_max) == 8  # 4 + 4
    assert int(prod_max) == 225  # 15 * 15

    div_large = u_narrow / u_wide
    assert int(div_large) == 0

    mod_large = u_narrow % u_wide
    assert int(mod_large) == 15

    mod_one = u_wide % u_one
    assert int(mod_one) == 0


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

    a += Unsigned(4, 5)
    assert int(a) == 15

    a -= Unsigned(5, 3)
    assert int(a) == 12

    a *= Unsigned(2, 2)
    assert int(a) == 24

    a -= Unsigned(9, 32)
    assert int(a) == 248

    a /= Unsigned(4, 4)
    assert int(a) == 62

    a //= Unsigned(4, 2)
    assert int(a) == 31

    a %= Unsigned(4, 4)
    assert int(a) == 3


def test_compound_assignment_harsh():
    a = Unsigned(4, 15)
    a += Unsigned(8, 2)  # 15 + 2 = 17 % 16 = 1
    assert int(a) == 1
    assert len(a) == 4

    b = Unsigned(8, 5)
    b -= Unsigned(16, 10)  # 5 - 10 = -5
    assert int(b) == 251  # -5 in 8-bit unsigned = 251
    assert len(b) == 8

    c = Unsigned(4, 5)
    c *= Unsigned(16, 1000)  # 5 * 1000 = 5000
    assert int(c) == 8  # 5000 % 16 = 8
    assert len(c) == 4

    d = Unsigned(32, 100)
    d += Unsigned(4, 15)
    assert int(d) == 115
    assert len(d) == 32

    e = Unsigned(8, 128)
    e *= Unsigned(4, 0)
    assert int(e) == 0

    e += Unsigned(8, 255)  # 0 + 255
    assert int(e) == 255

    e /= Unsigned(2, 1)
    assert int(e) == 255

    e %= Unsigned(8, 255)
    assert int(e) == 0

    f = Unsigned(8, 250)
    f += 10  # 260
    assert int(f) == 4  # 260 % 256 = 4

    f -= 10  # 4 - 10 = -6. -6 in 8-bit unsigned = 250
    assert int(f) == 250

    f *= 2  # 250 * 2 = 500. 500 % 256 = 244
    assert int(f) == 244

    with pytest.raises(ValueError):
        f /= 0

    with pytest.raises(ValueError):
        f %= 0

    with pytest.raises(OverflowError):
        f += -5

    f += 10
    assert int(f) == 254

    with pytest.raises(ValueError):
        f //= 0


def test_compound_assignment_operators_mixed_signedness():
    u1 = Unsigned(8, 15)
    u1 += Signed(8, -5)
    assert u1 == Unsigned(8, 10)

    u2 = Unsigned(8, 250)
    u2 += Signed(8, 10)
    assert u2 == Unsigned(8, 4)

    u3 = Unsigned(8, 5)
    u3 -= Signed(8, 10)
    assert u3 == Unsigned(8, 251)

    u4 = Unsigned(8, 10)
    u4 *= Signed(8, -3)
    assert u4 == Unsigned(8, 226)

    u5 = Unsigned(8, 20)
    u5 //= Signed(8, -4)
    assert u5 == Unsigned(8, 251)

    u6 = Unsigned(8, 23)
    u6 %= Signed(8, -7)
    assert u6 == Unsigned(8, 2)

    u7 = Unsigned(8, 50)
    with pytest.raises(ValueError):
        u7 //= Signed(8, 0)
    with pytest.raises(ValueError):
        u7 %= Signed(8, 0)


def test_compound_assignment_mixed_signedness_harsh():
    u_narrow = Unsigned(4, 5)
    u_narrow += Signed(32, -20)  # 5 + (-20) = -15
    assert int(u_narrow) == 1  # 4-bit unsigned wrap: 16 - 15 = 1
    assert len(u_narrow) == 4

    u_wide = Unsigned(32, 10)
    u_wide -= Signed(4, -8)  # 10 - (-8) = 18
    assert int(u_wide) == 18
    assert len(u_wide) == 32

    u_mult = Unsigned(8, 10)
    # 10 * -500 = -5000. In 8-bit unsigned: -5000 % 256 = 120
    u_mult *= Signed(32, -500)
    assert int(u_mult) == 120

    u_div = Unsigned(8, 250)
    # 250 / -60 = -4. Wrap to 8-bit unsigned: 256 - 4 = 252
    u_div //= Signed(16, -60)
    assert int(u_div) == 252

    u_mod = Unsigned(8, 250)
    # Dividend is positive -> Remainder is positive (250 = -4 * -60 + 10)
    u_mod %= Signed(16, -60)
    assert int(u_mod) == 10

    u_zero = Unsigned(8, 100)
    with pytest.raises(ValueError):
        u_zero //= Signed(32, 0)
    with pytest.raises(ValueError):
        u_zero %= Signed(32, 0)


def test_shift_operators():
    a = Unsigned(8, 5)

    sl = a << 2
    assert int(sl) == 20

    sr = a >> 1
    assert int(sr) == 2

    assert int(a << 8) == 0
    assert int(a >> 10) == 0

    a <<= 3
    assert int(a) == 40
    a >>= 2
    assert int(a) == 10

    with pytest.raises(TypeError):
        _ = a << -1
    with pytest.raises(TypeError):
        _ = a >> -2

    shift_amt = Unsigned(4, 2)
    assert int(a << shift_amt) == 40


def test_shift_operators_harsh_edge_cases():
    u_8 = Unsigned(8, 0b10101010)  # 170 in decimal

    assert int(u_8 << 7) == 0  # LSB was 0 -> shifted to MSB -> 0
    u_8_b = Unsigned(8, 1)
    assert int(u_8_b << 7) == 128  # LSB was 1 -> shifted to MSB -> 128

    assert int(u_8 << 8) == 0
    assert int(u_8 >> 8) == 0

    assert int(u_8 << 1000) == 0
    assert int(u_8 >> 1000) == 0

    shift_s_pos = Signed(8, 3)
    assert int(u_8 << shift_s_pos) == 80
    assert int(u_8 >> shift_s_pos) == 21  # 170 >> 3 = 21

    shift_s_huge = Signed(32, 50000)
    assert int(u_8 << shift_s_huge) == 0

    shift_s_neg = Signed(8, -2)
    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = u_8 << shift_s_neg
    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = u_8 >> shift_s_neg

    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = u_8 << Signed(32, -100)

    shift_u_huge = Unsigned(64, 9999999)
    assert int(u_8 << shift_u_huge) == 0
    assert int(u_8 >> shift_u_huge) == 0

    shift_u_zero = Unsigned(4, 0)
    shift_s_zero = Signed(4, 0)
    assert int(u_8 << shift_u_zero) == 170
    assert int(u_8 >> shift_s_zero) == 170
    assert int(u_8 << 0) == 170

    u_comp = Unsigned(4, 15)  # 1111 (binary)
    u_comp <<= 2  # 111100 -> truncated to 4 bits -> 1100 (12)
    assert int(u_comp) == 12
    assert len(u_comp) == 4  # Width MUST NOT grow

    u_comp <<= Signed(16, 2)  # 110000 -> truncated to 4 bits -> 0000 (0)
    assert int(u_comp) == 0
    assert len(u_comp) == 4

    u_comp2 = Unsigned(8, 255)
    u_comp2 >>= Unsigned(8, 4)  # 00001111 (15)
    assert int(u_comp2) == 15
    assert len(u_comp2) == 8

    u_128 = Unsigned(128, 550059)

    u_128 >>= Unsigned(40, 500)
    assert int(u_128) == 0

    with pytest.raises(TypeError):
        _ = u_8 << -1
    with pytest.raises(TypeError):
        u_8 >>= -10


def test_index_operator():
    a = Unsigned(4, 2)  # 0010

    assert not bool(a[3])
    assert not bool(a[2])
    assert bool(a[1])
    assert not bool(a[0])

    with pytest.raises(IndexError):
        _ = a[4]


def test_formatter():
    small = Unsigned(10, 102)
    mid = Unsigned(39, 0x0AFFFE9001)

    assert format(small, "b") == "Unsigned[9 downto 0]{0001100110}"
    assert (
        format(mid, "b")
        == "Unsigned[38 downto 0]{000101011111111111111101001000000000001}"
    )

    assert format(small) == "Unsigned[9 downto 0]{102}"
    assert format(mid) == "Unsigned[38 downto 0]{47244546049}"

    assert format(small, "o") == "Unsigned[9 downto 0]{0146}"
    assert format(mid, "o") == "Unsigned[38 downto 0]{0537777510001}"

    assert format(small, "x") == "Unsigned[9 downto 0]{066}"
    assert format(mid, "x") == "Unsigned[38 downto 0]{0afffe9001}"


def test_unary_ops():
    a = Unsigned(8, 150)
    neg_a = -a

    assert type(neg_a) is Signed
    assert len(neg_a) == 9
    assert int(neg_a) == -150

    b = Unsigned(4, 5)
    neg_b = -b
    assert type(neg_b) is Signed
    assert len(neg_b) == 5
    assert int(neg_b) == -5

    pos_a = +a
    assert type(pos_a) is Signed
    assert len(pos_a) == 9
    assert int(pos_a) == 150


def test_zero_width():
    with pytest.raises(ValueError):
        _ = Unsigned(0, 0)
