#pragma once


#include <c28x_emb/emb_algorithm.h>
#include <c28x_emb/emb_array.h>
#include <c28x_emb/emb_bitset.h>
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

