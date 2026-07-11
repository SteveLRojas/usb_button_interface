import argparse
import time
import ubi_platform as ubi

def hex_byte(value):
    try:
        num = int(value, 0)    # accepts 0x27, 0X27, or even decimal
    except ValueError:
        raise argparse.ArgumentTypeError(f"'{value}' is not a valid number")

    if not 0 <= num <= 0xFF:
        raise argparse.ArgumentTypeError(f"{value} is outside the range 0x00-0xFF")

    return num


def main():
	parser = argparse.ArgumentParser(description="Configure HID key mappings")

	# SRAM / EEPROM
	group = parser.add_mutually_exclusive_group(required=True)
	group.add_argument("--sram", action="store_true", help="Store mapping in SRAM (temporary)")
	group.add_argument("--eeprom", action="store_true", help="Store mapping in EEPROM (persistent)")

	# Six switch mappings
	for i in range(6):
		parser.add_argument(f"--sw{i}", type=hex_byte, metavar="HEX", help=f"Key mapping for switch {i} (0x00-0xFF)")
		parser.add_argument(f"--sw{i}-pol", type=str.lower, choices=["no", "nc"], default="no", metavar="POLARITY", help=f"Switch {i} polarity (NO or NC, default: NO)")

	args = parser.parse_args()

	unique_id = ubi.init(0)
	if unique_id == 0:
		print("Could not find programming interface")
		exit(1)
	
	print(f"Device ID: {ubi.get_device_id():08X}")
	print(f"Unique ID: {ubi.get_unique_id():08X}")

	print(f"Blank key map address: {ubi.read_reg(ubi.regs.R_BLANK_MAP_ADDR):02X}")

	print("Current key map:")
	key_map = list(ubi.read_key_map())
	print(", ".join(f"{v:02X}" for v in key_map))

	key_map[0] = 0x08
	mask = 0x01
	for i in range(6):
		value = getattr(args, f"sw{i}")
		pol = getattr(args, f"sw{i}_pol")
		if value is not None:
			key_map[i + 2] = value
			key_map[1] &= ~mask
			if pol == "no":
				key_map[1] |= mask
		mask <<= 1

	ubi.write_key_map(key_map)
	if args.eeprom:
		ubi.write_reg(ubi.regs.R_WRITE_KEY_MAP, 0)
		time.sleep(0.25)

	print("New key map:")
	key_map = ubi.read_key_map()
	print(", ".join(f"{v:02X}" for v in key_map))

	if args.eeprom:
		print("EEPROM content:")
		eeprom = ubi.read_eeprom(0, 128)
		for i in range(0, len(eeprom), 8):
			print(", ".join(f"{v:02X}" for v in eeprom[i:i+8]))
	
	print("Done!")
	return 0

if __name__ == "__main__":
	main()
