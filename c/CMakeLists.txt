#  c++ pre-processor and compilation flags
add_definitions(-posix -std=c99 -m64 -Wall -pedantic -Werror -Wno-deprecated -g -D_XOPEN_SOURCE=700)

# setup include path to lib directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs ${CMAKE_CURRENT_BINARY_DIR}/libs )

add_subdirectory (apps)
