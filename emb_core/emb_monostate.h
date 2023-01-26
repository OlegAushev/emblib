#pragma once


#include <stdint.h>
#include <stddef.h>
#include <assert.h>


namespace emb {

template <class T>
class monostate
{
private:
	static bool _initialized;
protected:
	monostate()
	{
		assert(_initialized);
	}

	~monostate() {}

	static void _set_initialized()
	{
		assert(!_initialized);
		_initialized = true;
	}
public:
	static bool initialized() { return _initialized; }
};

template <class T>
bool monostate<T>::_initialized = false;

} // namespace emb

