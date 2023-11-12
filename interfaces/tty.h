#pragma once


#include "../core.h"


namespace emb {


class Tty {
public:
    Tty() EMB_DEFAULT
    virtual ~Tty() EMB_DEFAULT

    virtual int getchar() = 0;
    virtual int putchar(int ch) = 0;
};


} // namespace emb
