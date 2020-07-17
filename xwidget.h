#ifndef WIDGETH
#define WIDGETH

#include <xcb/xcb.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <pthread.h>

typedef struct color_t{
    unsigned int r;
    unsigned int g;
    unsigned int b;
    unsigned int a;
} color_t;


typedef struct config_t{
    //Type 0: Text, 1: Image
    char type;
    int x;
    int y;
    unsigned int w;
    unsigned int h;
    unsigned int padding;
    color_t bg;
    color_t fg;
    char font[128];
    unsigned int fontsize;
    char action_l[128];
    char action_r[128];
    char action_m[128];
    char action_u[128];
    char action_d[128];
} config_t;


typedef struct area_t{
    config_t c;
    char text[128];
    struct area_t *next;
} area_t;

typedef struct widget_t {
    unsigned int x;
    unsigned int y;
    unsigned int w;
    unsigned int h;
    color_t bg;
    color_t fg;
    unsigned long refresh_rate;
    double line_height;
    unsigned int padding;
    char command[128];
    char font[128];
    unsigned int fontsize;
    xcb_window_t window;
    area_t *areas;
    pthread_t thread;
    cairo_t *cr;
    cairo_surface_t *surface;
    cairo_t *tcr;
    cairo_surface_t *tsurface;
    struct widget_t *next;
} widget_t;

typedef enum startmode_e{
    STARTMODE_DEFAULT,
    STARTMODE_RELOAD,
    STARTMODE_CHECK

} startmode_e;

extern xcb_connection_t *conn;
extern xcb_screen_t *dpy;
extern widget_t *widgets;
extern xcb_visualtype_t *visual;
extern color_t empty_color;
extern color_t black_color;
extern color_t white_color;
extern pthread_t fifo_thread;
extern char configfile[128];
extern int fifo_file;
extern char fifo_path[64];
extern int kill_threads;

xcb_visualtype_t *get_visual ();
void createWindow(widget_t* widget);
void *initWidget(void* widget);
void renderAreas(widget_t* widget);

void parseArea(char type, char* value, config_t *c);
void parseArgs(int argc, char *argv[]);
void parseConfig();
widget_t *findWidget(xcb_window_t window);

void *fifo(void* config);

void reload();
void widget_threads();
void init();
void finish();
void sighandle(int signal);

int main(int argc, char *argv[]);

#endif