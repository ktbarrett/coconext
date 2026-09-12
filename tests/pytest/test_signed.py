from __future__ import annotations

import pytest

from coconext.types import Signed, Unsigned


def test_constructors():
    # 4-bit Signed range: -8 to 7
    a = Signed(7, 4)
    assert int(a) == 7
    assert len(a) == 4

    b = Signed(-8, 4)
    assert int(b) == -8
    assert len(b) == 4

    with pytest.raises(OverflowError):
        Signed(8, 4)
    with pytest.raises(OverflowError):
        Signed(-9, 4)

    large_pos = Signed(120, 8)
    large_neg = Signed(-100, 8)
    small_pos = Signed(5, 8)
    small_neg = Signed(-4, 8)

    narrow_fit_pos = Signed(int(small_pos), 4)
    assert int(narrow_fit_pos) == 5

    narrow_fit_neg = Signed(int(small_neg), 4)
    assert int(narrow_fit_neg) == -4

    with pytest.raises(OverflowError):
        Signed(int(large_pos), 4)
    with pytest.raises(OverflowError):
        Signed(int(large_neg), 4)

    very_large = Signed(-1, 132)
    assert (
        format(very_large, "x")
        == "Signed[131 downto 0]{fffffffffffffffffffffffffffffffff}"
    )


def test_explicit_native_casts():
    a = Signed(-32000, 16)
    assert bool(a) is True
    assert int(a) == -32000

    b = Signed(-32000, 16)
    assert bool(b) is True
    assert int(b) == -32000

    assert bool(Signed(0, 16)) is False
    assert bool(Signed(1, 16)) is True

    assert bool(Signed(0, 160)) is False
    assert bool(Signed(3, 150)) is True


def test_arithmetic_operators():
    a = Signed(100, 8)
    b = Signed(-50, 8)

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

    mod_res = a % Signed(7, 4)
    assert int(mod_res) == 100 % 7

    with pytest.raises(ValueError):
        _ = a // Signed(0, 4)

    with pytest.raises(ValueError):
        _ = a % Signed(0, 8)

    sub_pos = a - b
    sub_neg = b - a

    assert len(sub_pos) == 9
    assert len(sub_neg) == 9
    assert int(sub_pos) == 150
    assert int(sub_neg) == -150


def test_arithmetic_operators_big():
    val_a = 68056473384187692692674921486353642291
    val_b = -68056473384187692692674921486353642292

    a = Signed(val_a, 180)
    b = Signed(val_b, 180)

    sum_res = a + b
    assert type(sum_res) is Signed
    assert len(sum_res) == 181
    assert int(sum_res) == val_a + val_b

    prod_res = a * b
    assert type(prod_res) is Signed
    assert len(prod_res) == 360
    assert int(prod_res) == val_a * val_b

    div_res = a / b
    assert type(div_res) is Signed
    assert len(div_res) == 181
    assert int(div_res) == val_a // val_b

    mod_res = a % Signed(7, 4)
    assert type(div_res) is Signed
    assert len(div_res) == 181
    assert int(mod_res) == val_a % 7

    with pytest.raises(ValueError):
        _ = a // Signed(0, 4)

    with pytest.raises(ValueError):
        _ = a % Signed(0, 8)

    sub_pos = a - b
    sub_neg = b - a

    assert len(sub_pos) == 181
    assert len(sub_neg) == 181

    assert format(sub_pos) == f"Signed[180 downto 0]{{{val_a - val_b}}}"
    assert format(sub_neg) == f"Signed[180 downto 0]{{{val_b - val_a}}}"


def test_arithmetic_operators_edge_cases():
    s_narrow = Signed(-5, 4)
    s_wide = Signed(1000000, 32)

    sum_nw = s_narrow + s_wide
    assert len(sum_nw) == 33  # max(4, 32) + 1
    assert int(sum_nw) == 999995

    sum_wn = s_wide + s_narrow
    assert len(sum_wn) == 33
    assert int(sum_wn) == 999995

    sub_zero = s_narrow - Signed(-5, 8)
    assert len(sub_zero) == 9  # max(4, 8) + 1
    assert int(sub_zero) == 0

    sub_wn = s_wide - s_narrow
    assert len(sub_wn) == 33
    assert int(sub_wn) == 1000005

    sub_nw = s_narrow - s_wide
    assert len(sub_nw) == 33
    assert int(sub_nw) == -1000005

    s_zero = Signed(0, 8)
    s_neg_one = Signed(-1, 1)

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


