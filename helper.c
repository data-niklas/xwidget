#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "helper.h"
#include "xwidget.h"
#include <time.h>
#include <errno.h> 
#include <cairo/cairo.h>
#include <xcb/xcb.h>
#include<fcntl.h>

int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

char *lgraph(char *s){
    size_t len = strlen(s);
    unsigned int i = 0;
    while (i < len && !isgraph(s[i]))i++;
    if (i < len)return (s + i);
    return NULL;
}

char *rgraph(char *s){
    size_t len = strlen(s);
    unsigned int i = len - 1;
    while (i >= 0 && !isgraph(s[i]))i--;
    if (i >= 0)return (s + i);
    return NULL;
}

char *lngraph(char *s){
    size_t len = strlen(s);
    unsigned int i = 0;
    while (i < len && isgraph(s[i]))i++;
    if (i < len)return (s + i);
    return NULL;
}

char *rngraph(char *s){
    size_t len = strlen(s);
    unsigned int i = len - 1;
    while (i >= 0 && isgraph(s[i]))i--;
    if (i >= 0)return (s + i);
    return NULL;
}


int colortoargb(color_t color){
    return (color.a << 24) | ((color.r << 16) * color.a / 255) | ((color.g << 8) * color.a / 255) | (color.b * color.a / 255);
}

/*

    Turns a hex rgb or rgba color code into a color struct

*/
color_t hextocolor(char *hexcode){
    //First should be a "#"
    int len = strlen(hexcode);
    color_t res;
    res.a = 255;
    if (len != 7 && len != 9)return empty_color;
    long num = strtol(hexcode + 1, NULL, 16);
    if (len == 9){
        res.a = num & 0xFF;
        num = num >> 8;
    }
    res.r = (num >> 16) & 0xFF;
    res.g = (num >> 8) & 0xFF;
    res.b = num & 0xFF;
    return res;

}

color_t create_color(unsigned int r, unsigned int g, unsigned int b, unsigned int a){
    color_t res;
    res.r = r;
    res.g = g;
    res.b = b;
    res.a = a;
    return res;
}

widget_t *create_widget(){
    widget_t *res = malloc(sizeof(widget_t));
    res->bg = black_color;
    res->fg = white_color;
    strcpy(res->command, "echo \"This is a test\"");
    strcpy(res->font, "serif");
    strcpy(res->title, "xwidget");
    res->fontsize = 22;
    res->x = 0;
    res->y = 0;
    res->w = 1;
    res->h = 1;
    res->line_height = 1.25;
    res->padding = 0;
    res->refresh_rate = 1000;
    res->next = NULL;
    res->window = 0;
    res->areas = NULL;
    res->cr = NULL;
    res->surface = NULL;
    res->tsurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,0,0);
    res->tcr = cairo_create(res->tsurface);
    return res;
}

area_t *create_area(){
    area_t *res = malloc(sizeof(area_t));
    config_t c;
    c.bg = black_color;
    c.fg = white_color;
    strcpy(c.action_l, "");
    strcpy(c.action_r, "");
    strcpy(c.action_m, "");
    strcpy(c.action_d, "");
    strcpy(c.action_u, "");
    strcpy(c.font, "Arial");
    c.padding = 0;
    c.fontsize = 24;
    c.x = 0;
    c.y = 0;
    c.w = 0;
    c.h = 0;
    c.type = 0;
    res->c = c;
    res->next = NULL;
    return res;
}

void free_area(area_t *area){
    if (area->next != NULL)free_area(area->next);
    free(area);
}

void free_widget(widget_t *widget){
    if (widget->next != NULL)free_widget(widget->next);
    pthread_cancel(widget->thread);
    pthread_join(widget->thread, NULL);
    xcb_destroy_window(conn, widget->window);
    if (widget->areas != NULL)free_area(widget->areas);
    if (widget->surface != NULL)cairo_surface_destroy(widget->surface);
    if (widget->cr != NULL)cairo_destroy(widget->cr);
    cairo_surface_destroy(widget->tsurface);
    cairo_destroy(widget->tcr);
    free(widget);
}

//djb2 algorithm http://www.cse.yorku.ca/~oz/hash.html
unsigned int
hash(char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) != 0)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash % 2147483647;
}