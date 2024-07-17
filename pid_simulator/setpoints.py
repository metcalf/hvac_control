class FixedSetpoint:
    def __init__(self, heat_setpoint, cool_setpoint):
        self._heat_setpoint = heat_setpoint
        self._cool_setpoint = cool_setpoint

    def get(self, _):
        return (self._heat_setpoint, self._cool_setpoint)


class DayNightSetpoint:
    def __init__(self, day_heat, day_cool, night_heat, night_cool, precool_rate_c_hr=0):
        self._day_heat = day_heat
        self._day_cool = day_cool
        self._night_heat = night_heat
        self._night_cool = night_cool

        if precool_rate_c_hr < 0:
            raise ValueError(
                "precool_rate_c_hr must be positive, got: %f", precool_rate_c_hr
            )
        self._precool_rate_c_m = precool_rate_c_hr / 60.0

        self._day_start_hr = 7
        self._day_end_hr = 21

    def get(self, dt):
        if dt.hour < self._day_start_hr or dt.hour >= self._day_end_hr:
            return (self._night_heat, self._night_cool)

        cool = self._day_cool
        if self._precool_rate_c_m > 0:
            mins_until_night = (self._day_end_hr - dt.hour) * 60 - dt.minute
            cool = min(
                cool, self._night_cool + mins_until_night * self._precool_rate_c_m
            )

        return (self._day_heat, cool)
