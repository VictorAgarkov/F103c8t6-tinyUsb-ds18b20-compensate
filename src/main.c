/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 *
 * Стенд для калибровки DS18B20 на 0 градусов.
 *
 * Назначение проекта: запись ошибки измерение в EEPROM датчика для последующей
 * компенсации его показаний.
 *
 * Оновременно можно подключить несколько DS18b20, измерения на них
 * запустятся одновременно.
 *
 * Изначально проект строился на основе Bluepill STM32F103C8 (128k flash / 20k RAM),
 * для общения по 1-wire испоьзовался UART3.
 * Позднее было переделано под stm32f103c6 (32K flash / 10k RAM), у которого
 * UART3 отсутствует, и 1-wire реализован программно ("ногодрыг" с задержками).
 *
 * Используется библиотека Tiny USB.
 *
 *
 *                              Процесс компенсации:
 *                      ---------------------------------
 * 0. Стенд подключают к USB порту ПК, запускают терминальную программу (putty например).
 *
 * 1. Сенсор(ы) подключают к шине 1-wire стенда, и выдерживают их какое-то время при 0°C
 *    (вода со льдом). В это время питание на шине 1-wire отсутвует.
 *
 * 2. Выполняют последовательность команд:
 *    s - подача питания и сканирование шины
 *    m - измерение температуры
 *    r - чтение результатов измерения
 *    t - (необязательно) вывод результатов измерения температуры в терминал
 *    c - запись компенсационных данных в EEPROM устройства. При этом в байт 0
 *        (tH) пишется значение младшего байта измеренной температуры, в байт 1
 *        (tL) пишется его значение с побитовой инверсией, для проверки.
 *    o - отключение питания на шине.
 *
 *. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
 *
 *
 *                                                                                                                 +--------------------------+
 *                              Схема подключения DS18b20 к Bluepill                                               |                          |
 *                                                                                                       +---------|  1                       |
 *                                                                                                       |         |          DS18B20         |
 *                                    +----------------------+                                       +-------------|  2                       |
 *                                    |    (PNP transistor)  |                                       |   |         |             #1           |
 *   +3v3 (power)    o----------------|emitter               |                                       |   *---------|  3                       |
 *                           1K       |             collector|------------------+                    |   |         |                          |
 *   PB1 (PWR_EN)    o---[=======]----|base                  |                  |                    |   |         +--------------------------+
 *                                    |                      |                  |
 *                                    +----------------------+                  |                    .   .         . . . . . . . . . . . . . .
 *                                                                              |
 *                                                                              |                    |   |         +--------------------------+
 *   PB11 (RX)       o----------------------------------------------------------*                    |   |         |                          |
 *                                                                              |                    |   *---------|  1                       |
 *                            1K                                                |       1-wire       |   |         |          DS18B20         |
 *   PB10 (TX)       o----[=======]---------------------------------------------*--------------------*-------------|  2                       |
 *                                                                                        GND            |         |             #n           |
 *   GND             o-----------------------------------------------------------------------------------*---------|  3                       |
 *                                                                                                                 |                          |
 *                                                                                                                 +--------------------------+
 *
 *
 *. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
 *
 *
 * про поддельные DS18B20: https://github.com/cpetrich/counterfeit_DS18B20
 *
 *


 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "main.h"
#include "tusb.h"
#include "bsp/board.h"

#include "1wire_com.h"
#include "goods.h"
#include "ring_buff.h"
#include "1wire_family.h"

//------------- prototypes -------------//
static void cdc_task(void);
void board_init(void);
void board_led_write(bool state);
void rx_cmd_buff(char *src, int src_len, int host_idx);
void try_to_send_usb(int itf);
void say_hello(void);

//------------- variables -------------//
bool          g_Usbd_Connected = false;
int           g_DTR;
volatile int  g_Led_Timer = 0;
volatile uint32_t g_Ticks = 0;

struct ds18b20_data g_w1_devices[16];    // адреса найденых устройств на 1-wire
int     g_w1_Cnt = 0;        // кол-во найденых устройств на 1-wire
char    tmps[20];

// USB TX buffer
char               host_txb[2048];
struct s_ring_buff g_HostTx = { .buff = host_txb, .size = sizeof(host_txb) };

