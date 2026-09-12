from __future__ import annotations

import pytest

from coconext.types import Signed, Unsigned


def test_constructors():
    a = Unsigned(15, 4)
    assert int(a) == 15
    assert len(a) == 4

    # Out of range / overflow checks
    with pytest.raises(OverflowError):
        Unsigned(16, 4)
    with pytest.raises(OverflowError):
        Unsigned(-1, 4)

    large_val = Unsigned(200, 8)
    small_val = Unsigned(10, 8)

    narrow_fit = Unsigned(int(small_val), 4)
    assert int(narrow_fit) == 10

    with pytest.raises(OverflowError):
        Unsigned(int(large_val), 4)


def test_explicit_native_casts():
    a = Unsigned(42000, 16)

    assert bool(a) is True
    assert bool(Unsigned(0, 16)) is False

    assert int(a) == 42000


def test_arithmetic_operators():
    a = Unsigned(150, 8)

    b = Unsigned(50, 8)
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

    mod_res = a % Unsigned(7, 4)
    assert int(mod_res) == 150 % 7

    with pytest.raises(ValueError):
        _ = a // Unsigned(0, 4)

    with pytest.raises(ValueError):
        _ = a % Unsigned(0, 8)

    sub_pos = a - b
    sub_neg = b - a

    assert len(sub_pos) == 9
    assert len(sub_neg) == 9
    assert int(sub_pos) == 100
    assert int(sub_neg) == -100


def test_arithmetic_operators_edge_cases():
    u_narrow = Unsigned(15, 4)  # Max 4-bit (1111)
    u_wide = Unsigned(1000000, 32)  # 32-bit

    sum_nw = u_narrow + u_wide
    assert len(sum_nw) == 33  # max(4, 32) + 1
    assert int(sum_nw) == 1000015

    sum_wn = u_wide + u_narrow
    assert len(sum_wn) == 33
    assert int(sum_wn) == 1000015

    sub_zero = u_narrow - Unsigned(15, 8)
    assert len(sub_zero) == 9  # max(4, 8) + 1
    assert int(sub_zero) == 0

    sub_wn = u_wide - u_narrow
    assert len(sub_wn) == 33
    assert int(sub_wn) == 999985

    sub_nw = u_narrow - u_wide
    assert len(sub_nw) == 33
    assert int(sub_nw) == 15 - 1000000

    u_zero = Unsigned(0, 8)
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
    a = Unsigned(10, 8)
    b = Unsigned(10, 8)
    c = Unsigned(20, 8)
    d = Unsigned(5, 8)

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
    a = Unsigned(10, 8)

    a += Unsigned(5, 4)
    assert len(a) == 8
    assert int(a) == 15

    a -= Unsigned(3, 5)
    assert len(a) == 8
    assert int(a) == 12

    a *= Unsigned(2, 2)
    assert len(a) == 8
    assert int(a) == 24

    a -= Unsigned(32, 9)
    assert len(a) == 8
    assert int(a) == 248

    a /= Unsigned(4, 4)
    assert len(a) == 8
    assert int(a) == 62

    a //= Unsigned(2, 4)
    assert len(a) == 8
    assert int(a) == 31

    a %= Unsigned(4, 4)
    assert len(a) == 8
    assert int(a) == 3


def test_compound_assignment_harsh():
    a = Unsigned(15, 4)
    a += Unsigned(2, 8)  # 15 + 2 = 17 % 16 = 1
    assert int(a) == 1
    assert len(a) == 4

    b = Unsigned(5, 8)
    b -= Unsigned(10, 16)  # 5 - 10 = -5
    assert int(b) == 251  # -5 in 8-bit unsigned = 251
    assert len(b) == 8

    c = Unsigned(5, 4)
    c *= Unsigned(1000, 16)  # 5 * 1000 = 5000
    assert int(c) == 8  # 5000 % 16 = 8
    assert len(c) == 4

    d = Unsigned(100, 32)
    d += Unsigned(15, 4)
    assert int(d) == 115
    assert len(d) == 32

    e = Unsigned(128, 8)
    e *= Unsigned(0, 4)
    assert int(e) == 0

    e += Unsigned(255, 8)  # 0 + 255
    assert int(e) == 255

    e /= Unsigned(1, 2)
    assert int(e) == 255

    e %= Unsigned(255, 8)
    assert int(e) == 0

    f = Unsigned(250, 8)
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
    u1 = Unsigned(15, 8)
    u1 += Signed(-5, 8)
    assert u1 == Unsigned(10, 8)

    u2 = Unsigned(250, 8)
    u2 += Signed(10, 8)
    assert u2 == Unsigned(4, 8)

    u3 = Unsigned(5, 8)
    u3 -= Signed(10, 8)
    assert u3 == Unsigned(251, 8)

    u4 = Unsigned(10, 8)
    u4 *= Signed(-3, 8)
    assert u4 == Unsigned(226, 8)

    u5 = Unsigned(20, 8)
    u5 //= Signed(-4, 8)
    assert u5 == Unsigned(251, 8)

    u6 = Unsigned(23, 8)
    u6 %= Signed(-7, 8)
    assert u6 == Unsigned(2, 8)

    u7 = Unsigned(50, 8)
    with pytest.raises(ValueError):
        u7 //= Signed(0, 8)
    with pytest.raises(ValueError):
        u7 %= Signed(0, 8)


