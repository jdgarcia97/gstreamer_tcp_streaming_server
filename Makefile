GCC=gcc
LIBS=gstreamer-1.0 gtk+-3.0
PKG_CONFIG=pkg-config
PARAMS=--libs --cflags
BINARY_OUT=weyland_tcp_streamer
SRC=weyland_tcp_streamer.c
FLAGS=-o
PTHREAD=-lpthread
weyland_tcp_streamer: ${SRC} 
	${GCC} ${FLAGS} ${BINARY_OUT} ${SRC} `${PKG_CONFIG} ${PARAMS} ${LIBS}` ${PTHREAD}	

remove:
	rm -f ${BINARY_OUT}