//-----------------------------------------------------------------------------------------------------------------+
//                                           GPIO definition
//-----------------------------------------------------------------------------------------------------------------+

const struct pin_def g_Pins_Cmmn[] =   // все пины, кроме 1-wire
{
 	{USB_DP_PULLUP_PORT, USB_DP_PULLUP_PIN,  GPIO_Speed_2MHz | GPIO_Mode_OUT_PP,                  0},    //  0 - USB D+ pullup
 	{GPIOA,                             11,  GPIO_Speed_input | GPIO_Mode_IN_FLOAT,               0},    //  1 - USB D-
 	{GPIOA,                             12,  GPIO_Speed_input | GPIO_Mode_IN_FLOAT,               0},    //  2 - USB D+
 	{LED_PORT,                     LED_PIN,  GPIO_Speed_2MHz | GPIO_Mode_OUT_PP,        !LED_ACTIVE},    //  3 - LED
 //	{GPIO,  , GPIO_Speed_2MHz | ,    1},    //
	{NULL}
};


const struct pin_def g_Pins_1w[] =   // пины 1-wire, неактивное состояние
{
 	{GPIOB,              11,                 GPIO_Speed_input | GPIO_Mode_IN_FLOAT,                 0},    //  0 - UART3_RX
 	{GPIOB,              10,                 GPIO_Speed_50MHz | GPIO_Mode_OUT_PP,                   0},    //  1 - UART3_TX
 	{GPIOB,               1,                  GPIO_Speed_2MHz | GPIO_Mode_OUT_PP,  !PIN_1W_PWR_ACTIVE},    //  2 - PWR_EN
	{NULL}
};

//-----------------------------------------------------------------------------------------------------------------+
//                                               SysTick_Handler
//-----------------------------------------------------------------------------------------------------------------+
void SysTick_Handler (void)
{
	uint32_t tick = g_Ticks;
	g_Ticks = ++tick;

	int led_tmr = g_Led_Timer;
	if(led_tmr)
	{
		led_tmr--;
		g_Led_Timer = led_tmr;
	}

	// светодиод не горит, если не подключены к USB
	// горит в 1/8, если подключены к USB, но порт не открыт
	// в полную - если порт открыт
	// тухнет на 20 мсек после приёма команды
	int led_val = g_DTR || ((tick & 7) == 0);
	led_val &= g_Usbd_Connected;
	led_val &= !led_tmr;

	LED_SET(led_val);
	//board_led_write(g_Usbd_Connected && !led_tmr);
}


//-----------------------------------------------------------------------------------------------------------------+
//                                         Tiny USB  Device callbacks
//-----------------------------------------------------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
	g_Usbd_Connected = true;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
	g_Usbd_Connected = false;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
	(void) remote_wakeup_en;
	g_Usbd_Connected = false;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
	g_Usbd_Connected = true;
}

// Invoked when line state DTR & RTS are changed via SET_CONTROL_LINE_STATE
// Called in main() context: bkg_routines() -> tud_task() -> etc...
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
	if(dtr)
	{
		// открыли порт (подключились терминальной программой)
		say_hello();
	}

	g_DTR = dtr;
}

