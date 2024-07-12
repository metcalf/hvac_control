import room


class System:
    def __init__(self, components):
        self._components = components

    def get_energy(self, *args):
        return sum(c.get_energy(*args) for c in self._components)


class SimpleFanCooling:
    def __init__(self, cmm):
        self._cms = cmm / 60

    # TODO: Add composable hysteresis?
    def get_energy(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        if in_temp_c < cool_setpoint_c or out_temp_c >= in_temp_c:
            return 0

        return room.AIR_HEAT_CAPACITY_J_M3K * (out_temp_c - in_temp_c) * self._cms


class SimpleThermostat:
    def __init__(self, power_w, is_heater):
        self._power_w = power_w * (1 if is_heater else -1)
        self._is_heater = is_heater

    def get_energy(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        if self._is_heater:
            on = heat_setpoint_c > in_temp_c
        else:
            on = in_temp_c > cool_setpoint_c

        if on:
            return self._power_w
        else:
            return 0


class HysteresisThermostat:
    def __init__(self, power_w, is_heater, hysteresis_c=0.5):
        self._power_w = power_w * (1 if is_heater else -1)
        self._is_heater = is_heater
        self._hysteresis_c = hysteresis_c
        self._is_on = False

    def get_energy(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        if self._is_heater:
            delta = heat_setpoint_c - in_temp_c
        else:
            delta = in_temp_c - cool_setpoint_c

        if delta > self._hysteresis_c:
            self._is_on = True
        elif delta < -self._hysteresis_c:
            self._is_on = False

        if self._is_on:
            return self._power_w
        else:
            return 0


class FixedOutput:
    def __init__(self, energy_w=0):
        self._energy_w = energy_w

    def get_energy(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        return self._energy_w
