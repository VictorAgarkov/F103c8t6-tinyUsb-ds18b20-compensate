#ifndef WIRE1_GPIO_H_INCLUDED
#define WIRE1_GPIO_H_INCLUDED

	#include "1wire_com.h"

	enum e_Wire1_InitState wire1_gpio_init(void);
	void wire1_gpio_deinit(void);

	void wire1_gpio_transact(int bit_num);



#endif /* WIRE1_GPIO_H_INCLUDED */
