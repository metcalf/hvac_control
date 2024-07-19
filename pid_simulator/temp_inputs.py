import conversions
import csv
import datetime
import math

FIXED_START = datetime.datetime(2000, 1, 1)
SECS_PER_DAY = 60 * 60 * 24


class InterpolatedInput:
    # TODO: Implement max_jump
    def __init__(self, input, interval=datetime.timedelta(minutes=1)):
        if interval.microseconds != 0:
            raise ValueError("microsecond intervals not supported")

        self._input = input
        self._interval = interval

    def __iter__(self):
        prev_dt = None
        prev_tc = None

        for in_dt, in_tc in self._input:
            if prev_dt is None:
                prev_dt = in_dt
                prev_tc = in_tc
                continue

            yield (prev_dt, prev_tc)

            curr_dt = prev_dt.replace()  # Copy
            curr_tc = prev_tc
            curr_slope = float(in_tc - prev_tc) / (in_dt - prev_dt).total_seconds()

            while curr_dt < in_dt:
                yield (curr_dt, curr_tc)
                curr_dt += self._interval
                curr_tc += curr_slope * self._interval.total_seconds()

            prev_dt = in_dt
            prev_tc = in_tc


class FixedInput:
    def __init__(self, temp, duration, interval=datetime.timedelta(seconds=30)):
        self._temp = temp
        self._duration = duration
        self._interval = interval

    def __iter__(self):
        current = FIXED_START
        end = current + self._duration

        while current <= end:
            yield (current, self._temp)
            current += self._interval


class SineInput:
    def __init__(
        self,
        center_temp_c,
        amplitude_c,
        duration=datetime.timedelta(days=1),
        interval=datetime.timedelta(seconds=30),
    ):
        self._fixed_input = FixedInput(center_temp_c, duration, interval)
        self._amplitude_c = amplitude_c

    def __iter__(self):
        for dt, offset in self._fixed_input:
            yield dt, offset + self._amplitude_c * math.sin(
                2 * math.pi * (dt - FIXED_START).total_seconds() / float(SECS_PER_DAY)
            )


class WeatherInput:
    @staticmethod
    def from_csv(path, after=None, before=None):
        data = []
        seen_dts = set()

        with open(path) as csvfile:
            reader = csv.reader(csvfile)
            next(reader)  # skip header
            for dts, tc in reader:
                dt = datetime.datetime.strptime(dts, "%Y-%m-%d %H:%M:%S")
                if after is not None and after > dt:
                    continue
                if before is not None and before < dt:
                    continue
                if dt in seen_dts:
                    print("duplicate weather data: ", dt)
                    continue
                seen_dts.add(dt)

                data.append((dt, float(tc)))

        print("Read %d weater measurements" % len(data))

        return WeatherInput(sorted(data))

    def __init__(self, data):
        self._data = data

    def __iter__(self):

        return iter(self._data)
