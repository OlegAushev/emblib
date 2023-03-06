#pragma once


#include <c28x_emblib/core.h>


namespace emb {

template <typename T, size_t Capacity>
class CircularBuffer
{
private:
	T _data[Capacity];
	size_t _front;
	size_t _back;
	bool _full;
public:
	CircularBuffer()
		: _front(0)
		, _back(0)
		, _full(false)
	{}

	void clear()
	{
		_front = 0;
		_back = 0;
		_full = false;
	}

	bool empty() const { return (!_full && (_front == _back)); }
	bool full() const { return _full; }
	size_t capacity() const { return Capacity; }

	size_t size() const
	{
		size_t size = Capacity;
		if (!_full)
		{
			if (_back >= _front)
			{
				size = _back - _front;
			}
			else
			{
				size = Capacity + _back - _front;
			}
		}

		return size;
	}

	void push(const T& value)
	{
		_data[_back] = value;
		if (_full)
		{
			_front = (_front + 1) % Capacity;
		}
		_back = (_back + 1) % Capacity;
		_full = (_front == _back);
	}

	const T& front() const
	{
		assert(!empty());
		return _data[_front];
	}

	const T& back() const
	{
		assert(!empty());
		return _data[(_back + Capacity - 1) % Capacity];
	}

	void pop()
	{
		assert(!empty());
		_full = false;
		_front = (_front + 1) % Capacity;
	}

	const T* data() const { return _data; }
	const T* begin() const { return _data; }
	const T* end() const { return _data + Capacity; }

	void fill(const T& value)
	{
		for (size_t i = 0; i < Capacity; ++i)
		{
			_data[i] = value;
		}
	}
};

} // namespace emb

