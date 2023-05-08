#ifndef INT_UNION_H_INCLUDED
	#define INT_UNION_H_INCLUDED

	#include <stdint.h>

	typedef union
	{
		int16_t     i16;
		uint16_t   ui16;
		struct
		{
			int8_t   i8l;
			int8_t   i8h;
		};
		struct
		{
			uint8_t ui8l;
			uint8_t ui8h;
		};
		int8_t       i8[2];
		uint8_t     ui8[2];
	}union16;

	typedef union
	{
		int32_t     i32;
		uint32_t   ui32;
		union16     u16[2];
		struct
		{
			int16_t   i16l;
			int16_t   i16h;
		};
		struct
		{
			uint16_t ui16l;
			uint16_t ui16h;
		};
		int16_t     i16[2];
		int8_t       i8[4];
		uint16_t   ui16[2];
		uint8_t     ui8[4];
	}union32;


	typedef union
	{
		int64_t     i64;
		uint64_t   ui64;
		int32_t     i32[2];
		uint32_t   ui32[2];
		union32     u32[2];
		union16     u16[4];
		struct
		{
			int32_t   i32l;
			int32_t   i32h;
		};
		struct
		{
			uint32_t ui32l;
			uint32_t ui32h;
		};
		int16_t     i16[4];
		int8_t       i8[8];
		uint16_t   ui16[4];
		uint8_t     ui8[8];
		char         c8[8];
	}union64;




#endif /* INT_UNION_H_INCLUDED */
