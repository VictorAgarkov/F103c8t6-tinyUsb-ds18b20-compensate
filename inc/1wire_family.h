#ifndef WIRE1_FAMILY_H_INCLUDED
#define WIRE1_FAMILY_H_INCLUDED

	#include <stdint.h>

	struct wire1_family
	{
		uint8_t hex;
		char *name;
		char *description;
	};

	extern const struct wire1_family g_1wireFamily[];
	const struct wire1_family * family_find_by_hex(uint8_t hex);

#endif /* WIRE1_FAMILY_H_INCLUDED */
