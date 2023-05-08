#ifndef WIRE1_CMMN_H_INCLUDED
#define WIRE1_CMMN_H_INCLUDED

	#include <stdint.h>
	#include <stdbool.h>


	#if defined WIRE1_UART

		#include "1wire_uart.h"

		#define wire1_init     wire1_uart_init
		#define wire1_deinit   wire1_uart_deinit
		#define wire1_transact wire1_uart_transact

//	#elif defined WIRE1_TIM
//
//		#include "1wire_tim.h"
//
//		#define wire1_init     wire1_tim_init
//		#define wire1_deinit   wire1_tim_deinit
//		#define wire1_transact wire1_tim_transact

	#elif defined WIRE1_GPIO

		#include "1wire_gpio.h"

		#define wire1_init     wire1_gpio_init
		#define wire1_deinit   wire1_gpio_deinit
		#define wire1_transact wire1_gpio_transact

	#else

		#error "Error: 1wire interface not defined"

	#endif // WIRE1_xxx


	enum e_Wire1_States
	{
		Wire1State_Off = 0,
		Wire1State_Idle,
		Wire1State_Busy,
		Wire1State_Done,
	};

	enum e_Wire1_InitState
	{
		Wire1_InitState_Idle = 0,   // перед инициализацией и сбросом
		Wire1_InitState_Reset,      // сброс
		Wire1_InitState_Rise,       // ожидание подъёма линии до 1
		Wire1_InitState_Wait0,      // ожидание 0 от устройства
		Wire1_InitState_Wait1,      // ожидание 1 от устройства
		Wire1_InitState_Done_OK,    // устройство обнаружено
		Wire1_InitState_NoDevice,   // устройство не обнаоужено (тайм-аут)
		Wire1_InitState_ShortGnd    // линия закорочена на землю
	};

	enum e_Wire1_Direction
	{
		e_Wire1_Write,
		e_Wire1_Read,
	};


	extern volatile enum e_Wire1_InitState g_1W_InitState;
	extern volatile enum e_Wire1_States    g_W1_State;

//	extern volatile uint32_t g_Wire1_Ready;
	extern uint8_t g_Wire1_Buff[10];
	extern int g_Wire1_StrongLast0Pulse;  // последний импульс будет "0" и формироваться будет не UART-ом, а как 0 на GPIO, а после будет 1 от GPIO
	extern uint8_t wire1_ROM_NO[8];   // здесь находится номер очередного устройства на 1-wire после поиска wire1_search()


	int  wire1_search_all(void);
	void wire1_read_single_address(void);
	bool wire1_search(bool first);
	void wire1_init_buff_clr(uint8_t cmd);
	void wire1_init_buff_cpy(uint8_t cmd, uint8_t *src, int byte_num);


#endif /* WIRE1_CMMN_H_INCLUDED */
