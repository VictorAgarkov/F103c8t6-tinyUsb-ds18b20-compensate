

#ifdef WIRE1_GPIO

	#include "1wire_gpio.h"
	#include "goods.h"
	#include "main.h"
	#include "gpio_F1xx.h"

	//--------------------------------------------------------------------------------------------


	#pragma GCC push_options


		#if defined DEBUG_TARGET
			#pragma GCC optimize ("O3")
			#define delay_us(us) w1_delay(((us) - 0.77 + 0.5/7.2) * 7.2)
			#define WAIT_RX_CNT(us) (((us) - 0.888) * 4.24)
		#elif defined RELEASE_TARGET
			#define delay_us(us) w1_delay(((us) + 0.079 + 0.5/12.27) * 12.27)
			#define WAIT_RX_CNT(us) (((us) - 0.1) * 5.55555)
		#endif

		//--------------------------------------------------------------------------------------------
		// debug target with local O3 optimization, benchmark:
		// cnt = 1,  t ~ 0.77 us
		// cnt = 2,  t ~ 0.9 us
		// cnt = 10, t ~ 2.02 us
		// cnt = (us - 0.77) * 7.2

		// release target with no local optimization, benchmark:
		// cnt = 1,  t ~ 0.043 us
		// cnt = 2,  t ~ 0.084 us (+41 - ???)
		// cnt = 3,  t ~ 0.154 us (+70)
		// cnt = 4,  t ~ 0.236 us (+82)
		// cnt = 5,  t ~ 0.320 us (+84)
		// cnt = 10, t ~ 0.736 us
		// cnt = (us + 0.079) * 12.27

		void w1_delay(volatile int cnt)
		{
			if(cnt > 0)	while(--cnt);
		}

		//--------------------------------------------------------------------------------------------

		// debug target with local O3 optimization, benchmark:
		// cnt = 1,  t ~ 0.888us
		// cnt = 2,  t ~ 1.124us
		// cnt = 10, t ~ 3.01us
		// cnt = (us - 0.888) * 4.24

		// release target with no local optimization, benchmark:
		// cnt = 1,  t ~ 0.084 us (must be 0.100 us)
		// cnt = 2,  t ~ 0.280 us
		// cnt = 3,  t ~ 0.460 us
		// cnt = 4,  t ~ 0.640 us
		// cnt = 10, t ~ 1.720 us
		// cnt = (us - 0.1) * 5.55555

		int wait_rx_0(volatile int cnt)
		{
			while(--cnt)
			{
				if(GPIO_GET_PIN(PIN_1W_RX) == 0) break;
			}
			return cnt;
		}
		//--------------------------------------------------------------------------------------------
		int wait_rx_1(volatile int cnt)
		{
			while(--cnt)
			{
				if(GPIO_GET_PIN(PIN_1W_RX) != 0) break;
			}
			return cnt;
		}

		//--------------------------------------------------------------------------------------------

		enum e_Wire1_InitState wire1_gpio_init(void)
		{
			// ѕодаЄм питание на 1-wire и пытамс¤ получить presence pulse
			// ѕодразумеваетс¤, что на 1wire уже подано питание и устройство готово к работе

			int r;
			enum e_Wire1_InitState ret = Wire1_InitState_NoDevice;

			disable_irq;

			// make host reset pulse
			GPIO_OUT_VAL(PIN_1W_TX, 0);
			delay_us(600);
			GPIO_OUT_VAL(PIN_1W_TX, 1);

			r = wait_rx_1(WAIT_RX_CNT(100));  // wait 100 us for HIGH on 1-wire
			if(!r)
			{
				ret = Wire1_InitState_ShortGnd;
				goto init_end;
			}

			// line UP - wait for target response
			r = wait_rx_0(WAIT_RX_CNT(100));  // wait 100 us for target response (LOW) on 1-wire
			if(!r)
			{
				ret = Wire1_InitState_NoDevice;
				goto init_end;
			}

			// target answer - wait for line release
			r = wait_rx_1(WAIT_RX_CNT(400));  // wait 100 us for HIGH on 1-wire
			if(!r)
			{
				ret = Wire1_InitState_ShortGnd;
				goto init_end;
			}

			ret = Wire1_InitState_Done_OK;

			init_end:
			enable_irq;


			g_W1_State = Wire1State_Idle;
			g_1W_InitState = ret;

			return  ret;

		}


		//--------------------------------------------------------------------------------------------
		void wire1_gpio_transact(int bit_num)
		{
			// запускаем обмен по 1-wire
			// буфер g_Wire1_Buff должен быть проинициализирован:
			//  - в [0] должна лежать команда
			//  - в [1..8] - данные дл¤ передачи или 0xFF дл¤ приЄма
			// если на шине всЄ в пор¤дке, то в [0] снова окажетс¤ команда,
			// а в [1..8] - прин¤тые (или переданные, если нет устройства) данные
			// «авершение обмена закончитс¤ установкой
			// глоб. переменной g_W1_State в Wire1State_Done.


			int BitToRcv = min(bit_num, NUMOFARRAY(g_Wire1_Buff) * 8);

			g_W1_State = Wire1State_Busy;

			for(int curr_bit = 0; curr_bit < BitToRcv; curr_bit++)
			{
				delay_us(3);  // tREC
				int byte_idx = curr_bit >> 3;

				int bit_tx = g_Wire1_Buff[byte_idx] & 1;
				int bit_rx = 0;

				disable_irq;

				if(bit_tx) // '1' - short pulse and receive bit
				{
					GPIO_OUT_VAL(PIN_1W_TX, 0);
					delay_us(5);                   // tLOW1
					GPIO_OUT_VAL(PIN_1W_TX, 1);
					delay_us(5);
					bit_rx = GPIO_GET_PIN(PIN_1W_RX);
					delay_us(80);                  // tSLOT

				}
				else  // '0' - long pulse
				{
					GPIO_OUT_VAL(PIN_1W_TX, 0);
					delay_us(90);                  // tLOW0
					GPIO_OUT_VAL(PIN_1W_TX, 1);
				}

				if(g_Wire1_StrongLast0Pulse && BitToRcv - curr_bit == 1)
				{
					// если нужно "сильное" питани¤ по сигнальной линии - включаем питание
					// после передачи команды 0x44 примерно через 15..20us DS18b20 начнЄт преобразование.
					GPIO_OUT_VAL    (PIN_1W_PWR, PIN_1W_PWR_ACTIVE);
					g_Wire1_StrongLast0Pulse = 0;
				}

				enable_irq;

				uint8_t tt = g_Wire1_Buff[byte_idx];
				tt >>= 1;
				if(bit_rx) tt |= 0x80;
				g_Wire1_Buff[byte_idx] = tt;

			}

			g_W1_State = Wire1State_Idle;

		}


	#pragma GCC pop_options

	//--------------------------------------------------------------------------------------------
	void wire1_gpio_deinit(void)
	{
		// переводим выводы 1-wire в пассивное состо¤ние дл¤ экономии энергии
		gpio_init(g_Pins_1w);

	}

	//--------------------------------------------------------------------------------------------

#endif // WIRE1_GPIO
