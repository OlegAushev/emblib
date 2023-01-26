#pragma once


#include <emb_c28x/emb_algorithm.h>
#include <emb_c28x/emb_array.h>
#include <emb_c28x/emb_bitset.h>
#include <emb_c28x/emb_circularbuffer.h>
#include <emb_c28x/emb_core.h>
#include <emb_c28x/emb_filter.h>
#include <emb_c28x/emb_math.h>
#include <emb_c28x/emb_queue.h>
#include <emb_c28x/emb_stack.h>
#include <emb_c28x/emb_staticvector.h>
#include <emb_c28x/emb_string.h>
#include <emb_c28x/emb_testrunner/emb_testrunner.h>
#include <algorithm>



class EmbTest
{
public:
	static void CommonTest();
	static void MathTest();
	static void AlgorithmTest();
	static void ArrayTest();
	static void QueueTest();
	static void CircularBufferTest();
	static void FilterTest();
	static void StackTest();
	static void BitsetTest();
	static void StaticVectorTest();
	static void StringTest();
};

