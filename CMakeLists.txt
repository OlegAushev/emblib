file(GLOB_RECURSE sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/emb/*.cpp)

add_library(emblib OBJECT ${sources})

# PUBLIC propagates `#include <emb/...>` to consumers.
target_include_directories(emblib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})