def test_compound_assignment_mixed_signedness_harsh():
    u_narrow = Unsigned(5, 4)
    u_narrow += Signed(-20, 32)  # 5 + (-20) = -15
    assert int(u_narrow) == 1  # 4-bit unsigned wrap: 16 - 15 = 1
    assert len(u_narrow) == 4

    u_wide = Unsigned(10, 32)
    u_wide -= Signed(-8, 4)  # 10 - (-8) = 18
    assert int(u_wide) == 18
    assert len(u_wide) == 32

    u_mult = Unsigned(10, 8)
    # 10 * -500 = -5000. In 8-bit unsigned: -5000 % 256 = 120
    u_mult *= Signed(-500, 32)
    assert int(u_mult) == 120

    u_div = Unsigned(250, 8)
    # 250 / -60 = -4. Wrap to 8-bit unsigned: 256 - 4 = 252
    u_div //= Signed(-60, 16)
    assert int(u_div) == 252

    u_mod = Unsigned(250, 8)
    # Dividend is positive -> Remainder is positive (250 = -4 * -60 + 10)
    u_mod %= Signed(-60, 16)
    assert int(u_mod) == 10

    u_zero = Unsigned(100, 8)
    with pytest.raises(ValueError):
        u_zero //= Signed(0, 32)
    with pytest.raises(ValueError):
        u_zero %= Signed(0, 32)


def test_shift_operators():
    a = Unsigned(5, 8)

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

    shift_amt = Unsigned(2, 4)
    assert int(a << shift_amt) == 40


def test_shift_operators_harsh_edge_cases():
    u_8 = Unsigned(0b10101010, 8)  # 170 in decimal

    assert int(u_8 << 7) == 0  # LSB was 0 -> shifted to MSB -> 0
    u_8_b = Unsigned(1, 8)
    assert int(u_8_b << 7) == 128  # LSB was 1 -> shifted to MSB -> 128

    assert int(u_8 << 8) == 0
    assert int(u_8 >> 8) == 0

    assert int(u_8 << 1000) == 0
    assert int(u_8 >> 1000) == 0

    shift_s_pos = Signed(3, 8)
    assert int(u_8 << shift_s_pos) == 80
    assert int(u_8 >> shift_s_pos) == 21  # 170 >> 3 = 21

    shift_s_huge = Signed(50000, 32)
    assert int(u_8 << shift_s_huge) == 0

    shift_s_neg = Signed(-2, 8)
    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = u_8 << shift_s_neg
    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = u_8 >> shift_s_neg

    with pytest.raises(ValueError, match="Negative shift amount"):
        _ = u_8 << Signed(-100, 32)

    shift_u_huge = Unsigned(9999999, 64)
    assert int(u_8 << shift_u_huge) == 0
    assert int(u_8 >> shift_u_huge) == 0

    shift_u_zero = Unsigned(0, 4)
    shift_s_zero = Signed(0, 4)
    assert int(u_8 << shift_u_zero) == 170
    assert int(u_8 >> shift_s_zero) == 170
    assert int(u_8 << 0) == 170

    u_comp = Unsigned(15, 4)  # 1111 (binary)
    u_comp <<= 2  # 111100 -> truncated to 4 bits -> 1100 (12)
    assert int(u_comp) == 12
    assert len(u_comp) == 4  # Width MUST NOT grow

    u_comp <<= Signed(2, 16)  # 110000 -> truncated to 4 bits -> 0000 (0)
    assert int(u_comp) == 0
    assert len(u_comp) == 4

    u_comp2 = Unsigned(255, 8)
    u_comp2 >>= Unsigned(4, 8)  # 00001111 (15)
    assert int(u_comp2) == 15
    assert len(u_comp2) == 8

    u_128 = Unsigned(550059, 128)

    u_128 >>= Unsigned(500, 40)
    assert int(u_128) == 0

    with pytest.raises(TypeError):
        _ = u_8 << -1
    with pytest.raises(TypeError):
        u_8 >>= -10


