#ifndef ATTRIBCORE_H
#define ATTRIBCORE_H

namespace Attrib
{
	typedef uint32_t CollectionKey;
	typedef uint32_t ClassKey;
	
	inline void *DefaultDataArea()
	{
		return ((void *(*)())0x6269B0)();
	}
	
	struct StringKey
	{
		uint64_t mHash64;
		uint32_t mHash32;
		const char* mString;
	};
	
	class Collection;
	
	struct RefSpec
	{
		uint32_t mClassKey;
		uint32_t mCollectionKey;
		Collection *mCollectionPtr;
	};
	
	inline void Mix32_1(uint32_t &a, uint32_t &b, uint32_t &c)
	{
		a = c >> 13 ^ (a - b - c);
		b = a << 8 ^ (b - c - a);
		c = b >> 13 ^ (c - a - b);
		a = c >> 12 ^ (a - b - c);
		b = a << 16 ^ (b - c - a);
		c = b >> 5 ^ (c - a - b);
		a = c >> 3 ^ (a - b - c);
		b = a << 10 ^ (b - c - a);
		c = b >> 15 ^ (c - a - b);
	}
	
	inline constexpr uint32_t Mix32_2(uint32_t a, uint32_t b, uint32_t c)
	{
		a = c >> 13 ^ (a - b - c);
		b = a << 8 ^ (b - c - a);
		c = b >> 13 ^ (c - a - b);
		a = c >> 12 ^ (a - b - c);
		b = a << 16 ^ (b - c - a);
		c = b >> 5 ^ (c - a - b);
		a = c >> 3 ^ (a - b - c);
		b = a << 10 ^ (b - c - a);
		return b >> 15 ^ (c - a - b);
	}
	
	extern uint32_t hash32(const char *str, size_t length, uint32_t initval);
	extern uint32_t StringHash32(const char *str, uint32_t bytes);
	extern uint32_t StringHash32(const char* str);
	
	constexpr uint32_t CTStringHash32(const char *str)
	{
		uint32_t a = 0x9E3779B9;
		uint32_t b = 0x9E3779B9;
		uint32_t c = 0xABCDEF00;
		size_t length = std::char_traits<char>::length(str);
		int v1 = 0;
		int v2 = length;
	
		while (v2 >= 12)
		{
			a += ((uint32_t *)str)[(v1 / sizeof(uint32_t)) + 0];
			b += ((uint32_t *)str)[(v1 / sizeof(uint32_t)) + 1];
			c += ((uint32_t *)str)[(v1 / sizeof(uint32_t)) + 2];
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
}

#endif
