from collections import namedtuple
import math
import matplotlib.pyplot as plt
import conversions
import datetime

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
        "energy_j",
    ),
)
Stats = namedtuple(
    "Stats",
    (
        "heating_energy_kj",
        "cooling_energy_kj",
        "rms_temp_error",
        "night_rms_temp_error",
        "state_changes",
    ),
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
        self._heating_energy_j = 0
        self._cooling_energy_j = 0
        self._state_changes = 0

    def run(self, step_s=1, plot_all=False):
        prev_energy_w = None
        prev_dt = None

        for dt, tc in self._outdoor_temp_input:
            heat_setpoint_c, cool_setpoint_c = self._setpoint_schedule.get(dt)
            energy_w = self._hvac_system.get_energy(
                self._room.air_temp_c, tc, heat_setpoint_c, cool_setpoint_c
            )

            if energy_w != prev_energy_w:
                self._state_changes += 1

            total_energy_j = 0
            if prev_dt is not None:
                total_delta_s = (dt - prev_dt).total_seconds()
                curr_dt = dt
                while total_delta_s > 0:
                    curr_delta_s = min(total_delta_s, step_s)
                    # Limit the maximum time we apply the energy to avoid runaway issues
                    # if there's a big gap in outdoor temp data
                    energy_j = energy_w * min(curr_delta_s, self._max_interval_s)
                    total_energy_j += energy_j
                    self._room.update(curr_delta_s, tc, energy_j)

                    if energy_j > 0:
                        self._heating_energy_j += energy_j
                    else:
                        self._cooling_energy_j += -energy_j

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
                                energy_j,
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
                        total_energy_j,
                    )
                )

            prev_dt = dt
            prev_energy_w = energy_w

    def stats(self):
        sq_temp_err = 0
        night_sq_temp_err = 0
        night_cnt = 0
        for dp in self._data:
            if dp.room_temp_c > dp.cool_setpoint_c:
                err = dp.room_temp_c - dp.cool_setpoint_c
            elif dp.room_temp_c < dp.heat_setpoint_c:
                err = dp.room_temp_c - dp.heat_setpoint_c
            else:
                err = 0

            sq_err = err**2
            sq_temp_err += sq_err
            if dp.datetime.hour < 7 or dp.datetime.hour > 8:
                night_sq_temp_err += sq_err
                night_cnt += 1

        return Stats(
            self._heating_energy_j / 1000.0,
            self._cooling_energy_j / 1000.0,  # TODO: Split out fan cooling from A/C
            math.sqrt(sq_temp_err / len(self._data)),
            math.sqrt(night_sq_temp_err / night_cnt),
            self._state_changes,
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
        energy_js = [conversions.c_to_f(dp.energy_j) for dp in self._data]

        fig, temp_ax = plt.subplots()

        (construction_l,) = temp_ax.plot(
            dts, construction_tfs, label="Construction", color="darkgrey"
        )
        (surfaces_l,) = temp_ax.plot(
            dts, surfaces_tfs, label="SURFACES Fixtures", color="lightgrey"
        )
        (heat_set_l,) = temp_ax.plot(dts, heat_set_tfs, label="Heat", color="red")
        (cool_set_l,) = temp_ax.plot(dts, cool_set_tfs, label="Cool", color="blue")
        (room_l,) = temp_ax.plot(dts, room_tfs, label="Room")
        (out_l,) = temp_ax.plot(dts, out_tfs, label="Outdoor")

        # energy_ax = temp_ax.twinx()
        # energy_ax.scatter(dts, energy_js, s=5)

        temp_ax.set_ylabel("Temp(F)")
        temp_ax.legend()

        plt.show()
