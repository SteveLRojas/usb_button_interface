import serial
import serial.tools.list_ports
import time
import ubi_regs as regs

baud = 1000000
ser = serial.Serial()

def write_reg(address, value):
	address = address | 0x80
	bytes = address.to_bytes(1, 'big') + value.to_bytes(1, 'big')
	ser.write(bytes)

def read_reg(address):
	bytes = address.to_bytes(1, 'big')
	ser.write(bytes)
	response = ser.read(1)
	return int.from_bytes(response, 'big')

def get_device_id():
	response = read_reg(regs.R_DEVICE_ID_HH)
	response = (response << 8) | read_reg(regs.R_DEVICE_ID_HL) 
	response = (response << 8) | read_reg(regs.R_DEVICE_ID_LH)
	response = (response << 8) | read_reg(regs.R_DEVICE_ID_LL)
	return response

def get_unique_id():
	response = read_reg(regs.R_UNIQUE_ID_HH)
	response = (response << 8) | read_reg(regs.R_UNIQUE_ID_HL)
	response = (response << 8) | read_reg(regs.R_UNIQUE_ID_LH)
	response = (response << 8) | read_reg(regs.R_UNIQUE_ID_LL)
	return response

def read_key_map():
	bytes = bytearray()
	write_reg(regs.R_MAP_IDX, 0)
	for byte in range(8):
		bytes.append(regs.R_MAP_DATA)
	ser.write(bytes)
	return ser.read(8)

def write_key_map(key_map):
	bytes = bytearray()
	write_reg(regs.R_MAP_IDX, 0)
	for byte in key_map:
		bytes.append(regs.R_MAP_DATA | 0x80)
		bytes.append(byte)
	ser.write(bytes)

def read_eeprom(start_addr, num_bytes):
	bytes = bytearray()
	write_reg(regs.R_EEPROM_IDX, start_addr)
	for byte in range(num_bytes):
		bytes.append(regs.R_EEPROM_DATA)
	ser.write(bytes)
	return ser.read(num_bytes)

def write_eeprom(start_addr, data):
	bytes = bytearray()
	write_reg(regs.R_EEPROM_IDX, start_addr)
	for byte in data:
		bytes.append(regs.R_EEPROM_DATA | 0x80)
		bytes.append(byte)
	ser.write(bytes)

def init(unique_id = 0):
	display_ports = []
	ports = serial.tools.list_ports.comports()
	for port in ports:
		if hasattr(port, 'vid') and port.vid and (port.vid == 0x1A86) and (port.pid == 0xFE0C):
			display_ports.append(port.device)
	
	ser.baudrate = baud
	ser.dsrdtr = False
	ser.dtr = False
	ser.timeout = 1.0
	ser.write_timeout = 1.0

	for port in display_ports:
		try:
			ser.port = port
			ser.open()
			time.sleep(0.5)
			ser.reset_input_buffer()
		except Exception:
			continue

		device_id = get_device_id()
		if device_id != 0x31494255:
			ser.close()
			continue
		dut_unique_id = get_unique_id()
		if unique_id and (unique_id != dut_unique_id):
			ser.close()
			continue
		return dut_unique_id
	return 0

def stop():
	if ser.is_open:
		ser.close()
