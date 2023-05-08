
/*********************************************

         Работаем с датчиками 1-wire
                через UART3.


*********************************************/


#ifdef WIRE1_UART


	#include <string.h>
	#include <stdbool.h>

	#include "1wire_uart.h"
	#include "main.h"
	#include "goods.h"
	#include "gpio_F1xx.h"

	extern volatile enum e_Wire1_InitState g_1W_InitState;

	volatile int s_CurrBit;
	volatile int s_BitToRcv;
	volatile int s_InitTimer = 0;

	//--------------------------------------------------------------------------------------------
	enum e_Wire1_InitState wire1_uart_init(void)
	{
		// Подаём питание на 1-wire и пытамся получить presence pulse
		// Находимся в этой процедуре не более 800us
		// 1. Переводим линию PIN_1W_TX в режим GPIO (она подключена к 1-wire через резистор), устанавливаем 0
		//    запускаем таймер - все измерения будут проводиться в обработчике прерывания от него
		// 2. через 500us переводим линию PIN_1W_TX в 1
		// 3. в течении 15..60us устройство должно выдать на линию лог. 0 (читаем это на линии PIN_1W_RX)
		// 4. через 60..240us устройство должно отпустить линию (лог. 1)
		// 5. Устройство считается найденым, если оно выполнило действия пп. 3 и 4.
		// 6. Устройство считается ненайденым, если оно не выполнило дейсвтие п.3 в течении 100us
		// 7. После предыдущих действий линия переводится в режим alternative function -> USART3,
		//    и в дальнейшем общение с устройством производится через него

		// запрещаем USART
		RCC->APB1ENR  &= ~RCC_APB1ENR_USART3EN;
		g_Wire1_StrongLast0Pulse = 0;

		// выход TX переводим в режим GPIO
		GPIO_SET_MODE(PIN_1W_TX, GPIO_Speed_2MHz | GPIO_Mode_OUT_PP);

		// инициализируем таймер TIM3 RCC_APB1ENR_TIM3EN
		uint32_t apb1rstr = RCC->APB1RSTR;
		RCC->APB1RSTR  = apb1rstr |  RCC_APB1RSTR_TIM3RST;
		RCC->APB1RSTR  = apb1rstr & ~RCC_APB1RSTR_TIM3RST;


		g_1W_InitState = Wire1_InitState_Idle;
	//
		//NVIC_SetPriority(DMA1_Channel3_IRQn, 2);  - ?
		NVIC_EnableIRQ(TIM3_IRQn);


		TIM3->ARR  = SystemCoreClock / 1000000 * 5 - 1;   // period 5us
		TIM3->DIER = TIM_DIER_UIE;
		// запускаем таймер
		TIM3->CR1  = TIM_CR1_CEN;


		// ожидаем ответ от устройства или тайм-аут
		// в прерываниях от таймера будем опрашивать состояние 1-wire,
		// и примем решение о наличии устройства
		//GPIO_OUT_VAL(PIN_DBG1, 1);
		while(g_1W_InitState < Wire1_InitState_Done_OK)
		{
			//__WFI();
		}
		//GPIO_OUT_VAL(PIN_DBG1, 0);

		// закрываем таймер
		TIM3->CR1 = 0;
		NVIC_DisableIRQ(TIM3_IRQn);




		// настраиваем USART
//		g_Wire1_Ready = 0;
		RCC->APB1RSTR  = apb1rstr |  RCC_APB1RSTR_USART3RST;  // апп. сброс
		RCC->APB1ENR  |= RCC_APB1ENR_USART3EN;                // разреш. тактирование
		RCC->APB1RSTR  = apb1rstr & ~RCC_APB1RSTR_USART3RST;  // снять апп. сброс

		USART3->CR1 = 0;
		// период 1 бита - 8.7 us
		// 8.7 * 7 = 60.9 > 60uS - длительность 0
		// 8.7 * 2 = 17.4 > 15 us - пауза после бита "0"
		USART3->BRR = 8.7 * SystemCoreClock / 1000000;
		USART3->DR; // очистка буфера приёмника

		USART3->CR2 = USART_CR2_STOP_1;   // 2.0 stop bits
		USART3->CR1 = USART_CR1_RXNEIE | USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

		//NVIC_SetPriority(USART3_IRQn, IRQ_PRIORITY_1WIRE_USART);
		NVIC_EnableIRQ(USART3_IRQn);


		g_W1_State = Wire1State_Idle;

		return g_1W_InitState;
	}

	//--------------------------------------------------------------------------------------------
	void wire1_uart_deinit(void)
	{
		RCC->APB1ENR  &= ~RCC_APB1ENR_USART3EN;
		NVIC_DisableIRQ(USART3_IRQn);

		// переводим выводы 1-wire в пассивное состояние для экономии энергии
		gpio_init(g_Pins_1w);

		g_W1_State = Wire1State_Off;
	}
	//--------------------------------------------------------------------------------------------
	void wire1_uart_send_next_bit(void)
	{
		int bit = g_Wire1_Buff[s_CurrBit >> 3] & 1;

	//	if((s_CurrBit + 1) == s_BitToRcv && g_Wire1_StrongLast0Pulse != 0)
	//	{
	//		// при поднятом флаге g_Wire1_StrongLast0Pulse последний бит будет 0, и выдаваться
	//		// он будет не TX_UART, а "0" на GPIO. Это несколько удлинит импульс, что не критично.
	//		// После отрабатывания TX_UART этот GPIO будет переведён в "1", что позволит питать
	//		// датчик температуры по сигнальной линии и без просадок
	//		GPIO_OUT_VAL    (PIN_THERMO_SCL, 0);
	//		GPIO_SET_OTYPE  (PIN_THERMO_SCL, GPIO_Type_PushPull);
	//		GPIO_SET_MODE   (PIN_THERMO_SCL, GPIO_Mode_Out);
	//	}
		USART3->DR = bit ? 0xff : 0x80;
	}

	//--------------------------------------------------------------------------------------------
	void wire1_uart_wait4done(void)
	{
		while(g_W1_State == Wire1State_Busy) __WFI();
	}
	//--------------------------------------------------------------------------------------------

	void wire1_uart_transact(int bit_num)
	{
		// запускаем обмен по 1-wire
		// буфер g_Wire1_Buff должен быть проинициализирован:
		//  - в [0] должна лежать команда
		//  - в [1..8] - данные для передачи или 0xFF для приёма
		// если на шине всё в порядке, то в [0] снова окажется команда,
		// а в [1..8] - принятые (или переданные, если нет устройства) данные
		// Завершение обмена закончится установкой
		// глоб. переменной g_W1_State в Wire1State_Done.

		s_BitToRcv = min(bit_num, NUMOFARRAY(g_Wire1_Buff) * 8);

		s_CurrBit = 0;

		g_W1_State = Wire1State_Busy;

		wire1_uart_send_next_bit();

		wire1_uart_wait4done();

	}


	//--------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------
	void USART3_IRQHandler(void)
	{

		int rdr = USART3->DR;

		uint8_t tt = g_Wire1_Buff[s_CurrBit >> 3];
		tt >>= 1;
		if(rdr & 1) tt |= 0x80;
		g_Wire1_Buff[s_CurrBit >> 3] = tt;

		s_CurrBit++;

		if(s_CurrBit < s_BitToRcv)
		{
			wire1_uart_send_next_bit();
		}
		else
		{
			if(g_Wire1_StrongLast0Pulse)
			{
				// если нужно "сильное" питания по сигнальной линии - включаем питание
				// после передачи команды 0x44 примерно через 15..20us DS18b20 начнёт преобразование.
				GPIO_OUT_VAL    (PIN_1W_PWR, PIN_1W_PWR_ACTIVE);
				g_Wire1_StrongLast0Pulse = 0;
			}
			g_W1_State = Wire1State_Done;
		}

	}
	//--------------------------------------------------------------------------------------------
	void TIM3_IRQHandler(void)
	{
		// здесь делаем сброс на линии 1-wire
		// и анализируем её поведение (есть/нет устройств)

	//	GPIO_OUT_VAL(PIN_DBG2, 1);
		TIM3->SR = 0;  // clear interrupt flags
		s_InitTimer++;

		switch(g_1W_InitState)
		{
			// . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
			case Wire1_InitState_Idle:

					// переводим линию в 0
				GPIO_OUT_VAL(PIN_1W_TX, 0);
				g_1W_InitState = Wire1_InitState_Reset;
				s_InitTimer = 0;
			break;

			// . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
			case Wire1_InitState_Reset:
				if(s_InitTimer >= 500 / 5)
				{
					// переводим линию в 1
					GPIO_OUT_VAL(PIN_1W_TX, 1);

					g_1W_InitState = Wire1_InitState_Rise;
					s_InitTimer = 0;
				}
			break;

			// . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
			case Wire1_InitState_Rise:
				// ждём, когда после перевода линии в 1 она переведётся в 1
				if(GPIO_GET_PIN(PIN_1W_RX) != 0)
				{
					g_1W_InitState = Wire1_InitState_Wait0;
					s_InitTimer = 0;
				}
				else if(s_InitTimer >= 60 / 5)
				{
					// более 60 us - тайм-аут
					g_1W_InitState = Wire1_InitState_ShortGnd;
				}
			break;

			// . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
			case Wire1_InitState_Wait0:
				// ждём начала Presence pulse (устройство ответит, закоротив линию на 0)
				if(GPIO_GET_PIN(PIN_1W_RX) == 0)
				{
					g_1W_InitState = Wire1_InitState_Wait1;
					s_InitTimer = 0;
				}
				else if(s_InitTimer >= 120 / 5)
				{
					// более 80 us - тайм-аут
					g_1W_InitState = Wire1_InitState_NoDevice;
				}
			break;

			// . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
			case Wire1_InitState_Wait1:
				// ждём окончания Presence pulse (устройство отпустит линию (1))
				if(GPIO_GET_PIN(PIN_1W_RX) != 0)
				{
					g_1W_InitState = Wire1_InitState_Done_OK;
				}
				else if(s_InitTimer >= 300 / 5)
				{
					// более 300 us - тайм-аут
					g_1W_InitState = Wire1_InitState_NoDevice;
				}
			break;

			default:
			break;
		}
	//	GPIO_OUT_VAL(PIN_DBG2, 0);

	}
	//--------------------------------------------------------------------------------------------


#endif // WIRE1_UART




