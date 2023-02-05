#pragma once


#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <c28x_emb/emb_algorithm.h>
#include <cstring>



namespace emb {

template <size_t Capacity>
class String
{
private:
	static const size_t _data_len = Capacity + 1;
	char _data[_data_len];
	size_t _len;
public:
	String()
	{
		memset(_data, 0, _data_len);
		_len = 0;
	}

	String(const char str[])
	{
		memset(_data, 0, _data_len);
		strncpy(_data, str, Capacity);
		_len = strlen(str);
	}
public:
	size_t capacity() const { return Capacity; }
	size_t lenght() const { return _len; }
	size_t size() const { return _len; }
	bool empty() const { return _len == 0; }
	bool full() const { return _len == Capacity; }

	char& operator[] (size_t pos)
	{
#ifdef NDEBUG
		return _data[pos];
#else
		return at(pos);
#endif
	}

	const char& operator[](size_t pos) const
	{
#ifdef NDEBUG
		return _data[pos];
#else
		return at(pos);
#endif
	}

	char& at(size_t pos)
	{
		assert(pos < _len);
		return _data[pos];
	}

	const char& at(size_t pos) const
	{
		assert(pos < _len);
		return _data[pos];
	}
public:
	char* begin() { return _data; }
	char* end() { return _data + _len; }
	const char* begin() const { return _data; }
	const char* end() const { return _data + _len; }

	char* data() { return _data; }
	const char* data() const { return _data; }

	char& front()
	{
		assert(!empty());
		return _data[0];
	}

	const char& front() const
	{
		assert(!empty());
		return _data[0];
	}

	char& back()
	{
		assert(!empty());
		return _data[_len - 1];
	}

	const char& back() const
	{
		assert(!empty());
		return _data[_len - 1];
	}
public:
	void resize(size_t len)
	{
		assert(len <= Capacity);
		if (len > _len)
		{
			emb::fill(_data + _len, _data + len, 0);
		}
		else
		{
			emb::fill(_data + len, _data + _len, 0);
		}
		_len = len;
	}

	void resize(size_t len, char ch)
	{
		assert(len <= Capacity);
		if (len > _len)
		{
			emb::fill(_data + _len, _data + len, ch);
		}
		else
		{
			emb::fill(_data + len, _data + _len, ch);
		}
		_len = len;
	}

	void clear()
	{
		memset(_data, 0, _data_len);
		_len = 0;
	}
public:
	void push_back(char ch)
	{
		assert(!full());
		_data[_len++] = ch;
	}

	void pop_back()
	{
		assert(!empty());
		_data[--_len] = 0;
	}
public:
	void insert(size_t index, char ch)
	{
		assert(!full());
		assert(index <= lenght());

		if (index == _len)
		{
			push_back(ch);
			return;
		}

		memmove(_data + index + 1 , _data + index, _len - index);
		_data[index] = ch;
		++_len;
	}
};


template <size_t Capacity>
inline bool operator==(const String<Capacity>& lhs, const String<Capacity>& rhs)
{
	return strcmp(lhs.data(), rhs.data()) == 0;
}


template <size_t Capacity>
inline bool operator!=(const String<Capacity>& lhs, const String<Capacity>& rhs)
{
	return !(lhs == rhs);
}

} // namespace emb

