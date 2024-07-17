import room
from functools import cached_property
import conversions
import math

# Fan cooling needs to know temps
# Temp delay needs to know temps


class SystemEnergy:
    def __init__(self, component_energy_j=None):
        self._component_energy_j = component_energy_j or {}

    @cached_property
    def net_energy_j(self):
        return sum(e for e in self._component_energy_j.values())

    @cached_property
    def total_energy_j(self):
        return sum(abs(e) for e in self._component_energy_j.values())

    @cached_property
    def energy_by_component_name(self):
        return self._component_energy_j

    def __iadd__(self, other):
        for n, e in other.energy_by_component_name.items():
            self._component_energy_j.setdefault(n, 0)
            self._component_energy_j[n] += e

        return self

    def __str__(self):
        return "%.1f (%s)" % (
            (conversions.j_to_wh(self.total_energy_j) / 1000.0),
            ", ".join(
                "%s:%.1f" % (n, conversions.j_to_wh(e) / 1000.0)
                for n, e in self._component_energy_j.items()
            ),
        )

    def __repr__(self):
        return "SystemEnergy(<%s>%s)" % (id(self), str(self))


class SystemPower:
    def __init__(self, component_power_w):
        self._component_power_w = component_power_w

    def energy(self, time_s):
        return SystemEnergy({n: time_s * p for n, p in self._component_power_w.items()})

    @cached_property
    def total_power_w(self):
        return sum(p for p in self._component_power_w.values())


class System:
    def __init__(self, components_by_name):
        self._components_by_name = components_by_name

    def get_power(self, *args):
        return SystemPower(
            {n: c.get_power(*args) for n, c in self._components_by_name.items()}
        )


class PIDSystem:
    def __init__(self, components):
        self._components = components

    def get_power(self, *args):
        pass


class SimpleFanCooling:
    def __init__(self, m3_m):
        self._m3_s = m3_m / 60

    # TODO: Add composable hysteresis?
    def get_power(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        if in_temp_c < cool_setpoint_c or out_temp_c >= in_temp_c:
            return 0

        return room.AIR_HEAT_CAPACITY_J_M3K * (out_temp_c - in_temp_c) * self._m3_s


class SimpleThermostat:
    def __init__(self, power_w, is_heater):
        self._power_w = power_w * (1 if is_heater else -1)
        self._is_heater = is_heater

    def get_power(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
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

    def get_power(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
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


class TempDelay:
    def __init__(self, temp_delay_c, component):
        self._temp_delay_c = temp_delay_c
        self._component = component
        self._is_on = False

    def get_power(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        energy = self._component.get_power(
            in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c
        )

        if energy == 0:
            self._is_on = False
            return energy

        if (
            max(heat_setpoint_c - in_temp_c, in_temp_c - cool_setpoint_c)
            > self._temp_delay_c
        ):
            self._is_on = True

        if self._is_on:
            return energy
        else:
            return 0


class FixedOutput:
    def __init__(self, energy_w=0):
        self._energy_w = energy_w

    def get_power(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        return self._energy_w
