from room import Room
from temp_inputs import WeatherInput, FixedInput, SineInput, InterpolatedInput
import conversions
import datetime
import setpoints
from simulator import Simulator
import hvac


def print_stats(stats):
    print("Energy (kwh): %s" % stats.total_energy)
    print("Temp error: %s" % stats.rms_temp_error)
    print("Night temp error: %s" % stats.night_rms_temp_error)
    # TODO: Maybe want state/mode changes per component?
    # print("State changes: %d" % stats.state_changes)
    print("Mode changes: %d" % stats.mode_changes)


def create_room():
    mbr = Room(
        conversions.f3_to_m3(2100),
        Room.est_transmittance_rate(
            outdoor_temp_c=conversions.f_to_c(40),
            design_temp_c=conversions.f_to_c(69),
            load_watts=conversions.btuhr_to_w(1722),
        ),
        surfaces_thermal_mass_j_k=conversions.lb_to_kg(1300)
        * 1500,  # 1500 J/kg*K is somewhere between cotton and wood
        # Wow is this number made up to provide enough damping to make the air temp
        # data look realistic
        surfaces_transmittance_rate_w_k=2000,
    )
    mbr.init_temp_c(conversions.f_to_c(70))

    return mbr


temps = InterpolatedInput(
    WeatherInput.from_csv(
        "./weather/KCASANFR1934.csv",
        # after=datetime.datetime(2023, 7, 1),
        # before=datetime.datetime(2024, 2, 1),
    ),
    interval=datetime.timedelta(minutes=1),
)
# temps = FixedInput(conversions.f_to_c(70), duration=datetime.timedelta(hours=1))
# temps = SineInput(
#     conversions.f_to_c(60),
#     conversions.rel_f_to_c(12),
#     duration=datetime.timedelta(days=7),
#     interval=datetime.timedelta(minutes=1),
# )

# setpoint_schedule = setpoints.FixedSetpoint(
#     conversions.f_to_c(67), conversions.f_to_c(72)
# )
setpoint_schedule = setpoints.DayNightSetpoint(
    day_heat=conversions.f_to_c(67),
    day_cool=conversions.f_to_c(72),
    night_heat=conversions.f_to_c(66),
    night_cool=conversions.f_to_c(68),
    precool_rate_c_hr=conversions.rel_f_to_c(1),
)
# hvac_system = hvac.FixedOutput(250)

# These are based on the fan power consumption at various levels adjusted up a bit since
# lower speeds produce more output per CFM.
fancoil_levels = [
    -1,  # H
    -0.8,  # M
    -0.7,  # L
    0,
    0.5,  # ultra-low... wild guess
    0.7,  # L
    0.8,  # M
    1,  # H
]

on_off_system = hvac.System(
    {
        "heat": hvac.HysteresisThermostat(power_w=500, is_heater=True),
        "ac": hvac.TempDelay(
            temp_delay_c=conversions.rel_f_to_c(4),
            component=hvac.HysteresisThermostat(power_w=500, is_heater=False),
        ),
        "fan": hvac.FanCooling(conversions.ft3_to_m3(150)),
    }
)
p_system = hvac.PIDSystem(
    {
        "heat": hvac.Thermostat(power_w=500, is_heater=True),
        "ac": hvac.TempDelay(
            temp_delay_c=conversions.rel_f_to_c(4),
            component=hvac.Thermostat(power_w=500, is_heater=False),
        ),
        "fan": hvac.FanCooling(conversions.ft3_to_m3(150)),
    },
    p_range_c=conversions.rel_f_to_c(2),
    t_i=2,
)
no_fan_system = hvac.PIDSystem(
    {
        "heat": hvac.DiscreteLevels(
            hvac.Thermostat(power_w=500, is_heater=True), fancoil_levels
        ),
        "ac": hvac.DiscreteLevels(
            hvac.Thermostat(power_w=500, is_heater=False),
            fancoil_levels,
        ),
    },
    p_range_c=conversions.rel_f_to_c(2),
    t_i=2,
)
hvac_system = p_system


def fancoil_system(t_i=1):
    return hvac.PIDSystem(
        {
            "heat": hvac.DiscreteLevels(
                hvac.Thermostat(power_w=500, is_heater=True), fancoil_levels
            ),
            "ac": hvac.DiscreteLevels(
                hvac.TempDelay(
                    temp_delay_c=conversions.rel_f_to_c(4),
                    component=hvac.Thermostat(power_w=500, is_heater=False),
                ),
                fancoil_levels,
            ),
            "fan": hvac.FanCooling(conversions.ft3_to_m3(150)),
        },
        p_range_c=conversions.rel_f_to_c(2),
        t_i=t_i,
    )


for hvac_system in [fancoil_system(t_i) for t_i in (2, 4, 6, 8)]:
    print("starting run")
    mbr = create_room()
    sim = Simulator(temps, mbr, setpoint_schedule, hvac_system)
    sim.run(step_s=20, plot_all=False)

    print_stats(sim.stats())
    # sim.plot()
