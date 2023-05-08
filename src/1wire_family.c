
// 1-wire devices list
// Based on:
// https://owfs.org/index_php_page_family-code-list.html
// https://owfs.sourceforge.net/simple_family.html

#include "1wire_family.h"
#include "goods.h"

//#define W1_FAMILY_SLOW_SEARCH // медленный поиск перебором массива

const struct wire1_family g_1wireFamily[] =
{
	//Family Code hex, Name, Description
	{0x00, "Link locator, Provide location information"},
	{0x01, "DS2401/DS2411/DS1990R/DS2490A", "ID-only"},
	{0x02, "DS1991", "Multikey"},
	{0x04, "DS2404", "EconoRam Time chip"},
	{0x05, "Ds2405", "Switch"},
	{0x06, "DS1993","Memory"},
	{0x08, "DS1992", "Memory"},
	{0x09, "DS2502/DS2703/DS2704", "Memory"},
	{0x0a, "DS1995", "16k memory ibutton"},
	{0x0B, "DS2505", "Memory"},
	{0x0c, "DS1996", "64k memory ibutton"},
	{0x0F, "DS2506", "Memory"},
	{0x10, "DS18S20", "Temperature"},
	{0x12, "DS2406", "Switch"},
	{0x14, "DS2430A", "Memory"},
	{0x16, "DS1954/DS1957", "crypto-ibutton"},
	{0x18, "DS1963S/DS1962", "SHA iButton"},
	{0x1A, "DS1963L", "Monetary iButton"},
	{0x1B, "DS2436", "Battery monitor"},
	{0x1C, "DS28E04-100", "Switch"},
	{0x1D, "DS2423", "Counter"},
	{0x1E, "DS2437", "Battery monitor"},
	{0x1F, "DS2409", "Microhub"},
	{0x20, "DS2450", "Voltage"},
	{0x21, "DS1921", "Thermocron"},
	{0x22, "DS1922", "Temperature"},
	{0x23, "DS2433/DS1973", "Memory"},
	{0x24, "DS2415", "Clock"},
	{0x26, "DS2438", "Battery monitor"},
	{0x27, "DS2417", "Clock + interrupt"},
	{0x28, "DS18B20", "Temperature"},
	{0x29, "DS2408", "Switch"},
	{0x2C, "DS2890", "Variable Resitor"},
	{0x2D, "DS2431/DS1972", "Memory"},
	{0x2E, "DS2770", "Battery"},
	{0x30, "DS2760/DS2761/DS2762", "Battery"},
	{0x31, "DS2720", "Battery ID"},
	{0x32, "DS2780", "Battery"},
	{0x33, "DS1961s/DS2432", "SHA-1 ibutton"},
	{0x34, "DS2703", "SHA-1 Battery"},
	{0x35, "DS2755", "Battery"},
	{0x36, "DS2740", "Coulomb counter"},
	{0x37, "DS1977", "password EEPROM"},
	{0x3A, "DS2413", "Switch"},
	{0x3B, "DS1825/MAX31826","Temperature, Temperature/memory"},
	{0x3D, "DS2781", "Battery"},
	{0x41, "DS1923", "Hygrocron"},
	{0x42, "DS28EA00", "Temperature/IO"},
	{0x43, "DS28EC20", "Memory"},
	{0x44, "DS28E10", "SHA-1 Authenticator"},
	{0x51, "DS2751", "Battery monitor"},
	{0x7E, "EDS00xx", "Envoronmental Monitors"},
	{0x81, "USB id", "ID found in DS2490R and DS2490B USB adapters"},
	{0x82, "DS1425", "Authorization"},
	{0x84, "DS2404S", "Dual port plus time"},
	{0x89, "DS1982U", "Uniqueware"},
	{0x8B, "DS1985U", "Uniqueware"},
	{0x8F, "DS1986U", "Uniqueware"},
	{0xA0, "mRS001", "Rotation Sensor"},
	{0xA1, "mVM001", "Vibratio"},
	{0xA2, "mCM001", "AC Voltage"},
	{0xA6, "mTS017", "IR Temperature"},
	{0xB1, "mTC001", "Thermocouple Converter"},
	{0xB2, "mAM001", "DC Current or Voltage"},
	{0xB3, "mTC002", "Thermocouple Converter"},
	{0xEE, "UVI", "Ultra Violet Index"},
	{0xEF, "Moisture Hub"," Moisture meter, 4 Channel Hub"},
	{0xFC, "BAE0910/BAE0911", "Programmable Miroprocessor"},
	{0xFF, "LCD", "Swart LCD"},
};


//-----------------------------------------------------------------------------------------------------------------+



const struct wire1_family * family_find_by_hex(uint8_t hex)
{

	#ifdef W1_FAMILY_SLOW_SEARCH
		// медленный поиск - перебираем весь массив, пока не найдём
		// экономит немного (64 или 20) байт памяти программ
		for(int i = 0; i < NUMOFARRAY(g_1wireFamily); i++)
		{
			if(g_1wireFamily[i].hex == hex) return g_1wireFamily + i;
		}
	#else  // not W1_FAMILY_SLOW_SEARCH
		// быстрый поиск - ищем методом половинного
		// деления по отсортированному списку,
		// за 8 итераций максимум
		int len = (NUMOFARRAY(g_1wireFamily) > 128) ? 256 : 128;  // до ближайшего 2^n, не меньше размера списка
		int pos = len / 2;

		for(; len; len >>= 1)
		{
			int d = len / 4;  // на сколько будем прыгать от текущей позиции
			if(!d) d = 1;

			if(pos >= NUMOFARRAY(g_1wireFamily)) // за пределами списка
			{
				pos -= d;
				continue;
			}

			const struct wire1_family *f = g_1wireFamily + pos;
			if(f->hex == hex)
			{
				return f;  // нашли
			}
			else
			{
				pos += (f->hex < hex) ? +d : -d; // прыгаем
			}
		}
	#endif // W1_FAMILY_SLOW_SEARCH

	return 0;
}
//-----------------------------------------------------------------------------------------------------------------+



