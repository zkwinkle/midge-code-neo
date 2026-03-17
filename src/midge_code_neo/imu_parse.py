import argparse
import pathlib

from midge_badge_framework.files import ImuAccelEntry, ImuGyroEntry, ImuMagnetoEntry, RotationVectorEntry


def main():
    parser = argparse.ArgumentParser(description="Parse scan data from input file")
    parser.add_argument("input_file", type=str, help="Input file to parse")
    parser.add_argument(
        "-t",
        "--type",
        type=str,
        choices=["accel", "gyro", "magneto", "rotation"],
        required=True,
        help="Type of IMU data to parse",
    )
    parser.add_argument("-o", "--output", type=str, default=None, help="Output file (default: stdout)")

    args = parser.parse_args()

    # Open input file
    input_path = pathlib.Path(args.input_file)

    # Determine output destination
    output_path = args.output if (args.output) else "/dev/stdout"
    entry_class = None
    match args.type:
        case "accel":
            entry_class = ImuAccelEntry
        case "gyro":
            entry_class = ImuGyroEntry
        case "magneto":
            entry_class = ImuMagnetoEntry
        case "rotation":
            entry_class = RotationVectorEntry

    with open(output_path, "w") as output_file:
        header = (
            f"{entry_class._fields_[0][0]}, "
            f"{entry_class._fields_[1][0]}, "
            f"{entry_class._fields_[2][0]}, "
            f"{entry_class._fields_[3][0]}, "
            f"{entry_class._fields_[4][0]}\n"
        )
        output_file.write(header)
        with open(input_path, "rb") as file:
            while True:
                a = entry_class()
                sz = file.readinto(a)
                if not sz:
                    break

                timestamp = str(a.timestamp)
                if isinstance(a, RotationVectorEntry):
                    line = f"{timestamp}, {a.i}, {a.j}, {a.k}, {a.real}\n"
                else:
                    line = f"{timestamp}, {a.x}, {a.y}, {a.z}, {a.reserved}\n"
                output_file.write(line)
