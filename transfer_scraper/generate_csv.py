import json
import sys
import collections
import csv
import os


def main():
    if len(sys.argv) != 3:
        print(
            f"USAGE: python {sys.argv[0]} <by-course json> <output dir>",
            file=sys.stderr,
        )
        return 1
    with open(sys.argv[1], "r") as transfer_json:
        transfer_data = json.load(transfer_json)

    if not os.path.exists(sys.argv[2]):
        os.makedirs(sys.argv[2])

    for course, data in transfer_data.items():
        print(f"Generating {course} transfer guide...", file=sys.stderr)
        csv_output = [
            (
                "City",
                "State",
                "Institution",
                "Transfer Course ID",
                "Transfer Course Name",
                "RPI Course ID",
                "RPI Course Name",
                "Note",
                "Begin",
                "End",
                "Transfer Catalog",
            )
        ]

        for xfer in data:
            csv_output.append(
                (
                    xfer["city"],
                    xfer["state"],
                    xfer["institution"],
                    " + ".join([x["id"] for x in xfer["transfer"]["courses"]]),
                    " + ".join([x["name"] for x in xfer["transfer"]["courses"]]),
                    " + ".join([x["id"] for x in xfer["rpi"]["courses"]]),
                    " + ".join([x["name"] for x in xfer["rpi"]["courses"]]),
                    xfer["note"],
                    xfer["begin"],
                    xfer["end"],
                    xfer["transfer"]["catalog"],
                )
            )

        with open(f"{sys.argv[2]}/{course} Transfer Guide.csv", "w") as course_csv:
            csv.writer(course_csv).writerows(csv_output)


if __name__ == "__main__":
    exit(main())
