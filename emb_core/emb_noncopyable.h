#pragma once


#include <stdint.h>
#include <stddef.h>
#include <assert.h>


namespace emb {

class noncopyable
{
protected:
	noncopyable() {}
	~noncopyable() {}
private:
	noncopyable(const noncopyable&);
	const noncopyable& operator=(const noncopyable&);
};

} // namespace emb

