# usb_button_interface
Interface module that maps 6 button inputs to keyboard keys.  

## What is in this repo?
Here you will find the design files for a small custom PCB that lets you connect buttons or switches to a PC via USB and map them to keyboard buttons.  
The hardware is designed in KiCad, the design files are in the HW folder.  
The firmware is written in C and meant to be compiled with Keil uVison C51 and linked with LX51. The project makes extensive use of pointers and requires a linker script for correct memory overlay.  
The entire uVision project is in the FW folder.  
The SW folder contains test scripts as well as a command line tool (ubi_programmer.py) used to set the key mapping. To set the key mapping you need ubi_programmer.py, ubi_platform.py, and ubi_regs.py. The other files are test scripts.  

## How do I use this thing?
First you will need the hardware, boards can be ordered from JLCPCB and all the components are available from LCSC.  
Once you have working hardware close the jumper J6 to put it in programming mode, compile the firmware, and use WCHISP or a similar program to upload the firmware.  
After the firmware is uploaded remove jumper J6 to put the device in functional mode. Now when it is plugged into USB the LED will blink and then stay on, this means the device is in functional mode.  
Touching the capacitive touch button will make the LED turn off, this means the device is in configuration mode. Touching the button again will put the device back into functional mode.  
Note that the capacitive button is automatically calibrated each time the device is powered on, so while powering on the device make sure it is placed in its normal operating position and don't touch the capacitive button.  
When the device is in functional mode it should appear to the PC as a completely satandard USB keyboard, when it is in configuration mode it will appear as a CDC ACM device.  
There is no need to specify a serial port when running the configuration script, as it can scan the serial ports and automatically find the device.  

## How do I set the key mapping?
Use the provided python script (ubi_programmer.py). Run it with -h to see what arguments it supports.  
Example usage: python ubi_programmer.py --eeprom --sw0 0x1A --sw1 0x29  
The above example maps sw0 to the 'W' key, and sw1 to the escape key. The hexadecimal numbers representing each keyboard key can be found in the document "HID Usage Tables for Universal Serial Bus (USB)".  
The first time you configure the device you should specify all the switches (even if you don't intend to use them) to make sure nothing is mapped to an invalid key.  
