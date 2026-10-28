file(GLOB_RECURSE sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/emb/*.cpp)

add_library(emblib OBJECT ${sources})

# PUBLIC propagates `#include <emb/...>` to consumers.
target_include_directories(emblib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

# emb_od_description(), which hangs the extraction and the checks of the
# description of an object dictionary on an executable.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/od_description.cmake)
