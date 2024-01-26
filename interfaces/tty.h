#pragma once


#include "../core.h"


namespace emb {


class tty {
public:
    tty() EMB_DEFAULT
    virtual ~tty() EMB_DEFAULT

    virtual int getchar() = 0;
    virtual int putchar(int ch) = 0;
};


} // namespace emb