@pytest.mark.parametrize(
    ("lhs", "rhs"),
    [
        (-17, 5),
        (17, -5),
        (-17, -5),
    ],
)
def test_modulo_uses_python_semantics(lhs: int, rhs: int):
    value = Signed(lhs, 8)
    divisor = Signed(rhs, 4)

    result = value % divisor
    assert int(result) == lhs % rhs
    assert len(result) == len(divisor)

    value %= divisor
    assert int(value) == lhs % rhs
    assert len(value) == 8

    value = Signed(lhs, 8)
    value %= rhs
    assert int(value) == lhs % rhs
    assert len(value) == 8


def test_compound_assignment():
    a = Signed(10, 8)

    a += Signed(-5, 4)
    assert len(a) == 8
    assert int(a) == 5

    a -= Signed(-3, 5)
    assert len(a) == 8
    assert int(a) == 8

    a *= Signed(-2, 2)
    assert len(a) == 8
    assert int(a) == -16

    a -= Signed(32, 9)
    assert len(a) == 8
    assert int(a) == -48

    a /= Signed(4, 4)
    assert len(a) == 8
    assert int(a) == -12

    a //= Signed(2, 4)
    assert len(a) == 8
    assert int(a) == -6

    a %= Signed(4, 4)
    assert len(a) == 8
    assert int(a) == 2


def test_compound_assignment_harsh():
    # Wrap-around boundary tests
    a = Signed(7, 4)
    a += Signed(2, 8)  # 7 + 2 = 9. 4-bit signed wraps to -7
    assert len(a) == 4
    assert int(a) == -7

    b = Signed(-125, 8)
    b -= Signed(10, 16)  # -125 - 10 = -135. 8-bit signed wraps to 121
    assert int(b) == 121
    assert len(b) == 8

    z = Signed(-5, 5)
    z *= Signed(3, 16)  # -15. Fits in 5-bit signed (-16 to 15)
    assert len(z) == 5
    assert int(z) == -15

    c = Signed(-5, 4)
    c *= Signed(3, 16)  # -15. Wraps around in 4-bit signed (-8 to 7)
    assert len(c) == 4
    assert int(c) == 1  # -15 % 16 = 1

    d = Signed(-100, 32)
    d += Signed(-1, 4)
    assert int(d) == -101
    assert len(d) == 32

    e = Signed(-128, 8)
    e *= Signed(0, 4)
    assert int(e) == 0

    e += Signed(127, 8)  # 0 + 127
    assert int(e) == 127

    e /= Signed(-1, 2)
    assert int(e) == -127

    e %= Signed(127, 8)
    assert int(e) == 0

    f = Signed(120, 8)
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
    s1 = Signed(-5, 8)
    s1 += Unsigned(15, 8)
    assert s1 == Signed(10, 8)

    s2 = Signed(120, 8)
    s2 += Unsigned(10, 8)  # 130 wraps to -126
    assert s2 == Signed(-126, 8)

    s3 = Signed(5, 8)
    s3 -= Unsigned(10, 8)
    assert s3 == Signed(-5, 8)

    s4 = Signed(-10, 8)
    s4 *= Unsigned(3, 8)
    assert s4 == Signed(-30, 8)

    s5 = Signed(-20, 8)
    s5 //= Unsigned(4, 8)
    assert len(s5) == 8
    assert int(s5) == -5

    s6 = Signed(-23, 8)
    s6 %= Unsigned(7, 8)
    assert len(s6) == 8
    assert int(s6) == 5

    s7 = Signed(-50, 8)
    with pytest.raises(ValueError):
        s7 //= Unsigned(0, 8)
    with pytest.raises(ValueError):
        s7 %= Unsigned(0, 8)


