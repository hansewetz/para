add_executable (para para.c paraloop.c priq.c error.c sys.c tmo.c buf.c combuf.c outq.c const.h util.c inq.c)
install(TARGETS para DESTINATION bin)
