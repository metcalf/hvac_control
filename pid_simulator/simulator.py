from collections import namedtuple
import math
import matplotlib.pyplot as plt
import conversions
import datetime
from enum import Enum
import hvac

DataPoint = namedtuple(
    "DataPoint",
    (
        "datetime",
        "outdoor_temp_c",
        "room_temp_c",
        "heat_setpoint_c",
        "cool_setpoint_c",
        "construction_temp_c",
        "surfaces_temp_c",
        "system_energy",
    ),
)
Stats = namedtuple(
    "Stats",
    (
        "total_energy",
        "rms_temp_error",
        "night_rms_temp_error",
        "state_changes",
        "mode_changes",
    ),
)


class RMSTempError:
    def __init__(self):
        self._sq_temp_errs = [0, 0, 0]
        self._cnt = 0

    def record(self, datapoint):
        if datapoint.room_temp_c > datapoint.cool_setpoint_c:
            err = (datapoint.room_temp_c - datapoint.cool_setpoint_c) ** 2
            self._sq_temp_errs[2] += err
        elif datapoint.room_temp_c < datapoint.heat_setpoint_c:
            err = (datapoint.room_temp_c - datapoint.heat_setpoint_c) ** 2
            self._sq_temp_errs[1] += err
        else:
            err = 0

        self._sq_temp_errs[0] += err
        self._cnt += 1

    def rms(self):
        return [math.sqrt(e / self._cnt) for e in self._sq_temp_errs]

    def __str__(self):
        return "{:.3f} (heat: {:.3f}, cool: {:.3f})".format(
            *(conversions.rel_c_to_f(t) for t in self.rms())
        )


class Simulator:
    def __init__(
        self,
        outdoor_temp_input,
        room,
        setpoint_schedule,
        hvac_system,
        max_interval_min=60,
    ):
        self._outdoor_temp_input = outdoor_temp_input
        self._room = room
        self._setpoint_schedule = setpoint_schedule
        self._hvac_system = hvac_system
        self._max_interval_s = max_interval_min * 60
        self._data = []
        self._state_changes = 0
        self._mode_changes = 0
        self._hvac_mode = hvac.HVACMode.OFF

    def run(self, step_s=1, plot_all=False):
        prev_power_w = 0
        prev_dt = None

        for dt, tc in self._outdoor_temp_input:
            heat_setpoint_c, cool_setpoint_c = self._setpoint_schedule.get(dt)
            system_power = self._hvac_system.get_power(
                self._room.air_temp_c, tc, heat_setpoint_c, cool_setpoint_c
            )
            mode = hvac.HVACMode.from_power(system_power.total_power_w)
            if mode != self._hvac_mode:
                self._hvac_mode = mode
                self._mode_changes += 1
            if system_power.total_power_w != prev_power_w:
                self._state_changes += 1

            total_energy = hvac.SystemEnergy()
            if prev_dt is not None:
                total_delta_s = (dt - prev_dt).total_seconds()
                curr_dt = dt
                while total_delta_s > 0:
                    curr_delta_s = min(total_delta_s, step_s)
                    # Limit the maximum time we apply the energy to avoid runaway issues
                    # if there's a big gap in outdoor temp data
                    system_energy = system_power.energy(
                        min(curr_delta_s, self._max_interval_s)
                    )
                    total_energy += system_energy
                    self._room.update(curr_delta_s, tc, system_energy.net_energy_j)

                    if plot_all:
                        self._data.append(
                            DataPoint(
                                curr_dt,
                                tc,
                                self._room.air_temp_c,
                                heat_setpoint_c,
                                cool_setpoint_c,
                                self._room.construction_temp_c,
                                self._room.surfaces_temp_c,
                                system_energy,
                            )
                        )
                        curr_dt += datetime.timedelta(seconds=curr_delta_s)

                    total_delta_s -= curr_delta_s

            if not plot_all:
                self._data.append(
                    DataPoint(
                        dt,
                        tc,
                        self._room.air_temp_c,
                        heat_setpoint_c,
                        cool_setpoint_c,
                        self._room.construction_temp_c,
                        self._room.surfaces_temp_c,
                        total_energy,
                    )
                )

            prev_dt = dt
            prev_power_w = system_power.total_power_w

    def stats(self):
        all_err = RMSTempError()
        night_err = RMSTempError()
        total_energy = hvac.SystemEnergy()
        for dp in self._data:
            all_err.record(dp)
            if dp.datetime.hour < 7 or dp.datetime.hour > 21:
                night_err.record(dp)

            total_energy += dp.system_energy

        return Stats(
            total_energy,
            all_err,
            night_err,
            self._state_changes,
            self._mode_changes,
        )

    def plot(self):
        # self._data = [d for d in self._data if d.datetime.hour == 5]

        dts = [dp.datetime for dp in self._data]
        out_tfs = [conversions.c_to_f(dp.outdoor_temp_c) for dp in self._data]
        room_tfs = [conversions.c_to_f(dp.room_temp_c) for dp in self._data]
        heat_set_tfs = [conversions.c_to_f(dp.heat_setpoint_c) for dp in self._data]
        cool_set_tfs = [conversions.c_to_f(dp.cool_setpoint_c) for dp in self._data]
        construction_tfs = [
            conversions.c_to_f(dp.construction_temp_c) for dp in self._data
        ]
        surfaces_tfs = [conversions.c_to_f(dp.surfaces_temp_c) for dp in self._data]
        energy_js = [dp.system_energy.net_energy_j for dp in self._data]

        fig, temp_ax = plt.subplots()

        # (construction_l,) = temp_ax.plot(
        #     dts, construction_tfs, label="Construction", color="darkgrey"
        # )
        # (surfaces_l,) = temp_ax.plot(
        #     dts, surfaces_tfs, label="Surfaces", color="lightgrey"
        # )
        (heat_set_l,) = temp_ax.plot(dts, heat_set_tfs, label="Heat", color="red")
        (cool_set_l,) = temp_ax.plot(dts, cool_set_tfs, label="Cool", color="blue")
        (room_l,) = temp_ax.plot(dts, room_tfs, label="Room")
        # (out_l,) = temp_ax.plot(dts, out_tfs, label="Outdoor")

        energy_ax = temp_ax.twinx()
        energy_ax.scatter(dts, energy_js, s=5)
        energy_ax.set_ylabel("Power(J)")

        temp_ax.set_ylabel("Temp(F)")
        temp_ax.legend()

        plt.show()
