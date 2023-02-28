#pragma once


#include <c28x_emb/emb_algorithm.h>
#include <c28x_emb/emb_array.h>
#include <c28x_emb/emb_bitset.h>
#include <c28x_emb/emb_chrono.h>
#include <c28x_emb/emb_circularbuffer.h>
#include <c28x_emb/emb_core.h>
#include <c28x_emb/emb_filter.h>
#include <c28x_emb/emb_math.h>
#include <c28x_emb/emb_queue.h>
#include <c28x_emb/emb_stack.h>
#include <c28x_emb/emb_staticvector.h>
#include <c28x_emb/emb_string.h>
#include <c28x_emb/emb_testrunner/emb_testrunner.h>
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
};

} // namespace emb
