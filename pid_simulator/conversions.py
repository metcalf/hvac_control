def f3_to_m3(volume_f3):
    return volume_f3 * 0.0283168


def f_to_c(temp_f):
    return rel_f_to_c(temp_f - 32)


def c_to_f(temp_c):
    return rel_c_to_f(temp_c) + 32


def rel_c_to_f(temp_c):
    return temp_c * 9.0 / 5.0


def rel_f_to_c(temp_f):
    return temp_f * (5.0 / 9.0)


def btuhr_to_w(pwr):
    return pwr * 0.2930710702


def lb_to_kg(lb):
    return lb * 0.453592


def j_to_wh(j):
    return j * 0.000277778


def ft3_to_m3(ft3):
    return ft3 * 0.0283168
