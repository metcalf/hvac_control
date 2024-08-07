AIR_HEAT_CAPACITY_J_M3K = 1200
CONSTRUCTION_U_VALUE = 1.5  # Mostly a guess
CONSTRUCTION_HEAT_CAPACITY_J_KGK = 1020  # ~drywall
CONSTRUCTION_DENSITY_KG_M3 = 750  # ~drywall


def compute_watts(t1_c, t2_c, transmittance_w_k):
    return (t2_c - t1_c) * transmittance_w_k


class Room:
    def __init__(
        self,
        volume_m3,
        transmittance_rate_w_k,
        surfaces_thermal_mass_j_k,
        surfaces_transmittance_rate_w_k,
        construction_area=None,
        construction_thermal_mass_j_k=None,
    ):
        self._air_thermal_mass_j_k = volume_m3 * AIR_HEAT_CAPACITY_J_M3K
        self._air_temp_c = None
        self._construction_temp_c = None

        if construction_area is None:
            # Roughly assume that the surface area of the construction and fixtures is the
            # double the surface area of a cube with this volume.
            construction_area = area = 6 * (volume_m3 ** (2.0 / 3))
        self._construction_area = construction_area

        # Estimate construction thermal mass to be 5/8" drywall covering the estimated area
        if construction_thermal_mass_j_k is None:
            volume = area * 0.016  # 5/8" drywall
            mass = volume * CONSTRUCTION_DENSITY_KG_M3
            construction_thermal_mass_j_k = CONSTRUCTION_HEAT_CAPACITY_J_KGK * mass
        self._construction_thermal_mass_j_k = construction_thermal_mass_j_k

        self._surfaces_thermal_mass_j_k = surfaces_thermal_mass_j_k

        self._room_surfaces_transmittance_rate_w_k = surfaces_transmittance_rate_w_k
        self._room_construction_transmittance_rate_w_k = (
            CONSTRUCTION_U_VALUE * self._construction_area
        )
        # Resistivity is additive so take the inverse, operate, then invert again
        self._construction_out_transmittance_rate_w_k = 1 / (
            1.0 / transmittance_rate_w_k
            - 1.0 / self._room_construction_transmittance_rate_w_k
        )

    @staticmethod
    def est_transmittance_rate(outdoor_temp_c, design_temp_c, load_watts):
        # this ignores impacts of wind, should be fine
        return load_watts / (design_temp_c - outdoor_temp_c)

    # Positive energy is heating, negative energy is cooling
    def update(self, time_s, out_temp_c, input_energy_j):
        if self._air_temp_c is None:
            raise Exception("Must call init_temp_c first")

        room_construction_j = time_s * compute_watts(
            self._air_temp_c,
            self._construction_temp_c,
            self._room_construction_transmittance_rate_w_k,
        )
        construction_out_j = time_s * compute_watts(
            self._construction_temp_c,
            out_temp_c,
            self._construction_out_transmittance_rate_w_k,
        )
        room_surfaces_j = time_s * compute_watts(
            self._air_temp_c,
            self._surfaces_temp_c,
            self._room_surfaces_transmittance_rate_w_k,
        )

        self._air_temp_c = (
            self._air_temp_c
            + (room_construction_j + room_surfaces_j + input_energy_j)
            / self._air_thermal_mass_j_k
        )
        self._construction_temp_c = (
            self._construction_temp_c
            + (construction_out_j - room_construction_j)
            / self._construction_thermal_mass_j_k
        )
        self._surfaces_temp_c = (
            self._surfaces_temp_c - room_surfaces_j / self._surfaces_thermal_mass_j_k
        )

    def init_temp_c(self, init_t_c):
        self._air_temp_c = init_t_c
        self._construction_temp_c = init_t_c
        self._surfaces_temp_c = init_t_c

    @property
    def air_temp_c(self):
        return self._air_temp_c

    @property
    def construction_temp_c(self):
        return self._construction_temp_c

    @property
    def surfaces_temp_c(self):
        return self._surfaces_temp_c
