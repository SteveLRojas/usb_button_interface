import time
import ubi_platform as ubi

def main():
	unique_id = ubi.init(0)
	if unique_id == 0:
		print("Could not find programming interface")
		exit()
	
	print(f"Device ID: {ubi.get_device_id():08X}")
	print(f"Unique ID: {ubi.get_unique_id():08X}")

	print(f"Blank key map address: {ubi.read_reg(ubi.regs.R_BLANK_MAP_ADDR):02X}")

	key_map = [0x08, 0xFF, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23]
	ubi.write_key_map(key_map)
	ubi.write_reg(ubi.regs.R_WRITE_KEY_MAP, 0)
	time.sleep(0.25)

	print("Key map:")
	key_map = ubi.read_key_map()
	print(", ".join(f"{v:02X}" for v in key_map))

	print("EEPROM:")
	eeprom = ubi.read_eeprom(0, 128)
	for i in range(0, len(eeprom), 8):
		print(", ".join(f"{v:02X}" for v in eeprom[i:i+8]))

if __name__ == "__main__":
	main()