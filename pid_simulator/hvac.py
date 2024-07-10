class SimpleThermostat:
    def __init__(self, heat_w, cool_w):
        self._heat_w = heat_w
        self._cool_w = cool_w

    def get_energy(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        if in_temp_c > cool_setpoint_c:
            return -self._cool_w
        elif in_temp_c < heat_setpoint_c:
            return self._heat_w
        else:
            return 0


class FixedOutput:
    def __init__(self, energy_w=0):
        self._energy_w = energy_w

    def get_energy(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        return self._energy_w