def test_index_operator():
    a = Unsigned(2, 4)  # 0010

    assert not bool(a[3])
    assert not bool(a[2])
    assert bool(a[1])
    assert not bool(a[0])

    with pytest.raises(IndexError):
        _ = a[4]


def test_formatter():
    small = Unsigned(102, 10)
    mid = Unsigned(0x0AFFFE9001, 39)

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
    a = Unsigned(150, 8)
    neg_a = -a

    assert type(neg_a) is Signed
    assert len(neg_a) == 9
    assert int(neg_a) == -150

    b = Unsigned(5, 4)
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


def test_constructors_big():
    # 150-bit Unsigned range: 0 to 2**150 - 1
    max_val = (1 << 150) - 1

    a = Unsigned(max_val, 150)
    assert int(a) == max_val
    assert len(a) == 150

    b = Unsigned(0, 150)
    assert int(b) == 0
    assert len(b) == 150

    with pytest.raises(IndexError):
        Unsigned(max_val + 1, 150)
    with pytest.raises(OverflowError):
        Unsigned(-1, 150)


def test_explicit_native_casts_big():
    big_val = (1 << 200) + 500
    a = Unsigned(big_val, 250)
    assert bool(a) is True
    assert int(a) == big_val

    assert bool(Unsigned(0, 250)) is False
    assert bool(Unsigned(1 << 240, 250)) is True


def test_arithmetic_operators_edge_cases_big():
    u_narrow = Unsigned(15, 4)
    u_wide = Unsigned(1 << 190, (200))

    sum_nw = u_narrow + u_wide
    assert len(sum_nw) == 201
    assert int(sum_nw) == (1 << 190) + 15

    sum_wn = u_wide + u_narrow
    assert len(sum_wn) == 201
    assert int(sum_wn) == (1 << 190) + 15

    sub_wn = u_wide - u_narrow
    assert len(sub_wn) == 201
    assert int(sub_wn) == (1 << 190) - 15

    sub_nw = u_narrow - u_wide
    assert len(sub_nw) == 201
    assert int(sub_nw) == 15 - (1 << 190)

    prod_one = u_wide * Unsigned(1, 1)
    assert len(prod_one) == 201
    assert int(prod_one) == (1 << 190)

    div_large = u_narrow / u_wide
    assert int(div_large) == 0

    mod_large = u_narrow % u_wide
    assert int(mod_large) == 15


def test_compound_assignment_big():
    val = 1 << 180
    a = Unsigned(val, 200)

    a += Unsigned(1 << 90, (100))
    assert len(a) == 200
    assert int(a) == val + (1 << 90)

    a -= Unsigned(500, 10)
    assert len(a) == 200
    assert int(a) == val + (1 << 90) - 500

    a *= Unsigned(2, 2)
    assert len(a) == 200
    assert int(a) == (val + (1 << 90) - 500) * 2


def test_compound_assignment_harsh_big():
    max_val = (1 << 150) - 1

    a = Unsigned(max_val, 150)
    a += Unsigned(2, 10)
    assert len(a) == 150
    assert int(a) == 1  # Unsigned wrap around

    b = Unsigned(2, 150)
    b -= Unsigned(5, 10)
    assert len(b) == 150
    assert int(b) == max_val - 2  # Unsigned underflow wrap

    c = Unsigned(500, 100)
    c *= Unsigned((1 << 95) + 7, 150)
    assert len(c) == 100
    # Calculate exact wrap around for 100-bit unsigned integer
    expected = 500 * ((1 << 95) + 7)
    expected_wrapped = expected % (1 << 100)
    assert int(c) == expected_wrapped


