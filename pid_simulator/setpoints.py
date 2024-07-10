class FixedSetpoint:
    def __init__(self, heat_setpoint, cool_setpoint):
        self._heat_setpoint = heat_setpoint
        self._cool_setpoint = cool_setpoint

    def get(self, _):
        return (self._heat_setpoint, self._cool_setpoint)
