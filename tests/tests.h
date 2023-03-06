#pragma once


#include "../algorithm.h"
#include "../array.h"
#include "../bitset.h"
#include "../chrono.h"
#include "../circularbuffer.h"
#include "../core.h"
#include "../filter.h"
#include "../math.h"
#include "../queue.h"
#include "../stack.h"
#include "../staticvector.h"
#include "../string.h"
#include "../testrunner/testrunner.h"
#include "../eeprom/eeprom.h"
#include <algorithm>


namespace emb {

class tests
{
public:
	static void common();
	static void math();
	static void algorithm();
	static void array();
	static void queue();
	static void circular_buffer();
	static void filter();
	static void stack();
	static void bitset();
	static void static_vector();
	static void string();
	static void chrono();
	static void eeprom();
};

} // namespace emb
