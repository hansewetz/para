include_directories(${CMAKE_CURRENT_BINARY_DIR})

# create version.h file
configure_file(
  "version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/version.h"
  )
add_executable (para para.c paraloop.c priq.c error.c sys.c tmo.c buf.c combuf.c outq.c const.h util.c inq.c)
install(TARGETS para DESTINATION bin)