def test_compound_assignment_mixed_signedness_harsh():
    s_narrow = Signed(-5, 4)
    s_narrow += Unsigned(20, 32)  # -5 + 20 = 15. 4-bit signed wraps to -1
    assert int(s_narrow) == -1
    assert len(s_narrow) == 4

    s_wide = Signed(-10, 32)
    s_wide -= Unsigned(15, 4)  # -10 - 15 = -25
    assert int(s_wide) == -25
    assert len(s_wide) == 32

    s_mult = Signed(-10, 8)
    # -10 * 500 = -5000. In 8-bit signed: -5000 % 256 = 120
    s_mult *= Unsigned(500, 32)
    assert int(s_mult) == 120

    s_div = Signed(-120, 8)
    s_div //= Unsigned(60, 16)
    assert int(s_div) == -2

    s_mod = Signed(-120, 8)
    # Dividend is negative, Divisor is positive -> Remainder is positive
    s_mod %= Unsigned(60, 16)
    assert int(s_mod) == 0

    s_zero = Signed(100, 8)
    with pytest.raises(ValueError):
        s_zero //= Unsigned(0, 32)
    with pytest.raises(ValueError):
        s_zero %= Unsigned(0, 32)


def test_comparisons():
    a = Signed(-10, 8)
    b = Signed(-10, 8)
    c = Signed(20, 8)
    d = Signed(-20, 8)

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
    a = Signed(-5, 8)

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

    shift_amt = Unsigned(2, 4)
    assert int(a << shift_amt) == -40


def test_shift_operators_harsh_edge_cases():
    s_8 = Signed(-86, 8)  # 0b10101010

    assert int(s_8 << 7) == 0  # LSB was 0 -> shifted to MSB -> 0
    s_8_b = Signed(-127, 8)  # 0b10000001
    assert int(s_8_b << 7) == -128  # LSB was 1 -> shifted to MSB -> -128

    assert int(s_8 << 8) == 0
    assert int(s_8 >> 8) == -1  # Sign extension replicated

    assert int(s_8 << 1000) == 0
    assert int(s_8 >> 1000) == -1

    shift_s_pos = Signed(3, 8)
    assert int(s_8 << shift_s_pos) == 80  # 0b10101010 << 3 = 01010000 = 80
    assert int(s_8 >> shift_s_pos) == -11  # -86 // 8 = -11

    shift_s_huge = Signed(50000, 32)
    assert int(s_8 << shift_s_huge) == 0

    shift_s_neg = Signed(-2, 8)
    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = s_8 << shift_s_neg
    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = s_8 >> shift_s_neg

    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = s_8 << Signed(-100, 32)

    shift_u_huge = Unsigned(9999999, 64)
    assert int(s_8 << shift_u_huge) == 0
    assert int(s_8 >> shift_u_huge) == -1

    shift_u_zero = Unsigned(0, 4)
    shift_s_zero = Signed(0, 4)
    assert int(s_8 << shift_u_zero) == -86
    assert int(s_8 >> shift_s_zero) == -86
    assert int(s_8 << 0) == -86

    s_comp = Signed(-5, 4)  # 1011 (binary)
    s_comp <<= 2  # 101100 -> truncated to 4 bits -> 1100 (-4)
    assert int(s_comp) == -4
    assert len(s_comp) == 4  # Width MUST NOT grow

    s_comp <<= Signed(2, 16)  # 110000 -> truncated to 4 bits -> 0000 (0)
    assert int(s_comp) == 0
    assert len(s_comp) == 4

    s_comp2 = Signed(-1, 8)  # 11111111 (-1)
    s_comp2 >>= Unsigned(4, 8)  # Sign extended right shift leaves it as -1
    assert int(s_comp2) == -1
    assert len(s_comp2) == 8

    s_128 = Signed(-550059, 128)

    s_128 >>= Unsigned(500, 40)
    assert int(s_128) == -1

    with pytest.raises(TypeError):
        _ = s_8 << -1
    with pytest.raises(TypeError):
        s_8 >>= -10


def test_index_operator():
    a = Signed(-6, 4)  # 1010 in 2's complement

    assert bool(a[3])  # Sign bit is 1
    assert not bool(a[2])
    assert bool(a[1])
    assert not bool(a[0])

    with pytest.raises(IndexError):
        _ = a[4]


