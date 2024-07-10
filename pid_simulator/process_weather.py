import sys
import os.path
import glob
import json
import csv
import conversions


def preprocess_weather(directory):
    outfile = directory.rstrip("/") + ".csv"

    json_files = glob.glob(os.path.join(directory, "*.json"))

    data = []
    with open(outfile, "w") as csvfile:
        csvwriter = csv.writer(csvfile)
        csvwriter.writerow(("obsTimeLocal", "tempAvgC"))

        for json_file in json_files:
            with open(json_file, "r") as file:
                day = json.load(file)

                for record in day:
                    csvwriter.writerow(
                        (
                            record["obsTimeLocal"],
                            "%.1f" % conversions.f_to_c(record["imperial"]["tempAvg"]),
                        )
                    )


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        if not os.path.isdir(arg):
            continue

        preprocess_weather(arg)