//-----------------------------------------------------------------------------------------------------------------+
//                                                            USB CDC
//-----------------------------------------------------------------------------------------------------------------+
static void cdc_task(void)
{
	uint8_t itf;

	for (itf = 0; itf < CFG_TUD_CDC; itf++)
	{
		// connected() check for DTR bit
		// Most but not all terminal client set this when making connection
		// if ( tud_cdc_n_connected(itf) )
		{
			if ( tud_cdc_n_available(itf) )
			{
				uint8_t buf[64];

				uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

				g_Led_Timer = 20;

				rx_cmd_buff((char*)buf, count, itf);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------------------+
//                                          ***  some user functions  ***
//-----------------------------------------------------------------------------------------------------------------+
void bkg_routines(void)
{
	//
	tud_task(); // tinyusb device task
	try_to_send_usb(0);
}
//-----------------------------------------------------------------------------------------------------------------+
void delay_ms(uint32_t ms)
{
	uint32_t to_tick = g_Ticks + ms;
	while(g_Ticks < to_tick)
	{
		__WFI();
		bkg_routines();
	}
}
//-----------------------------------------------------------------------------------------------------------------+
void w1_on(void)
{
	// подаём питание на линию 1-wire на 5 мс, что бы DS18 начал работать
	GPIO_OUT_VAL(PIN_1W_TX, 1);
	//PIN_1W_TX_AS_GPIO();
	rb_adds(&g_HostTx, "1-wire power ON\n");

	delay_ms(2);
	g_w1_Cnt = 0;
	memset(&g_w1_devices, 0, sizeof(g_w1_devices));
}
//-----------------------------------------------------------------------------------------------------------------+
void w1_off(void)
{
	// отключаем питание 1-wire
	wire1_deinit();
	rb_adds(&g_HostTx, "1-wire power OFF\n");
}
//-----------------------------------------------------------------------------------------------------------------+
void gpio_init(const struct pin_def *pins)
{

	for(int i = 0;; i++)
	{
		const struct pin_def *p = pins + i;
		if(p->port)
		{
			int bit_pos = p->pin + (p->init_value ? 0 : 16);
			p->port->BSRR = 1 << bit_pos;
			GPIO_MODE(p->port, p->pin, p->mode);
		}
		else break;
	}
}
//-----------------------------------------------------------------------------------------------------------------+
void say_hello(void)
{
	char *hello_str =
		"Hello!\n"
		"This is DS18B20 thermo sensor calibrator.\n"
		"Press '?' for command list,\n"
		"or send 'smrtcmrto' for full process.\n"
		"\n";
	rb_adds(&g_HostTx, hello_str);
}
//-----------------------------------------------------------------------------------------------------------------+
char *itoa10(int v, char *str)
{
	// преобразуем 'v' в строку (10-ное значение)
	// возвращаем указатель на конец строки
	itoa(v, str, 10);
	return str + strlen(str);
}
//-----------------------------------------------------------------------------------------------------------------+
char* fixp_to_str_1(int32_t v, int fix, char *dst)
{
	// преобразуем значение с фиксированной точкой c fix бит после запятой
	// в 10-ю строку с 1 знаком после запятой
	// возвращает указатель на конец строки
	v += 214748365U >> (32 - fix);  // для корректности округления добавляем 0.05 * 2^fix
	char *cp = itoa10(v >> fix, dst);
	*(cp++) = '.';
	int fr = v & ((1 << fix) - 1);
	fr *= 10;
	fr >>= fix;
	return itoa10(fr, cp);
}
//-----------------------------------------------------------------------------------------------------------------+
char* fixp_to_str_1_plus(int32_t v, int fix, char *dst)
{
	if(v > 0) *(dst++) = '+';
	return fixp_to_str_1(v, fix, dst);
}
//-----------------------------------------------------------------------------------------------------------------+

char* byte_2_str(uint8_t v, char* dst)
{
	const uint8_t nibbles[] = "0123456789abcdef";
	dst[0] = nibbles[(v >> 4) & 15];
	dst[1] = nibbles[(v >> 0) & 15];
	dst[2] = 0;
	return dst + 2;
}

//-----------------------------------------------------------------------------------------------------------------+
//void print_char_map(int start, int stop)
//{
//	for(int c = start; c <= stop; c++)
//	{
//		if((c & 15) == 0)
//		{
//			byte_2_str(c, tmps);
//			tmps[2] = ':';
//			rb_add(&g_HostTx, tmps, 3);
//		}
//
//		rb_add(&g_HostTx, &c, 1);
//
//		if((c & 15) == 15)
//		{
//			rb_add(&g_HostTx, "\n", 1);
//		}
//	}
//	rb_add(&g_HostTx, "\n", 1);
//}
//-----------------------------------------------------------------------------------------------------------------+
void ROM_no_2_str(char *no, char *dst)
{
	// номер 1-wire устройства (64 бит) в строку
	// dst должен указывать на буфер размером не менее 17 байт
	for(int i = 0; i < 8; i++)
	{
		dst = byte_2_str(no[i], dst);
	}
}
//-----------------------------------------------------------------------------------------------------------------+
void temp_to_str(uint16_t t)
{
	// преобразуем значение температуры в строку типа "+23.8`C"
	char *cc = fixp_to_str_1_plus(t, 4, tmps);  // строка значения температуры типа "25.8"
	cc[0] = '\"';
	cc[1] = 'C';
	cc[2] = 0;
}
//-----------------------------------------------------------------------------------------------------------------+
void print_1_dev(int idx, int what_print)
{
	if(idx < g_w1_Cnt)
	{
		struct ds18b20_data *devdata = g_w1_devices + idx;

		// индекс устройства
		if(what_print & W1_LIST_IDX)
		{
			itoa(idx + 1, tmps, 10);
			rb_adds(&g_HostTx, tmps);
			rb_add(&g_HostTx, ":", 2);
		}

		// 64-битный адрес устройства
		if(what_print & W1_LIST_ADDR)
		{
			rb_add(&g_HostTx, " ", 1);
			ROM_no_2_str(devdata->addr.c8, tmps);
			rb_add(&g_HostTx, tmps, 16);
		}

		// имя и/или дескриптор устройства (по коду семейства)
		if(what_print & (W1_LIST_NAME | W1_LIST_DESCR))
		{
			const struct wire1_family *f = family_find_by_hex(devdata->addr.ui8[0]);
			if(!f) rb_adds(&g_HostTx, " *unknown family*");
			else
			{
				if(what_print & W1_LIST_NAME)
				{
					rb_add(&g_HostTx, " ", 1);
					rb_adds(&g_HostTx, f->name);
				}
				if(what_print & W1_LIST_DESCR)
				{
					rb_add(&g_HostTx, " (", 2);
					rb_adds(&g_HostTx, f->description);
					rb_add(&g_HostTx, ")", 1);
				}
			}
		}

		// считанная температура без компенсации
		if(what_print & W1_LIST_TEMP)
		{
			rb_adds(&g_HostTx, " readed: ");
			temp_to_str(devdata->pad.temp);
			rb_adds(&g_HostTx, tmps);
		}

		// считанная температура с компенсацией
		if(what_print & W1_LIST_TEMPC)
		{
			rb_adds(&g_HostTx, " compensated: ");
			int8_t ct = devdata->pad.data[2];
			temp_to_str(devdata->pad.temp - ct);
			rb_adds(&g_HostTx, tmps);
			if((devdata->pad.data[2] ^ devdata->pad.data[3]) != 0xff)
			{
				rb_adds(&g_HostTx, " (compens. data invalid)");
			}
		}

		// дамп скречпэда
		if(what_print & W1_LIST_PAD)  // scratchpad content
		{
			tmps[0] = ' ';
			for(int i = 0; i < NUMOFARRAY(devdata->pad.data); i++)
			{
				byte_2_str(devdata->pad.data[i], tmps + 1);
				rb_add(&g_HostTx, tmps, 3);
				tmps[0] = '-';
			}
		}

		// перевод на новую строку
		if(what_print & W1_LIST_LF)
		{
			rb_add(&g_HostTx, "\n", 1);
		}
	}
}
//-----------------------------------------------------------------------------------------------------------------+
void list_1w_devices(int what_print)
{
	// печатаем список найденных 1-wire устройств
	if(!g_w1_Cnt)
	{
		rb_adds(&g_HostTx, "Device list is emty.\nTry press 's' for scan 1-wire.\n");
	}
	else
	{
		if(what_print & W1_LIST_QTT)
		{
			rb_adds(&g_HostTx, "Found ");
			itoa(g_w1_Cnt, tmps, 10);
			rb_adds(&g_HostTx, tmps);
			rb_adds(&g_HostTx, " device(s) on 1-wire\n");
		}

		for(int i = 0; i < g_w1_Cnt; i++)
		{
			print_1_dev(i, what_print);
		}
	}
}
//-----------------------------------------------------------------------------------------------------------------+
void scan_1wire(void)
{
	union64 *src = (union64*)wire1_ROM_NO;
	for(g_w1_Cnt = 0; g_w1_Cnt < NUMOFARRAY(g_w1_devices); g_w1_Cnt++)
	{
		bool search_result = wire1_search(g_w1_Cnt == 0);
		if(!search_result) break;
		g_w1_devices[g_w1_Cnt].addr.i64 = src->i64;
		//memcpy(&g_w1_devices[g_w1_Cnt].addr, wire1_ROM_NO, sizeof(g_w1_devices[0].addr));
		bkg_routines();
	}
}
//-----------------------------------------------------------------------------------------------------------------+
void send_1wire_address(uint8_t *addr_ROM)
{
	wire1_init_buff_cpy(0x55, addr_ROM, 8); // Match Rom [55H] command
	wire1_transact(9*8);                    // send cmd & address
}
//-----------------------------------------------------------------------------------------------------------------+
void ds18b20_read_scratchpad(uint8_t *addr_ROM)
{
	wire1_init();                  // сброс
	send_1wire_address(addr_ROM);  // адрес устройства
	wire1_init_buff_clr(0xBE);     // Read Scratchpad [BEh]
	wire1_transact(80);            // write command & read all data
	//return
}
//-----------------------------------------------------------------------------------------------------------------+
void ds18b20_read_all()
{
	// по очереди читаем scratchpad со всех датчиков
	for(int i = 0; i < g_w1_Cnt; i++)
	{
		struct ds18b20_data *devdata = g_w1_devices + i;
		uint8_t * addr = devdata->addr.ui8;
		if(addr[0] == 0x28)
		{
			ds18b20_read_scratchpad(addr);
			memcpy(devdata->pad.data, g_Wire1_Buff + 1, sizeof(devdata->pad.data));
		}
	}
}
//-----------------------------------------------------------------------------------------------------------------+
void ds18b20_print_temp_all(void)
{
	for(int i = 0; i < g_w1_Cnt; i++)
	{
		struct ds18b20_data *devdata = g_w1_devices + i;
		uint8_t * addr = devdata->addr.ui8;
		if(addr[0] == 0x28)
		{
			print_1_dev(i, W1_LIST_IDX | W1_LIST_TEMP | W1_LIST_TEMPC | W1_LIST_LF);
		}
	}
}

//-----------------------------------------------------------------------------------------------------------------+
void ds18b20_start_temp_measure(void)
{
	// зависаем здесь на 1 сек, пока сенсор измеряет
	if(GPIO_GET_PIN(PIN_1W_RX) == 0) w1_on();
	wire1_init();               // сброс
	wire1_init_buff_clr(0xcc);  // Skip Rom [CCh]
	wire1_transact(8);          // send

	g_Wire1_StrongLast0Pulse = 1;  // "хорошее" питание на линии после передачи команды
	wire1_init_buff_clr(0x44);     // start convert [44]
	wire1_transact(8);             // send & power

	rb_adds(&g_HostTx, "Start temp measure...");
	delay_ms(888);
	GPIO_OUT_VAL    (PIN_1W_PWR, !PIN_1W_PWR_ACTIVE); // снимаем питание

	rb_adds(&g_HostTx, " Complete\nPress 'r' for read measured temp\n");

}
//-----------------------------------------------------------------------------------------------------------------+
void ds18b20_compensate_all()
{
	// Переписываем младший байт температуры в первый байт EEPROM, а
	// его инверсную копию - во второй.
	// В дальнейшем эта величина будет использована для компенсации показаний
	// термодатчика.
	//
	// Подразумевается, что уже выполнены команды:
	// s - шина просканирована и найдены все устройства,
	// m - темперетура оцифрована и
	// r - все данные (в т.ч. оцифрованная температура) считаны из устройства.

	for(int i = 0; i < g_w1_Cnt; i++)
	{
		struct ds18b20_data *devdata = g_w1_devices + i;
		uint8_t * addr = devdata->addr.ui8;
		if(addr[0] == 0x28)
		{
			print_1_dev(i, W1_LIST_IDX);

			if(devdata->pad.data[5] != 0xff && devdata->pad.data[7] != 0x10)  // reserved values
			{
				rb_adds(&g_HostTx, " ERROR: Wrong Scratchpad reserved values. Try press 'r' for read.\n");
				continue;
			}
			else if(devdata->pad.temp == 0x0550)  // default temp after power on
			{
				rb_adds(&g_HostTx, " ERROR: temp = 0x0550 (default). Try press 'm' for start temp measure.\n");
				continue;
			}
			else if(devdata->pad.data[1])
			{
				rb_adds(&g_HostTx, " WARNING: temp value out of range +/-8`C.");
			}

			// переписываем младший байт температуры в Scratchpad
			wire1_init();                                     // reset
			send_1wire_address(addr);                         // device address
			g_Wire1_Buff[0] = 0x4e;                           // Write Scratchpad [4Eh]
			g_Wire1_Buff[1] = devdata->pad.data[0];           // LSB of temp
			g_Wire1_Buff[2] = devdata->pad.data[0] ^ 0xff;    // inversed LSB of temp
			g_Wire1_Buff[3] = devdata->pad.data[4];           // old value (config)
			wire1_transact(32);

			// сохраняем значение Scratchpad в EEPROM
			wire1_init();                                 // reset
			send_1wire_address(addr);                     // device address
			g_Wire1_Buff[0] = 0x48;                       // Copy Scratchpad [48h] command
			g_Wire1_StrongLast0Pulse = 1;                 // need strong parasite power
			wire1_transact(8);                            // send
			delay_ms(50);                                 // delay
			GPIO_OUT_VAL(PIN_1W_PWR, !PIN_1W_PWR_ACTIVE); // strong power off

			rb_adds(&g_HostTx, " OK: LSB of temp stored in EEPROM[0].\n");
		}
	}
}
//-----------------------------------------------------------------------------------------------------------------+
void w1_init_print_state()
{
	wire1_init();

	rb_adds(&g_HostTx, "1-wire init: ");
	if     (g_1W_InitState == Wire1_InitState_Done_OK)  rb_adds(&g_HostTx, "done");
	else if(g_1W_InitState == Wire1_InitState_NoDevice) rb_adds(&g_HostTx, "device(s) not acknowledge");
	else if(g_1W_InitState == Wire1_InitState_ShortGnd) rb_adds(&g_HostTx, "shorted to GND");
	else
	{
		rb_adds(&g_HostTx, "state #");
		itoa(g_1W_InitState, tmps, 10);
		rb_adds(&g_HostTx, tmps);
	}
	rb_add(&g_HostTx, "\n", 1);
}
//-----------------------------------------------------------------------------------------------------------------+
void rx_cmd_buff(char *src, int src_len, int host_idx)
{
	// приняли по UART или USB 1 или несколько байт как часть команды от хоста.
	// каждая комада - один символ
	// начинаем их выполнять
	int  err_code = 0;
	char *err_str = NULL;
	char *help =
	"Supported commands:\n"
	"i - init 1-wire (power on and reset 1-wire) & clear devices list\n"
	"s - scan 1-wire for devices\n"
	"l - list found devices with descriptors\n"
	"m - measure temp\n"
	"r - read DS18B20 scratchpad\n"
	"t - print measured temp\n"
	"c - compensate (write LSB of temp into EEPROM)\n"
	"o - deinit (power off)\n";

	for(int i = 0; i < src_len; i++)
	{
		char ch = src[i];

		if(ch == ' ') continue;  // игнорируем пробелы

		if(ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';  // to lower case

		// echo
		tmps[0] = ch;
		tmps[1] = '\n';
		rb_add(&g_HostTx, tmps, 2);

		// parse command
		// вариант switch-case требует лишних 200 байт flash
//		switch(ch)
//		{
//			case 'i':  // [i]nit - power on & delay 2 ms
//				if(GPIO_GET_PIN(PIN_1W_RX) == 0) w1_on();
//				w1_init_print_state();
//				break;
//
//			case 's':  // [s]can 1-wire bus
//				if(GPIO_GET_PIN(PIN_1W_RX) == 0) w1_on();
//				scan_1wire();
//				list_1w_devices(W1_LIST_QTT | W1_LIST_IDX | W1_LIST_ADDR | W1_LIST_LF);
//				break;
//
//			case 'l':  // [l]ist found devices with descriptors
//				list_1w_devices(W1_LIST_QTT | W1_LIST_IDX | W1_LIST_ADDR | W1_LIST_NAME | W1_LIST_DESCR | W1_LIST_LF);
//				break;
//
//			case 'm':  // [m]easure temp
//				ds18b20_start_temp_measure();
//				break;
//
//			case 'r':  // [r]ead scratchpad
//				ds18b20_read_all();  // забираем всё что можно из всех ds18b20
//				list_1w_devices(W1_LIST_IDX | W1_LIST_ADDR | W1_LIST_PAD | W1_LIST_LF);
//				break;
//
//			case 't':  // [t]temperature print
//
//				ds18b20_print_temp_all();
//				break;
//
//			case 'c':  // [c]ompensate - write compensate data into each device
//
//				//print_char_map(' ', 255);
//				ds18b20_compensate_all();
//				break;
//
//			case 'o':  // [o]ff - power off
//
//				w1_off();
//				break;
//
//			case '?':  // help
//				rb_adds(&g_HostTx, help);
//				break;
//
//			case '\r':
//			case '\n':
//				say_hello();
//				break;
//
////			case :
////				break;
////
//			default:
//				err_code = 1;
//				err_str = "Unknown command";
//		}


		if(ch == 'i')       // [i]nit - power on & delay 2 ms
		{
			if(GPIO_GET_PIN(PIN_1W_RX) == 0) w1_on();
			w1_init_print_state();
		}
		else if(ch == 's')  // [s]can 1-wire bus
		{
			if(GPIO_GET_PIN(PIN_1W_RX) == 0) w1_on();
			scan_1wire();
			list_1w_devices(W1_LIST_QTT | W1_LIST_IDX | W1_LIST_ADDR | W1_LIST_LF);
		}
		else if(ch == 'l')  // [l]ist found devices with descriptors
		{
			list_1w_devices(W1_LIST_QTT | W1_LIST_IDX | W1_LIST_ADDR | W1_LIST_NAME | W1_LIST_DESCR | W1_LIST_LF);
		}
		else if(ch == 'm')  // [m]easure temp
		{
			ds18b20_start_temp_measure();
		}
		else if(ch == 'r')  // [r]ead scratchpad
		{
			ds18b20_read_all();  // забираем всё что можно из всех ds18b20
			list_1w_devices(W1_LIST_IDX | W1_LIST_ADDR | W1_LIST_PAD | W1_LIST_LF);
		}
		else if(ch == 't')  // [t]temperature print
		{
			ds18b20_print_temp_all();
		}
		else if(ch == 'c')  // [c]ompensate - write compensate data into each device
		{
			//print_char_map(' ', 255);
			ds18b20_compensate_all();
		}
		else if(ch == 'o')  // [o]ff - power off
		{
			w1_off();
		}
		else if(ch == '?')
		{
			rb_adds(&g_HostTx, help);
		}
		else if(ch == '\r' || ch == '\n')
		{
			say_hello();
		}
//		else if(ch == '')
//		{
//
//		}
		else  // unknown command
		{
			err_code = 1;
			err_str = "Unknown command";
		}


		// вывод результата
		if(err_code == 0)
		{
			rb_adds(&g_HostTx, "OK\n");
		}
		else if(err_code > 0)
		{
			if(err_str)
			{
				rb_adds(&g_HostTx, err_str);
				rb_add(&g_HostTx, "\n", 1);
			}
			rb_adds(&g_HostTx, "ERROR\n");
		}
	}
}

//-----------------------------------------------------------------------------------------------------------------+
void try_to_send_usb(int itf)
{
	// пробуем отправить ответ в USB с номером интерфейса itf
	struct s_ring_buff * rb = &g_HostTx;

	// длина непрерывного куска текста в нашем буфере,
	// но не больше свободного места в буфере передатчика
	int to_send = min(rb_solid(rb), tud_cdc_n_write_available(itf));

	if(to_send)
	{
		// если нужно отправлять
		void *b = rb_tailp(rb);                              // непрерывный кусок текста
		uint32_t sended = tud_cdc_n_write(itf, b, to_send);  // отправляем, получаем размер отправленного
		rb_skip(rb, sended);                                 // освобождаем наш буфер на размер отправленного текста
		tud_cdc_n_write_flush(itf);                          // сливаем буфер передатчика
	}
}

//-----------------------------------------------------------------------------------------------------------------+
//                                                  MAIN
//-----------------------------------------------------------------------------------------------------------------+

int main(void)
{
 	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN
 	             | RCC_APB2ENR_AFIOEN /*| RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SPI1EN*/  ;

	RCC->AHBENR  |= RCC_AHBENR_DMA1EN;

	board_init();

	AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_1;        // make JTAG pins as GPIO

	gpio_init(g_Pins_Cmmn);
	gpio_init(g_Pins_1w);

	// init device stack on configured roothub port
	tud_init(BOARD_TUD_RHPORT);

	// USB D+ pullup
	GPIO_OUT_VAL(PIN_USB_PU, 1);

	while (1)
	{
		bkg_routines();
		cdc_task();
	}

	return 0;
}
//-----------------------------------------------------------------------------------------------------------------+


