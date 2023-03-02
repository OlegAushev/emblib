#pragma once


#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <driverlib.h>


namespace emb {

namespace c28x {

template <class T>
class InterruptInvoker
{
private:
	static T* _instance;
	static bool _initialized;
protected:
	InterruptInvoker(T* self)
	{
		assert(!_initialized);
		_instance = self;
		_initialized = true;
	}

	~InterruptInvoker()
	{
		_initialized = false;
		_instance = static_cast<T*>(NULL);
	}
public:
	static T* instance()
	{
		assert(_initialized);
		return _instance;
	}

	static bool initialized() { return _initialized; }
};

template <class T>
T* InterruptInvoker<T>::_instance = static_cast<T*>(NULL);
template <class T>
bool InterruptInvoker<T>::_initialized = false;


template <class T, size_t Size>
class InterruptInvokerArray
{
private:
	static T* _instance[Size];
	static bool _initialized[Size];
	static bool _constructed;
protected:
	InterruptInvokerArray(T* self, size_t instance_num)
	{
		assert(instance_num < Size);
		assert(!_initialized[instance_num]);
		if (!_constructed)
		{
			for (size_t i = 0; i < Size; ++i)
			{
				_instance[i] = static_cast<T*>(NULL);
				_initialized[i] = false;
			}
			_constructed = true;
		}

		_instance[instance_num] = self;
		_initialized[instance_num] = true;
	}
public:
	static T* instance(size_t instance_num)
	{
		assert(_constructed);
		assert(instance_num < Size);
		assert(_initialized[instance_num]);
		return _instance[instance_num];
	}

	static bool initialized(size_t instance_num)
	{
		assert(instance_num < Size);
		if (!_constructed) return false;
		return _initialized[instance_num];
	}
};

template <class T, size_t Size>
T* InterruptInvokerArray<T, Size>::_instance[Size];
template <class T, size_t Size>
bool InterruptInvokerArray<T, Size>::_initialized[Size];
template <class T, size_t Size>
bool InterruptInvokerArray<T, Size>::_constructed = false;


template <typename T>
void from_bytes(T& dest, const uint8_t* src)
{
	uint16_t c28_bytes[sizeof(T)];
	for (size_t i = 0; i < sizeof(T); ++i)
	{
		c28_bytes[i] = src[2*i] | src[2*i+1] << 8;
	}
	memcpy (&dest, &c28_bytes, sizeof(T));
}


template <typename T>
void to_bytes(uint8_t* dest, const T& src)
{
	uint16_t c28_bytes[sizeof(T)];
	memcpy(&c28_bytes, &src, sizeof(T));
	for (size_t i = 0; i < sizeof(T); ++i)
	{
		dest[2*i] = c28_bytes[i] & 0x00FF;
		dest[2*i+1] = c28_bytes[i] >> 8;
	}
}


template <typename T>
bool is_equal(const T& obj1, const T& obj2)
{
	uint8_t obj1_bytes[sizeof(T)*2];
	uint8_t obj2_bytes[sizeof(T)*2];

	to_bytes<T>(obj1_bytes, obj1);
	to_bytes<T>(obj2_bytes, obj2);

	for(size_t i = 0; i < sizeof(T)*2; ++i)
	{
		if (obj1_bytes[i] != obj2_bytes[i])
		{
			return false;
		}
	}
	return true;
}

} // namespace c28x

} // namespace emb

