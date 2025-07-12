
#include "Attrib.h"

namespace Attrib
{
	uint32_t hash32(const char* str, size_t length, uint32_t initval)
	{
        uint32_t a = 0x9E3779B9;
        uint32_t b = 0x9E3779B9;
        uint32_t c = initval;
        int v1 = 0;
        int v2 = length;

        while (v2 >= 12)
        {
            a += ((uint32_t*)str)[(v1 / sizeof(uint32_t)) + 0];
            b += ((uint32_t*)str)[(v1 / sizeof(uint32_t)) + 1];
            c += ((uint32_t*)str)[(v1 / sizeof(uint32_t)) + 2];
            Mix32_1(a, b, c);
            v1 += 12;
            v2 -= 12;
        }

        c += length;

        switch (v2)
        {
        case 11:
            c += str[10 + v1] << 24;
        case 10:
            c += str[9 + v1] << 16;
        case 9:
            c += str[8 + v1] << 8;
        case 8:
            b += str[7 + v1] << 24;
        case 7:
            b += str[6 + v1] << 16;
        case 6:
            b += str[5 + v1] << 8;
        case 5:
            b += str[4 + v1];
        case 4:
            a += str[3 + v1] << 24;
        case 3:
            a += str[2 + v1] << 16;
        case 2:
            a += str[1 + v1] << 8;
        case 1:
            a += str[v1];
            break;
        default:
            break;
        }

        return Mix32_2(a, b, c);
	}

    uint32_t StringHash32(const char *str)
    {
        if (str && *str != '\0')
            return hash32(str, strlen(str), 0xABCDEF00);
        else
            return 0;
    }

    uint32_t StringHash32(const char *str, uint32_t bytes)
    {
        if (str && *str != '\0')
            return hash32(str, bytes, 0xABCDEF00);
        else
            return 0;
    }
}