.POSIX:
.SUFFIXES:
CC     = gcc
LIBS = xcb xcb-shape cairo fontconfig
CFLAGS = -Wall $(shell pkg-config --cflags $(LIBS))
DEBUGFLAGS = -O0 -g -DDEBUG
LDFLAGS =
LDLIBS = $(shell pkg-config --libs $(LIBS))-lpthread
OFILES = xwidget.o helper.o

all: xwidget

xwidget: $(OFILES) ; $(CC) $(LDFLAGS) -o xwidget $(OFILES) $(LDLIBS)
xwidget.o: xwidget.c xwidget.h config.h ; $(CC) -c $(CFLAGS) xwidget.c
helper.o: helper.c helper.h ; $(CC) -c $(CFLAGS) helper.c

xwidgetdebug.o: xwidget.c xwidget.h xwidget.h ; $(CC) $(DEBUGFLAGS) -c $(CFLAGS) xwidget.c
helperdebug.o: helper.c helper.h ; $(CC) $(DEBUGFLAGS) -c $(CFLAGS) helper.c
debug: xwidgetdebug.o helperdebug.o; $(CC) $(LDFLAGS) $(DEBUGFLAGS) -o xwidget $(OFILES) $(LDLIBS)

clean: ; rm -f xwidget xwidget.o helper.o
run: all; ./xwidget