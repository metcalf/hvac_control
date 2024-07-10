from room import Room
from temp_inputs import WeatherInput, FixedInput, SineInput
import conversions
import datetime
import setpoints
from simulator import Simulator
import hvac


def print_stats(stats):
    print("Heating energy (kJ): %d" % stats.heating_energy_kj)
    print("Cooling energy (kJ): %d" % stats.cooling_energy_kj)
    print("Temp error: %.1f" % stats.rms_temp_error)
    print("Night temp error: %.1f" % stats.night_rms_temp_error)


mbr = Room(
    conversions.f3_to_m3(2100),
    Room.est_transmittance_rate(
        outdoor_temp_c=conversions.f_to_c(40),
        design_temp_c=conversions.f_to_c(69),
        load_watts=conversions.btuhr_to_w(1722),
    ),
)
mbr.init_temp_c(conversions.f_to_c(70))

temps = WeatherInput.from_csv(
    "./weather/KCASANFR1969.csv", after=datetime.datetime(2024, 7, 1)
)
# temps = FixedInput(conversions.f_to_c(40), duration=datetime.timedelta(days=2))
# temps = SineInput(conversions.f_to_c(60), conversions.rel_f_to_c(12))

setpoint_schedule = setpoints.FixedSetpoint(
    conversions.f_to_c(67), conversions.f_to_c(72)
)
hvac_system = hvac.SimpleThermostat(heat_w=500, cool_w=500)

print("starting run")
sim = Simulator(temps, mbr, setpoint_schedule, hvac_system)
sim.run()
print("ran")

print_stats(sim.stats())
sim.plot()
