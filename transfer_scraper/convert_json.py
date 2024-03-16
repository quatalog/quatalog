import json
import sys
import collections


def main():
    if len(sys.argv) != 3:
        print(
            f"USAGE: python {sys.argv[0]} <json from scraper> <by-course output file>",
            file=sys.stderr,
        )
        return 1
    with open(sys.argv[1], "r") as scraper_json:
        by_institution = json.load(scraper_json)

    by_rpi_course = collections.defaultdict(list)
    for inst in by_institution:
        for xfer in inst["transfers"]:
            for rpi_course in xfer["rpi"]["courses"]:
                for a in ["institution", "city", "state"]:
                    xfer[a] = inst[a]
                by_rpi_course[rpi_course["id"]].append(xfer)

    with open(sys.argv[2], "w") as out_json:
        json.dump(by_rpi_course, out_json, sort_keys=True, indent=2)


if __name__ == "__main__":
    exit(main())
