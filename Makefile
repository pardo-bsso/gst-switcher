CC=gcc
SRCS = $(wildcard *.c)
PROGS = $(patsubst %.c,%,$(SRCS))

DOTS = $(wildcard *.dot)
PNGS = $(patsubst %.dot,%.png,$(DOTS))

PKG2 = `pkg-config --cflags gstreamer-1.0 clutter-gst-2.0 glib-2.0 gtk+-2.0`
PKG1 = `pkg-config --cflags gstreamer-0.10 gstreamer-interfaces-0.10 gstreamer-controller-0.10 glib-2.0 gtk+-2.0 clutter-gst-1.0`

# see https://wiki.ubuntu.com/NattyNarwhal/ToolchainTransition
LIBS2 = `pkg-config --libs gstreamer-1.0 clutter-gst-2.0 glib-2.0 gtk+-2.0 `
LIBS1 = `pkg-config --libs gstreamer-0.10 gstreamer-interfaces-0.10 gstreamer-controller-0.10 glib-2.0 gtk+-2.0 clutter-gst-1.0`
CFLAGS2=-ggdb -O0 ${PKG2}
CFLAGS1=-ggdb -O0 ${PKG1}


LIBS=${LIBS1}
CFLAGS=-std=c99 ${CFLAGS1}

all: $(PROGS)

%: %.c
	${CC} ${CFLAGS}  -o $@ $<  -L${LD_LIBRARY_PATH} ${LIBS}

graphs: $(PNGS)

%.png: %.dot
	dot  -Tpng -o $@ $<

.PHONY: all
