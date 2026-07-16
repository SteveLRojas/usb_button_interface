#include "CH552.H"
#include "CH552_FLASH.h"
#include "CH552_GPIO.h"
#include "CH552_RCC.h"
#include "CH552_TKEY.h"
#include "CH552_USB.h"
#include "CH552_USB_CDC.h"
#include "CH552_USB_HID_KB.h"
#include "CH552_TIMER.h"

#define DEBOUNCE_SAMPLES 200

//Pins:
// SW0 = P11
// TKEY = P14
// SW7 = P15
// SW6 = P16
// SW3 = P17
// SW1 = P30
// SW2 = P31
// LED1 = P32
// SW5 = P33
// SW4 = P34
// UDP = P36
// UDM = P37

typedef struct _KEY_MAP {
	UINT8 len;
	UINT8 sw_pol;
	UINT8 sw_keys[6];
} key_map_t;

key_map_t key_map;

//HINT: returns an address out of range if all blocks are valid
UINT8 find_blank_key_map(void)
{
	UINT8 block_offset = 0;
	
	while((block_offset < 0x80) && (flash_read_byte(block_offset) == 8))
	{
		block_offset += 8;
	}
	return block_offset;
}

void load_key_map(void)
{
	UINT8 block_offset;
	
	block_offset = find_blank_key_map();
	block_offset -= 8;	//HINT: goes out of range if there are no valid blocks
	
	flash_read(block_offset, (UINT8*)&key_map, 8);
}

void write_key_map(void)
{
	UINT8 block_offset;
	
	block_offset = find_blank_key_map();
	if(block_offset > 0x7F)
	{
		flash_clear_range(0, 0, 0xFF);
		block_offset = 0x00;
	}
	
	flash_write(block_offset, (UINT8*)&key_map, 8);
}

