.POSIX:
.SUFFIXES:
CC     = gcc
CFLAGS = -Wall
DEBUGFLAGS = -O0 -g -DDEBUG
LDFLAGS = 
LDLIBS = -lxcb -lcairo -lpthread
OFILES = gesture.o xwidget.o helper.o

all: xwidget

xwidget: $(OFILES) ; $(CC) $(LDFLAGS) -o xwidget $(OFILES) $(LDLIBS)
xwidget.o: xwidget.c xwidget.h config.h ; $(CC) -c $(CFLAGS) xwidget.c
gesture.o: gesture.c gesture.h ; $(CC) -c $(CFLAGS) gesture.c
helper.o: helper.c helper.h ; $(CC) -c $(CFLAGS) helper.c

gesturedebug.o: gesture.c gesture.h ; $(CC) $(DEBUGFLAGS) -c $(CFLAGS) gesture.c
xwidgetdebug.o: xwidget.c xwidget.h xwidget.h ; $(CC) $(DEBUGFLAGS) -c $(CFLAGS) xwidget.c
helperdebug.o: helper.c helper.h ; $(CC) $(DEBUGFLAGS) -c $(CFLAGS) helper.c
debug: gesturedebug.o xwidgetdebug.o helperdebug.o; $(CC) $(LDFLAGS) $(DEBUGFLAGS) -o xwidget $(OFILES) $(LDLIBS)

clean: ; rm -f xwidget xwidget.o gesture.o helper.o
run: all; ./xwidget