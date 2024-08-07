import room
from functools import cached_property
import conversions
from enum import Enum

# Fan cooling needs to know temps
# Temp delay needs to know temps


class HVACMode(Enum):
    COOL = -1
    OFF = 0
    HEAT = 1

    @classmethod
    def from_power(cls, p):
        if p > 0:
            return cls.HEAT
        elif p < 0:
            return cls.COOL
        else:
            return cls.OFF


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
    def net_power_w(self):
        return sum(p for p in self._component_power_w.values())

    def __str__(self):
        return "%.2f (%s)" % (
            (self.net_power_w / 1000.0),
            ", ".join(
                "%s:%.2f" % (n, p / 1000.0) for n, p in self._component_power_w.items()
            ),
        )

    def __repr__(self):
        return "SystemPower(<%s>%s)" % (id(self), str(self))


class System:
    def __init__(self, components_by_name):
        self._components_by_name = components_by_name

    def get_power(self, *args):
        return SystemPower(
            {n: c.get_power(*args) for n, c in self._components_by_name.items()}
        )


class PIDSystem:
    def __init__(self, components_by_name, p_range_c, t_i):
        self._components_by_name = components_by_name
        self._p_range_c = float(p_range_c)
        self._i = 0.0
        self._t_i = t_i
        self._i_mode = HVACMode.OFF

    def get_power(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        p_setpoint_c = None
        i_setpoint_c = None
        sign = 1

        if in_temp_c > cool_setpoint_c:
            p_setpoint_c = cool_setpoint_c
            self._i_mode = HVACMode.COOL
            sign = -1
        elif in_temp_c < heat_setpoint_c:
            p_setpoint_c = heat_setpoint_c
            self._i_mode = HVACMode.HEAT

        if self._i_mode == HVACMode.COOL:
            i_setpoint_c = cool_setpoint_c
        elif self._i_mode == HVACMode.HEAT:
            i_setpoint_c = heat_setpoint_c

        if p_setpoint_c is not None:
            abs_p_err = abs(p_setpoint_c - in_temp_c) / self._p_range_c
        else:
            abs_p_err = 0

        if i_setpoint_c is not None:
            i_err = (i_setpoint_c - in_temp_c) / self._p_range_c
        else:
            i_err = 0

        # TODO: Integral error should be based on the last setpoint we
        # exceeded, not the same error as proportional

        # If integral is positive (heating), compute error on heat setpoint
        # If integral is negative (cooling), compute error on the cool setpoint
        # If integral is zero, base it on the proportional setpoint
        # We need to avoid discontinuity when the integral crosses zero

        # If last proportional error was positive (heating), integral can only be positive
        # If last proportional error was negative (cooling), integral can only be negative
        # Need to account for setpoint changes so don't just store the last one

        p_demand = sign * abs_p_err
        self._i += i_err
        self._clamp_integral(p_demand)
        i_demand = self._i / self._t_i

        demand = self._clamp(p_demand + i_demand)

        return SystemPower(
            {
                n: c.get_power_with_demand(
                    demand, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c
                )
                for n, c in self._components_by_name.items()
            }
        )

    def _clamp_integral(self, p_demand):
        p_demand = self._clamp(p_demand)

        # TODO: Should we clamp integral to never be opposite proportional?
        upper = 1 if self._i_mode == HVACMode.HEAT else 0
        lower = -1 if self._i_mode == HVACMode.COOL else 0
        if p_demand > 0:
            upper = max(0, upper - p_demand)
        else:
            lower = min(0, lower - p_demand)

        # If clamped p_demand is:
        # 1, clamp _i=[-1*t_i, 0*_t_i]
        # 0.5, clamp _i=[-1*t_i, 0.5*_t_i]
        # 0, clamp _i=[1*-t_i, 1*_t_i]
        # -0.5, clamp _i=[-0.5*_t_, 1*_t_i]
        # -1, clamp, _i=[0*_t_i, 1*_t_i]

        # Concept here was to limit the range of i further but doesn't seem to help
        # much with overshoot
        i_demand_max = 1

        self._i = self._clamp(
            self._i,
            max(-i_demand_max, lower) * self._t_i,
            min(i_demand_max, upper) * self._t_i,
        )

    def _clamp(self, value, min_value=-1, max_value=1):
        return max(min_value, min(value, max_value))


class FanCooling:
    def __init__(self, m3_m):
        self._m3_s = m3_m / 60.0

    def _get_power_for_delta(self, delta_c):
        if delta_c >= 0:
            return 0

        return room.AIR_HEAT_CAPACITY_J_M3K * delta_c * self._m3_s

    def get_available(self, in_temp_c, out_temp_c):
        delta = out_temp_c - in_temp_c
        return (
            self._get_power_for_delta(delta),
            self._get_power_for_delta(min(-10, delta)),
        )

    def get_power(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        demand = -1 if in_temp_c > cool_setpoint_c else 0
        return self.get_power_with_demand(
            demand, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c
        )

    def get_power_with_demand(
        self, demand, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c
    ):
        if demand >= 0 or out_temp_c >= in_temp_c:
            return 0

        return (
            -demand
            * room.AIR_HEAT_CAPACITY_J_M3K
            * (out_temp_c - in_temp_c)
            * self._m3_s
        )


class Thermostat:
    def __init__(self, power_w, is_heater):
        self._power_w = power_w
        self._is_heater = is_heater

    def get_available(self, *args):
        pwr = self._power_w * (1 if self._is_heater else -1)
        return (pwr, pwr)

    def get_power(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        if in_temp_c > cool_setpoint_c:
            demand = -1
        elif in_temp_c < heat_setpoint_c:
            demand = 1
        else:
            demand = 0

        return self.get_power_with_demand(
            demand, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c
        )

    def get_power_with_demand(
        self, demand, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c
    ):
        if (self._is_heater and demand < 0) or (not self._is_heater and demand > 0):
            return 0

        return demand * self._power_w


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

    def get_available(self, *args):
        res = self._component.get_available(*args)
        if not self._is_on:
            res[0] = 0

        return res

    def _handle_power(self, power, in_temp_c, heat_setpoint_c, cool_setpoint_c):
        if power == 0:
            self._is_on = False
        elif (power < 0 and in_temp_c < cool_setpoint_c) or (
            power > 0 and in_temp_c > heat_setpoint_c
        ):
            # Turn off when we reach setpoin
            self.is_on = False
        elif (
            max(heat_setpoint_c - in_temp_c, in_temp_c - cool_setpoint_c)
            > self._temp_delay_c
        ):
            self._is_on = True

        if self._is_on:
            return power
        else:
            return 0

    def get_power(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        power = self._component.get_power(
            in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c
        )
        return self._handle_power(power, in_temp_c, heat_setpoint_c, cool_setpoint_c)

    def get_power_with_demand(
        self, demand, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c
    ):
        power = self._component.get_power_with_demand(
            demand, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c
        )
        return self._handle_power(power, in_temp_c, heat_setpoint_c, cool_setpoint_c)


class DiscreteLevels:
    def __init__(self, component, levels):
        levels = sorted(levels)
        self._component = component
        self._levels = levels
        self._level_i = levels.index(0)
        self._max_level_i = len(self._levels) - 1

    def get_available(self, *args):
        return self._component.get_available(*args)

    def get_power_with_demand(
        self, demand, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c
    ):
        while self._level_i > 0 and demand <= self._levels[self._level_i - 1]:
            self._level_i -= 1

        while (
            self._level_i < self._max_level_i
            and demand >= self._levels[self._level_i + 1]
        ):
            self._level_i += 1

        delta = demand - self._levels[self._level_i]

        return self._component.get_power_with_demand(
            self._levels[self._level_i],
            in_temp_c,
            out_temp_c,
            heat_setpoint_c,
            cool_setpoint_c,
        )


class FixedOutput:
    def __init__(self, energy_w=0):
        self._energy_w = energy_w

    def get_power(self, in_temp_c, out_temp_c, heat_setpoint_c, cool_setpoint_c):
        return self._energy_w
