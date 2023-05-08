#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

	#include "stm32f1xx_hal.h"
	#include "gpio_F1xx.h"
	#include "int_union.h"


	#define USB_DP_PULLUP_PIN  15
	#define USB_DP_PULLUP_PORT GPIOA

	#define LED_PORT           GPIOC
	#define LED_PIN            13
	#define LED_ACTIVE         0

	void gpio_init(const struct pin_def *pins);

	extern const struct pin_def g_Pins_Cmmn[];    // все пины, кроме SWD
	extern const struct pin_def g_Pins_1w[];   // пины 1-wire

	#pragma pack(push, 1)

	typedef union
	{
		uint8_t data[9];
		struct
		{
			uint16_t temp;
			int8_t th, tl;
			uint8_t config, res_5, res_6, res_7, crc;
		};
	}
	ds18b20_scratchpad;
	#pragma pack(pop)


	struct ds18b20_data
	{
		union64 addr;
		ds18b20_scratchpad pad;
	};


	#define PIN_USB_PU         g_Pins_Cmmn[0]
	#define PIN_LED            g_Pins_Cmmn[3]

	#define PIN_1W_RX          g_Pins_1w[0]
	#define PIN_1W_TX          g_Pins_1w[1]
	#define PIN_1W_PWR         g_Pins_1w[2]
	#define PIN_1W_PWR_ACTIVE  0


	#define PIN_1W_TX_AS_GPIO() GPIO_SET_MODE(PIN_1W_TX, GPIO_Speed_50MHz | GPIO_Mode_OUT_PP)
	#define PIN_1W_TX_AS_AF()   GPIO_SET_MODE(PIN_1W_TX, GPIO_Speed_50MHz | GPIO_Mode_AF_PP)


	#define LED_SET(v)    GPIO_OUT_VAL(PIN_LED, (v) ? LED_ACTIVE : !LED_ACTIVE);


	#define W1_LIST_QTT   0x0001   // print number of scanned devices
	#define W1_LIST_IDX   0x0002   // device index in list (start with 1)
	#define W1_LIST_ADDR  0x0004   // device address (ROM)
	#define W1_LIST_NAME  0x0008   // device name
	#define W1_LIST_DESCR 0x0010   // device description
	#define W1_LIST_TEMP  0x0020   // readed temp
	#define W1_LIST_PAD   0x0040   // dump of scratchpad
	#define W1_LIST_TEMPC 0x0080   // compensated temp
	#define W1_LIST_LF    0x8000   // line feed at end
	#define W1_LIST_ALL   0xffff



#ifdef __cplusplus
}
#endif

#endif /* MAIN_H_INCLUDED */
