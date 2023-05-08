
#include "1wire_com.h"
#include "goods.h"
#include "string.h"

#include <stm32f1xx.h>


int LastDiscrepancy;
bool LastDeviceFlag;
uint8_t wire1_ROM_NO[8];


volatile enum e_Wire1_States    g_W1_State     = Wire1State_Off;
volatile enum e_Wire1_InitState g_1W_InitState = Wire1_InitState_NoDevice;

uint8_t g_Wire1_Buff[10];
int g_Wire1_StrongLast0Pulse = 0;   // максимально удлинить последний передаваемый 0 (перед запуском измерения температуры 0x44)



//--------------------------------------------------------------------------------------------
bool wire1_search(bool first)
{
	// пытаемся найти все устройства на шине 1-wire
	// алгоритм взят из http://www.sal.wisc.edu/pfis/docs/rss-nir/archive/public/Product%20Manuals/maxim-ic/AN187.pdf


	int id_bit_number;
	int search_result;
	int last_zero, rom_byte_number;
	int id_bit, cmp_id_bit;
	unsigned char rom_byte_mask, search_direction;

	// initialize for search
	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = false;

	if(first)
	{
		// reset the search state
		LastDiscrepancy = 0;
		LastDeviceFlag = false;
	}


	// if the last call was not the last one
	if (!LastDeviceFlag)
	{
		// 1-Wire reset before each next search
		if (wire1_init() != Wire1_InitState_Done_OK)
		{
			// reset the search
			LastDiscrepancy = 0;
			LastDeviceFlag = false;
			return false;
		}

		// issue the search command
		wire1_init_buff_clr(0xF0);  // отправляем команду Search Rom
		wire1_transact(8);   //отправляем команду

		// loop to do the search
		do
		{
			// read a bit and its complement

			g_Wire1_Buff[0] = 0xff;
			wire1_transact(2);


			id_bit     = (g_Wire1_Buff[0] >> 6) & 1;
			cmp_id_bit = (g_Wire1_Buff[0] >> 7) & 1;

			// check for no devices on 1-wire
			if ((id_bit == 1) && (cmp_id_bit == 1))
			{
				break;
			}
			else
			{
				// all devices coupled have 0 or 1
				if (id_bit != cmp_id_bit)
					search_direction = id_bit;  // bit write value for search
				else
				{
					// if this discrepancy if before the Last Discrepancy
					// on a previous next then pick the same as last time
					if (id_bit_number < LastDiscrepancy)
						search_direction = ((wire1_ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
					else
						// if equal to last pick 1, if not then pick 0
						search_direction = (id_bit_number == LastDiscrepancy);

					// if 0 was picked then record its position in LastZero
					if (search_direction == 0)
					{
						last_zero = id_bit_number;
					}
				}

				// set or clear the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if (search_direction == 1)
					wire1_ROM_NO[rom_byte_number] |= rom_byte_mask;
				else
					wire1_ROM_NO[rom_byte_number] &= ~rom_byte_mask;

				// serial number search direction write bit
				// отправляем бит обратно устройству
				g_Wire1_Buff[0] = search_direction;
				wire1_transact(1);

				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;

				// if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
				if (rom_byte_mask == 0)
				{
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		}
		while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

		// if the search was successful then
		if (!(id_bit_number < 65))
		{
			// search successful so set LastDiscrepancy,LastDeviceFlag,search_result
			LastDiscrepancy = last_zero;

			// check for last device
			if (LastDiscrepancy == 0)
				LastDeviceFlag = true;

			search_result = true;
		}
	}

	// if no device found then reset counters so next 'search' will be like a first
	if (!search_result || !wire1_ROM_NO[0])
	{
		LastDiscrepancy = 0;
		LastDeviceFlag = false;
		search_result = false;
	}

	return search_result;
}
//--------------------------------------------------------------------------------------------
void wire1_read_single_address(void)
{
//	wire1_uart_init();
//	wire1_uart_init_buff(0x33);
//	wire1_uart_start(72);
//	wire1_uart_wait4done();
}
//--------------------------------------------------------------------------------------------
void wire1_init_buff_clr(uint8_t cmd)
{
	// инициализируем буфер перед чтением:
	// в g_Wire1_Buff[0] кладём команду,
	// остальные 8 байт заливаем FF
	g_Wire1_Buff[0] = cmd;
	memset((void *)(g_Wire1_Buff + 1), 0xff, NUMOFARRAY(g_Wire1_Buff) - 1);
}
//--------------------------------------------------------------------------------------------
void wire1_init_buff_cpy(uint8_t cmd, uint8_t *src, int byte_num)
{
	// инициализируем буфер перед чтением:
	// в g_Wire1_Buff[0] кладём команду,
	// в остальные копируем содержимое src длиной byte_num байт
	g_Wire1_Buff[0] = cmd;
	int to_copy = min(byte_num, NUMOFARRAY(g_Wire1_Buff) - 1);
	if(src) memcpy((void *)(g_Wire1_Buff + 1), src, to_copy);
}
//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------


