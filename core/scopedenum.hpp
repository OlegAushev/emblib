// scoped_enum from boost
// https://github.com/steinwurf/boost/blob/master/boost/core/scoped_enum.hpp
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#define SCOPED_ENUM_UT_DECLARE_BEGIN(EnumType, UnderlyingType)  \
    struct EnumType {                                           \
        EnumType() {}                                           \
        explicit EnumType(UnderlyingType v) : v_(v) {}          \
        UnderlyingType underlying_value() const { return v_; }  \
    private:                                                    \
        UnderlyingType v_;                                      \
        typedef EnumType self_type;                             \
    public:                                                     \
        enum enum_type

#define SCOPED_ENUM_DECLARE_END2() \
    enum_type native_value() const { return enum_type(v_); } \
    friend bool operator ==(self_type lhs, self_type rhs) { return enum_type(lhs.v_)==enum_type(rhs.v_); } \
    friend bool operator ==(self_type lhs, enum_type rhs) { return enum_type(lhs.v_)==rhs; } \
    friend bool operator ==(enum_type lhs, self_type rhs) { return lhs==enum_type(rhs.v_); } \
    friend bool operator !=(self_type lhs, self_type rhs) { return enum_type(lhs.v_)!=enum_type(rhs.v_); } \
    friend bool operator !=(self_type lhs, enum_type rhs) { return enum_type(lhs.v_)!=rhs; } \
    friend bool operator !=(enum_type lhs, self_type rhs) { return lhs!=enum_type(rhs.v_); } \
    friend bool operator <(self_type lhs, self_type rhs) { return enum_type(lhs.v_)<enum_type(rhs.v_); } \
    friend bool operator <(self_type lhs, enum_type rhs) { return enum_type(lhs.v_)<rhs; } \
    friend bool operator <(enum_type lhs, self_type rhs) { return lhs<enum_type(rhs.v_); } \
    friend bool operator <=(self_type lhs, self_type rhs) { return enum_type(lhs.v_)<=enum_type(rhs.v_); } \
    friend bool operator <=(self_type lhs, enum_type rhs) { return enum_type(lhs.v_)<=rhs; } \
    friend bool operator <=(enum_type lhs, self_type rhs) { return lhs<=enum_type(rhs.v_); } \
    friend bool operator >(self_type lhs, self_type rhs) { return enum_type(lhs.v_)>enum_type(rhs.v_); } \
    friend bool operator >(self_type lhs, enum_type rhs) { return enum_type(lhs.v_)>rhs; } \
    friend bool operator >(enum_type lhs, self_type rhs) { return lhs>enum_type(rhs.v_); } \
    friend bool operator >=(self_type lhs, self_type rhs) { return enum_type(lhs.v_)>=enum_type(rhs.v_); } \
    friend bool operator >=(self_type lhs, enum_type rhs) { return enum_type(lhs.v_)>=rhs; } \
    friend bool operator >=(enum_type lhs, self_type rhs) { return lhs>=enum_type(rhs.v_); } \
    };

#define SCOPED_ENUM_DECLARE_END(EnumType)           \
    ;                                               \
    EnumType(enum_type v) : v_(v) {}                \
    SCOPED_ENUM_DECLARE_END2()

#define SCOPED_ENUM_DECLARE_BEGIN(EnumType) \
    SCOPED_ENUM_UT_DECLARE_BEGIN(EnumType, unsigned int)

