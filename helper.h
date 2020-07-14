#include "xwidget.h"

int msleep(long millis);
char* lgraph(char *s);
char* rgraph(char *s);
char* lngraph(char *s);
char* rngraph(char *s);

int colortoargb(color_t color);
color_t hextocolor(char *hexcode);
color_t create_color(unsigned int r, unsigned int g, unsigned int b, unsigned int a);
widget_t *create_widget();
area_t *create_area();
void free_area(area_t *area);
void free_widget(widget_t *widget);
unsigned int hash(char *str);