def test_formatter():
    # Negative value tests 2's complement representation
    small = Signed(-102, 10)  # 1024 - 102 = 922 -> 1110011010
    mid = Signed(0x0AFFFE9001, 39)

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
    a = Signed(-100, 8)
    neg_a = -a

    assert type(neg_a) is Signed
    assert len(neg_a) == 9
    assert int(neg_a) == 100

    b = Signed(5, 4)
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


def test_constructors_big():
    # 150-bit Signed range: -2**149 to 2**149 - 1
    max_val = (
        1 << 149
    ) - 1  # 713,623,846,352,979,940,529,142,984,724,747,568,191,373,311
    min_val = -(
        1 << 149
    )  #  -713,623,846,352,979,940,529,142,984,724,747,568,191,373,312

    a = Signed(max_val, 150)
    assert int(a) == max_val
    assert len(a) == 150

    b = Signed(min_val, 150)
    assert int(b) == min_val
    assert len(b) == 150

    with pytest.raises(ValueError):
        Signed(max_val + 1, 150)
    with pytest.raises(ValueError):
        Signed(min_val - 1, 150)


def test_explicit_native_casts_big():
    big_val = -(1 << 200) + 500
    a = Signed(big_val, 250)
    assert bool(a) is True
    assert int(a) == big_val

    assert bool(Signed(0, 250)) is False
    assert bool(Signed(1 << 240, 250)) is True


def test_arithmetic_operators_edge_cases_big():
    s_narrow = Signed(-5, 4)
    s_wide = Signed(1 << 190, (200))

    sum_nw = s_narrow + s_wide
    assert len(sum_nw) == 201
    assert int(sum_nw) == (1 << 190) - 5

    sum_wn = s_wide + s_narrow
    assert len(sum_wn) == 201
    assert int(sum_wn) == (1 << 190) - 5

    sub_wn = s_wide - s_narrow
    assert len(sub_wn) == 201
    assert int(sub_wn) == (1 << 190) + 5

    sub_nw = s_narrow - s_wide
    assert len(sub_nw) == 201
    assert int(sub_nw) == -5 - (1 << 190)

    prod_neg_one = s_wide * Signed(-1, 1)
    assert len(prod_neg_one) == 201
    assert int(prod_neg_one) == -(1 << 190)

    div_large = s_narrow / s_wide
    assert int(div_large) == -1

    mod_large = s_narrow % s_wide
    assert int(mod_large) == (1 << 190) - 5


def test_compound_assignment_big():
    val = 1 << 180
    a = Signed(val, 200)

    a += Signed(-(1 << 90), 100)
    assert len(a) == 200
    assert int(a) == val - (1 << 90)

    a -= Signed(-500, 10)
    assert len(a) == 200
    assert int(a) == val - (1 << 90) + 500

    a *= Signed(-2, 2)
    assert len(a) == 200
    assert int(a) == (val - (1 << 90) + 500) * -2


def test_compound_assignment_harsh_big():
    max_val = (1 << 149) - 1
    min_val = -(1 << 149)

    a = Signed(max_val, 150)
    a += Signed(2, 10)
    assert len(a) == 150
    assert int(a) == min_val + 1  # 2's complement wrap around to negative

    b = Signed(min_val, 150)
    b -= Signed(5, 10)
    assert len(b) == 150
    assert int(b) == max_val - 4  # 2's complement wrap around to positive

    c = Signed(-500, 100)
    c *= Signed((1 << 95) + 7, 150)
    assert len(c) == 100
    # Calculate exact wrap around for 100-bit signed integer
    expected = -500 * ((1 << 95) + 7)
    expected_wrapped = (expected + (1 << 99)) % (1 << 100) - (1 << 99)
    assert int(c) == expected_wrapped


def test_compound_assignment_operators_mixed_signedness_big():
    s1 = Signed(-(1 << 180), 200)
    s1 += Unsigned(1 << 140, 150)
    assert s1 == Signed(-(1 << 180) + (1 << 140), 200)

    s2 = Signed(1 << 190, 200)
    s2 -= Unsigned(1 << 195, 200)
    assert len(s2) == 200
    expected = (1 << 190) - (1 << 195)
    expected_wrapped = (expected + (1 << 199)) % (1 << 200) - (1 << 199)
    assert int(s2) == expected_wrapped


