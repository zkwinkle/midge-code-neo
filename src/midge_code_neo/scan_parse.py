import argparse
import pathlib
from datetime import datetime

from midge_badge_framework.files import ScanFileEntry
from midge_badge_framework.protocol import mingle_midge_batt_mv_to_percent


def mac_address_to_str(mac_address: bytes) -> str:
    return ":".join(f"{b:02x}" for b in mac_address)


def main():
    parser = argparse.ArgumentParser(description="Parse scan data from input file")
    parser.add_argument("input_file", type=str, help="Input file to parse")
    parser.add_argument("-o", "--output", type=str, default=None, help="Output file (default: stdout)")

    args = parser.parse_args()

    # Open input file
    input_path = pathlib.Path(args.input_file)

    # Determine output destination
    output_path = args.output if (args.output) else "/dev/stdout"
    with open(output_path, "w") as output_file:
        output_file.write("addr, group, badgeID, rssi, battery %, active_sensors ,timestamp\n")
        with open(input_path, "rb") as file:
            while True:
                a = ScanFileEntry()
                sz = file.readinto(a)
                if not sz:
                    break

                adv_data = a.advertised_data
                group = adv_data.badge_assignment.id.group
                badge_id = adv_data.badge_assignment.id.badge
                batt_p = mingle_midge_batt_mv_to_percent(adv_data.battery_mv)
                active_sensors = adv_data.active_sensor_bitflags
                timestamp = datetime.fromtimestamp(a.timestamp / 1000.0).isoformat()
                output_file.write(
                    f"{mac_address_to_str(a.mac_address):16},{group:2},{badge_id:4},{a.rssi:3},{batt_p:3},{active_sensors:05b},{timestamp}\n"
                )