int main(void)
{
	UINT8 tkey_time = 0;
	UINT8 dev_mode = 0;
	UINT8 prev_tkey_state;
	
	UINT8 prev_control_line_state;
	UINT8 datagram[2];
	UINT16 bytes_available;
	UINT8 temp;
	
	UINT8 map_idx;
	UINT8 flash_idx;
	
	UINT8 hid_kb_idle_time = 0;
	UINT8 sw_state;
	UINT8 sw_count[6];
	UINT8 sw_debounced = 0;
	UINT8 prev_sw_debounced = 0;
	
	rcc_set_clk_freq(RCC_CLK_FREQ_24M);
	
	gpio_set_mode(GPIO_MODE_INPUT, GPIO_PORT_1, GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_1);
	gpio_set_mode(GPIO_MODE_INPUT, GPIO_PORT_3, GPIO_PIN_4 | GPIO_PIN_3 | GPIO_PIN_1 | GPIO_PIN_0);
	gpio_set_mode(GPIO_MODE_PP, GPIO_PORT_3, GPIO_PIN_2);
	
	timer_init(TIMER_0, NULL);
	timer_set_period(TIMER_0, 4ul * FREQ_SYS / 1000ul);	//period is 4ms
	timer_init(TIMER_1, NULL);
	timer_set_period(TIMER_1, FREQ_SYS / 1000ul);	//period is 1ms
	EA = 1;	//enable interupts
	E_DIS = 0;
	
	RESET_KEEP = rcc_get_rst_typ();
	if((RESET_KEEP == RCC_RST_TYP_WDOG) || (RESET_KEEP == RCC_RST_TYP_SOFT))
	{
		rcc_delay_ms(500);
	}
	else
	{
		timer_long_delay(TIMER_1, 250);
	}
	
	tkey_init(TKEY_FAST);
	tkey_schedule[0] = TKEY_TIN2_P14;
	tkey_schedule_len = 1;
	tkey_init_schedule();
	tkey_calibrate();
	tkey_threshold = 0x200;
	tkey_run_schedule(1);
	while(tkey_pending_samples);
	prev_tkey_state = tkey_results[0];
	
	gpio_clear_pin(GPIO_PORT_3, GPIO_PIN_2);
	timer_long_delay(TIMER_1, 250);
	gpio_set_pin(GPIO_PORT_3, GPIO_PIN_2);
	timer_long_delay(TIMER_1, 250);
	gpio_clear_pin(GPIO_PORT_3, GPIO_PIN_2);

	timer_start(TIMER_0);
	load_key_map();
	hid_kb_init();
	
	if(key_map.len != 8)
	{
		sw_debounced = 0xFF;
		prev_sw_debounced = 0xFF;
	}
	
	for(map_idx = 0; map_idx < 6; ++map_idx)
	{
		sw_count[map_idx] = 0;
	}
	
	while(TRUE)
	{
		//Update timers
		if(timer_overflow_counts[TIMER_0])	//HINT: timer interval is 4ms
		{
			++tkey_time;
			++hid_kb_idle_time;
			timer_overflow_counts[TIMER_0] = 0;
		}
		
		//Update touck key state
		if(tkey_time >= 32)
		{
			tkey_run_schedule(1);
			tkey_time = 0;
		}
		
		//Handle mode switching
		if(tkey_results[0] && !prev_tkey_state)
		{
			dev_mode = !dev_mode;
			gpio_write_pin(GPIO_PORT_3, GPIO_PIN_2, dev_mode);
			usb_disconnect();
			timer_long_delay(TIMER_1, 250);
			cdc_config = 0;
			hid_kb_config = 0;
			if(dev_mode)
			{
				cdc_init();
				cdc_set_serial_state(CDC_SS_TXCARRIER | CDC_SS_RXCARRIER);
				prev_control_line_state = cdc_control_line_state;
			}
			else
			{
				hid_kb_init();
			}
		}
		prev_tkey_state = tkey_results[0];
		
		if(dev_mode && cdc_config)	//programming mode
		{
			bytes_available = cdc_bytes_available();
			temp = cdc_peek();
			if((bytes_available >= 2) && (temp & 0x80))	//Handle write datagram
			{
				cdc_read_bytes(datagram, 2);
			
				switch(datagram[0] & 0x7F)
				{
					case 0x08:
						map_idx = datagram[1];
						break;
					case 0x09:
						((UINT8*)&key_map)[map_idx & 0x07] = datagram[1];
						++map_idx;
						break;
					case 0x0A:
						load_key_map();
						break;
					case 0x0B:
						write_key_map();
						break;
					case 0x0D:
						flash_idx = datagram[1];
						break;
					case 0x0E:
						flash_write_byte(flash_idx & 0x7F, datagram[1]);
						++flash_idx;
						break;
					case 0x0F:
						flash_clear_range(0, 0, 0xFF);
						break;
				}
			}
			else if(bytes_available && !(temp & 0x80))	//handle read datagram
			{
				datagram[0] = cdc_read_byte();
			
				switch(datagram[0])
				{
					case 0x00:	//device_id[0]
						datagram[0] = 'U';
						break;
					case 0x01:	//device_id[1]
						datagram[0] = 'B';
						break;
					case 0x02:	//device_id[2]
						datagram[0] = 'I';
						break;
					case 0x03:	//device_id[3]
						datagram[0] = '1';
						break;
					
					case 0x04:	//unique_id[0]
						datagram[0] = (UINT8)(*(UINT16 code*)ROM_CHIP_ID_LO);
						break;
					case 0x05:	//unique_id[1]
						datagram[0] = (UINT8)((*(UINT16 code*)ROM_CHIP_ID_LO) >> 8);
						break;
					case 0x06:	//unique_id[2]
						datagram[0] = (UINT8)(*(UINT16 code*)ROM_CHIP_ID_HI);
						break;
					case 0x07:	//unique_id[3]
						datagram[0] = (UINT8)((*(UINT16 code*)ROM_CHIP_ID_HI) >> 8);
						break;
					
					case 0x08:
						datagram[0] = map_idx;
						break;
					case 0x09:
						datagram[0] = ((UINT8*)&key_map)[map_idx];
						++map_idx;
						break;
					case 0x0C:
						datagram[0] = find_blank_key_map();
						break;
					case 0x0D:
						datagram[0] = flash_idx;
						break;
					case 0x0E:
						datagram[0] = flash_read_byte(flash_idx);
						++flash_idx;
						break;
				}
				
				cdc_write_byte(datagram[0]);
			}
			else if(bytes_available)	//datagrams not received in a single transfer are ignored
			{
				cdc_read_bytes(datagram, bytes_available);	//get rid of the extra bytes
			}
		
			if(prev_control_line_state != cdc_control_line_state)
			{
				cdc_set_serial_state(cdc_control_line_state & 3);
				prev_control_line_state = cdc_control_line_state;
			}
		}
		else if(!dev_mode && hid_kb_config)	//functional mode
		{
			temp = 0x01;
			sw_state = 0;
			if(gpio_read_pin(GPIO_PORT_1, GPIO_PIN_1))	//SW0
				sw_state |= temp;
			temp <<= 1;
			if(gpio_read_pin(GPIO_PORT_3, GPIO_PIN_0))	//SW1
				sw_state |= temp;
			temp <<= 1;
			if(gpio_read_pin(GPIO_PORT_3, GPIO_PIN_1))	//SW2
				sw_state |= temp;
			temp <<= 1;
			if(gpio_read_pin(GPIO_PORT_1, GPIO_PIN_7))	//SW3
				sw_state |= temp;
			temp <<= 1;
			if(gpio_read_pin(GPIO_PORT_3, GPIO_PIN_4))	//SW4
				sw_state |= temp;
			temp <<= 1;
			if(gpio_read_pin(GPIO_PORT_3, GPIO_PIN_3))	//SW5
				sw_state |= temp;
			
			sw_state ^= key_map.sw_pol;
			
			temp = 0x01;
			for(map_idx = 0; map_idx < 6; ++map_idx)
			{	
				if(sw_state & temp)
					sw_count[map_idx] += (sw_count[map_idx] <= DEBOUNCE_SAMPLES);
				else
					sw_count[map_idx] -= (sw_count[map_idx] != 0);
				
				if(!sw_count[map_idx])
					sw_debounced &= ~temp;
				if(sw_count[map_idx] == DEBOUNCE_SAMPLES)
					sw_debounced |= temp;
				
				temp <<= 1;
			}
			
			temp = sw_debounced & ~prev_sw_debounced;
			for(map_idx = 0; map_idx < 6; ++map_idx)
			{
				if(temp & 0x01)
					hid_kb_press_key(key_map.sw_keys[map_idx]);
				temp >>= 1;
			}
			
			temp = prev_sw_debounced & ~sw_debounced;
			for(map_idx = 0; map_idx < 6; ++map_idx)
			{
				if(temp & 0x01)
					hid_kb_release_key(key_map.sw_keys[map_idx]);
				temp >>= 1;
			}
			
			prev_sw_debounced = sw_debounced;
			
			if(hid_kb_idle_rate && (hid_kb_idle_time >= hid_kb_idle_rate))
			{
				hid_kb_send_report();
				hid_kb_idle_time = 0;
			}
		}
	}
}

