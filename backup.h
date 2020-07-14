#include <xcb/xcb.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_atom.h>
#include <xcb/composite.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include "gesture.h"

cairo_t *cr;
cairo_surface_t *surface;

typedef struct data_t {
    xcb_connection_t *conn;
    xcb_window_t window;
    xcb_screen_t *screen;
    xcb_visualtype_t *visual;

    unsigned int width;
    unsigned int height;

} data_t;

data_t *data;


void initCairo();
void draw();
void init();
void createWindow();
void finish();
void hello(int id);
xcb_visualid_t get_visual();
int main(int argc, char *argv[]);
