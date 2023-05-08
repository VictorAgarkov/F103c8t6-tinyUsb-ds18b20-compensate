#ifndef WIRE1_UART_H_INCLUDED
#define WIRE1_UART_H_INCLUDED

	#include "1wire_com.h"

	enum e_Wire1_InitState wire1_uart_init(void);
	void wire1_uart_deinit(void);

	void wire1_uart_transact(int bit_num);


#endif /* WIRE1_UART_H_INCLUDED */
