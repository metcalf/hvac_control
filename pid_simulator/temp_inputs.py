import conversions
import csv
import datetime
import math

FIXED_START = datetime.datetime(2000, 1, 1)
SECS_PER_DAY = 60 * 60 * 24


class FixedInput:
    def __init__(self, temp, duration, interval=datetime.timedelta(seconds=30)):
        self._temp = temp
        self._current = FIXED_START
        self._end = self._current + duration
        self._interval = interval

    def __iter__(self):
        return self

    def __next__(self):
        if self._current > self._end:
            raise StopIteration

        dt = self._current
        self._current += self._interval

        return (dt, self._temp)


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
        return self

    def __next__(self):
        dt, offset = next(self._fixed_input)

        return dt, offset + self._amplitude_c * math.sin(
            2 * math.pi * (dt - FIXED_START).total_seconds() / float(SECS_PER_DAY)
        )


class WeatherInput:
    @staticmethod
    def from_csv(path, after=None, before=None):
        data = []

        with open(path) as csvfile:
            reader = csv.reader(csvfile)
            next(reader)  # skip header
            for dts, tc in reader:
                dt = datetime.datetime.strptime(dts, "%Y-%m-%d %H:%M:%S")
                if after is not None and after > dt:
                    continue
                if before is not None and before < dt:
                    continue

                data.append((dt, float(tc)))

        return WeatherInput(sorted(data))

    def __init__(self, data):
        self._data = data
        self._iter = iter(data)

    def __iter__(self):
        return iter(self._data)
