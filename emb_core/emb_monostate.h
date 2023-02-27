#pragma once


#include <stdint.h>
#include <stddef.h>
#include <assert.h>


namespace emb {

template <class T>
class Monostate
{
private:
	static bool _initialized;
protected:
	Monostate()
	{
		assert(_initialized);
	}

	~Monostate() {}

	static void _set_initialized()
	{
		assert(!_initialized);
		_initialized = true;
	}
public:
	static bool initialized() { return _initialized; }
};

template <class T>
bool Monostate<T>::_initialized = false;

} // namespace emb

