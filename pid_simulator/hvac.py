from enum import Enum


class HysteresisThermostat:
    class State(Enum):
        OFF = 1
        HEAT = 2
        COOL = 3

    def __init__(self, heat_w, cool_w, hysteresis_c=0.5):
        self._heat_w = heat_w
        self._cool_w = cool_w
        self._hysteresis_c = hysteresis_c / 2.0
        self._state = self.State.OFF

    def get_energy(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        if in_temp_c > (cool_setpoint_c + self._hysteresis_c):
            self._state = self.State.COOL
        elif in_temp_c < (heat_setpoint_c - self._hysteresis_c):
            self._state = self.State.HEAT
        elif (
            self._state == self.State.HEAT
            and in_temp_c > (heat_setpoint_c + self._hysteresis_c)
        ) or (
            self._state == self.State.COOL
            and in_temp_c < (cool_setpoint_c - self._hysteresis_c)
        ):
            self._state = self.State.OFF

        if self._state == self.State.OFF:
            return 0
        elif self._state == self.State.HEAT:
            return self._heat_w
        else:
            return -self._cool_w

        if in_temp_c > cool_setpoint_c:
            return -self._cool_w
        elif in_temp_c < heat_setpoint_c:
            return self._heat_w
        else:
            return 0


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