def test_compound_assignment_mixed_signedness_harsh_big():
    s_narrow = Signed(-5, 4)
    s_narrow += Unsigned((1 << 190) + 20, 200)
    assert len(s_narrow) == 4
    # Check width truncation down to 4 bits
    expected_wrapped = (-5 + (1 << 190) + 20 + 8) % 16 - 8
    assert int(s_narrow) == expected_wrapped

    s_div = Signed(-(1 << 140), 150)
    s_div //= Unsigned(1 << 90, 100)
    assert int(s_div) == -(1 << 50)


def test_comparisons_big():
    v1 = 1 << 200
    v2 = -(1 << 200)

    a = Signed(v1, 250)
    b = Signed(v1, 250)
    c = Signed(v2, 250)
    d = Signed(v2, 250)

    assert a == b
    assert c == d
    assert a != c

    assert c < a
    assert not (a < c)
    assert c <= a
    assert a >= c
    assert a > c


def test_shift_operators_big():
    val = -(1 << 150) + 999
    a = Signed(val, 200)

    sl = a << 10
    assert int(sl) == val * (1 << 10)

    sr = a >> 5
    assert int(sr) == val // (1 << 5)

    a <<= 20
    assert int(a) == val * (1 << 20)

    a >>= 25
    assert int(a) == (val * (1 << 20)) // (1 << 25)


def test_shift_operators_harsh_edge_cases_big():
    s_big = Signed(-(1 << 190) + 12345, 200)

    assert int(s_big << 200) == 0
    assert int(s_big >> 200) == -1
    assert int(s_big >> 10000) == -1

    shift_s_pos = Signed(50, 100)
    assert int(s_big << shift_s_pos) == (12345 << 50)

    s_comp = Signed(-(1 << 140), 150)
    s_comp <<= 20
    assert len(s_comp) == 150
    expected = (-(1 << 140)) << 20
    expected_wrapped = (expected + (1 << 149)) % (1 << 150) - (1 << 149)
    assert int(s_comp) == expected_wrapped


def test_formatter_big():
    val = -(1 << 140) + 0xABCDEF
    s = Signed(val, 150)

    # Calculate Python's equivalent 2's complement representation for a 150-bit width
    twos_comp = (1 << 150) + val

    assert format(s, "b") == f"Signed[149 downto 0]{{{twos_comp:0150b}}}"
    assert format(s) == f"Signed[149 downto 0]{{{val}}}"
    assert format(s, "o") == f"Signed[149 downto 0]{{{twos_comp:o}}}"
    assert format(s, "x") == f"Signed[149 downto 0]{{{twos_comp:x}}}"


def test_unary_ops_big():
    val = -(1 << 190) + 123456789
    a = Signed(val, 200)
    neg_a = -a

    assert type(neg_a) is Signed
    assert len(neg_a) == 201
    assert int(neg_a) == -val

    pos_a = +a
    assert type(pos_a) is Signed
    assert len(pos_a) == 200
    assert int(pos_a) == val


def test_index_operator_big():
    val = -(1 << 150) | (1 << 75) | 1
    a = Signed(val, 200)

    assert bool(a[199])
    assert bool(a[150])
    assert not bool(a[149])
    assert bool(a[75])
    assert not bool(a[74])
    assert bool(a[0])

    with pytest.raises(IndexError):
        _ = a[200]


def test_bitwise_operators():
    a = Signed(12, 8)  # 00001100
    b = Signed(10, 8)  # 00001010

    assert int(a & b) == 8
    assert int(a | b) == 14
    assert int(a ^ b) == 6
    assert int(~a) == -13  # Python's ~12 is -13

    assert int(a & Signed(10, 8)) == 8
    assert int(a | Signed(2, 8)) == 14
    assert int(a ^ Signed(6, 8)) == 10

    assert int(a & Signed(-1, 8)) == 12
    assert int(a | Signed(-1, 8)) == -1
    assert int(a ^ Signed(-1, 8)) == -13


def test_bitwise_operators_big():
    val_a = 0xFAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
    val_b = 0xF55555555555555555555555555555555

    a = Signed(val_a, 150)
    b = Signed(val_b, 150)

    assert int(a & b) == val_a & val_b
    assert int(a | b) == val_a | val_b
    assert int(a ^ b) == val_a ^ val_b
    assert int(~a) == ~val_a
