#pragma once


#include <stdint.h>
#include <stddef.h>
#include <assert.h>


namespace emb {

class NonCopyable
{
protected:
	NonCopyable() {}
	~NonCopyable() {}
private:
	NonCopyable(const NonCopyable&);
	const NonCopyable& operator=(const NonCopyable&);
};

} // namespace emb

