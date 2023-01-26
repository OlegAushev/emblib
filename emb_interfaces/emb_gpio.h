#pragma once


#include "../emb_core.h"


namespace emb {

namespace gpio {

SCOPED_ENUM_DECLARE_BEGIN(ActiveState)
{
	low = 0,
	high = 1
}
SCOPED_ENUM_DECLARE_END(ActiveState)


SCOPED_ENUM_DECLARE_BEGIN(State)
{
	inactive = 0,
	active = 1
}
SCOPED_ENUM_DECLARE_END(State)


class IInput
{
public:
	IInput() {}
	virtual ~IInput() {}

	virtual State read() const = 0;
};


class IOutput
{
public:
	IOutput() {}
	virtual ~IOutput() {}

	virtual State read() const = 0;
	virtual void set(State state = State::active) = 0;
	virtual void reset() = 0;
	virtual void toggle() = 0;
};

} // namespace gpio

} // namespace emb