def test_compound_assignment_operators_mixed_signedness_big():
    u1 = Unsigned(1 << 180, 200)
    u1 += Signed(-(1 << 140), 150)
    assert u1 == Unsigned((1 << 180) - (1 << 140), 200)

    u2 = Unsigned(1 << 190, 200)
    u2 -= Signed(1 << 195, (200))
    assert len(u2) == 200
    expected = (1 << 190) - (1 << 195)
    expected_wrapped = expected % (1 << 200)
    assert int(u2) == expected_wrapped


def test_compound_assignment_mixed_signedness_harsh_big():
    u_narrow = Unsigned(5, 4)
    u_narrow += Signed(-(1 << 190) + 20, 200)
    assert len(u_narrow) == 4
    # Check width truncation down to 4 bits
    expected_wrapped = (5 - (1 << 190) + 20) % 16
    assert int(u_narrow) == expected_wrapped

    u_div = Unsigned(1 << 140, (150))
    u_div //= Signed(-(1 << 90), 100)
    expected_wrapped = (-(1 << 50)) % (1 << 150)
    assert int(u_div) == expected_wrapped


def test_comparisons_big():
    v1 = 1 << 200
    v2 = (1 << 200) - 100

    a = Unsigned(v1, 250)
    b = Unsigned(v1, 250)
    c = Unsigned(v2, 250)
    d = Unsigned(v2, 250)

    assert a == b
    assert c == d
    assert a != c

    assert c < a
    assert not (a < c)
    assert c <= a
    assert a >= c
    assert a > c


def test_shift_operators_big():
    val = (1 << 150) + 999
    a = Unsigned(val, 200)

    sl = a << 10
    assert int(sl) == val * (1 << 10)

    sr = a >> 5
    assert int(sr) == val // (1 << 5)

    a <<= 20
    assert int(a) == val * (1 << 20)

    a >>= 25
    assert int(a) == (val * (1 << 20)) // (1 << 25)


def test_shift_operators_harsh_edge_cases_big():
    u_big = Unsigned((1 << 190) + 12345, 200)

    assert int(u_big << 200) == 0
    assert int(u_big >> 200) == 0
    assert int(u_big >> 10000) == 0

    shift_s_pos = Signed(50, 100)
    expected = (((1 << 190) + 12345) << 50) % (1 << 200)
    assert int(u_big << shift_s_pos) == expected

    u_comp = Unsigned(1 << 140, (150))
    u_comp <<= 20
    assert len(u_comp) == 150
    expected_comp = (1 << 140) << 20
    expected_wrapped = expected_comp % (1 << 150)
    assert int(u_comp) == expected_wrapped


def test_index_operator_big():
    val = (1 << 150) | (1 << 75) | 1
    a = Unsigned(val, 200)

    assert not bool(a[199])
    assert bool(a[150])
    assert not bool(a[149])
    assert bool(a[75])
    assert not bool(a[74])
    assert bool(a[0])

    with pytest.raises(IndexError):
        _ = a[200]


def test_formatter_big():
    val = (1 << 140) + 0xABCDEF
    u = Unsigned(val, 150)

    assert format(u, "b") == f"Unsigned[149 downto 0]{{{val:0150b}}}"
    assert format(u) == f"Unsigned[149 downto 0]{{{val}}}"
    assert format(u, "o") == f"Unsigned[149 downto 0]{{{val:050o}}}"
    assert format(u, "x") == f"Unsigned[149 downto 0]{{{val:038x}}}"


def test_unary_ops_big():
    val = (1 << 190) + 123456789
    a = Unsigned(val, 200)

    neg_a = -a
    assert type(neg_a) is Signed
    assert len(neg_a) == 201
    assert int(neg_a) == -val

    pos_a = +a
    assert type(pos_a) is Signed
    assert len(pos_a) == 201
    assert int(pos_a) == val


def test_bitwise_operators():
    a = Unsigned(12, 8)  # 00001100
    b = Unsigned(10, 8)  # 00001010

    assert int(a & b) == 8
    assert int(a | b) == 14
    assert int(a ^ b) == 6

    assert int(~a) == 243


def test_bitwise_operators_big():
    val_a = 0xAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
    val_b = 0x55555555555555555555555555555555

    a = Unsigned(val_a, 150)
    b = Unsigned(val_b, 150)

    assert int(a & b) == 0
    assert int(a | b) == val_a | val_b
    assert int(a ^ b) == val_a ^ val_b

    # For Unsigned, bitwise NOT must wrap around the 150-bit boundary
    mask = (1 << 150) - 1
    assert int(~a) == (~val_a) & mask
