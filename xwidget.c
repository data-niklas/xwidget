#include <xcb/xcb.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_aux.h>

#include <pthread.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "xwidget.h"
#include "helper.h"
#include "config.h"

widget_t *widgets = NULL;
xcb_connection_t *conn;
xcb_screen_t *dpy;
xcb_visualtype_t *visual;
color_t empty_color;
color_t black_color;
color_t white_color;
pthread_t fifo_thread = 0;
int fifo_file;
char fifo_path[64];
char configfile[128];
int kill_threads = 0;



void *initWidget(void* param){
    //Window 
    widget_t *w = (widget_t*)param;
    createWindow(w);
    while(kill_threads != 1){
        if (w->cr == NULL)continue;
        FILE *res = popen(w->command, "r");

        cairo_text_extents_t *ext = malloc(sizeof(cairo_text_extents_t));

        char buffer[1024];
        int start;
        area_t *first = NULL;
        area_t *last = NULL;
        area_t *area = NULL;
        config_t c;
        strcpy(c.font, w->font);
        c.fontsize = w->fontsize;
        c.fg = w->fg;
        c.bg = w->bg;
        strcpy(c.action_l, "");
        strcpy(c.action_r, "");
        c.type = 0;
        c.padding = 0;
        c.x = w->padding;
        c.y = w->padding;

        int width = 0;
        int height = 0;
        int areawidth;
        int areaheight;
        while (fgets(buffer, 1024, res)){
            start = 0;
            int len = strlen(buffer);
            int i = 0;
            while (i < len){
                while (i < len && buffer[i] != '{')i++;

                area = create_area();
                area->c = c;

                int nl = 0;
                if (i == len && buffer[i - 1] == '\n')nl = 1;
                strncpy(area->text, &buffer[start], i - start - nl);
                area->text[i - start - nl] = '\0';

                if (area->c.type == 0){
                    cairo_select_font_face(w->tcr,area->c.font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
                    cairo_set_font_size(w->tcr, c.fontsize);
                    cairo_text_extents(w->tcr, area->text, ext);
                    areawidth = ext->width + 2 * c.padding;
                    areaheight = ext->height + 2 * c.padding;
                }
                else if (area->c.type == 1){
                    cairo_surface_t *image = cairo_image_surface_create_from_png(area->text);
                    areawidth = cairo_image_surface_get_width(image) + 2 * c.padding;
                    areaheight = cairo_image_surface_get_height(image) + 2 * c.padding;
                    cairo_surface_destroy(image);
                }
                if (areawidth > area->c.w)area->c.w = areawidth;
                if (areaheight > area->c.h)area->c.h = areaheight;

                if (area->c.x < 0){
                    switch(area->c.x){
                        case -1:
                            area->c.x = (w->w - area->c.w) / 2;
                        break;
                        case -2:
                            area->c.x = w->w - area->c.w;
                        break;
                    }
                }
                if (area->c.y < 0){
                    switch(area->c.y){
                        case -1:
                            area->c.y = (w->h - area->c.h) / 2;
                        break;
                        case -2:
                            area->c.y = w->h - area->c.h;
                        break;
                    }
                }

                if (area->c.x + area->c.w > width)width = area->c.x + area->c.w;
                if (area->c.y + area->c.h > height)height = area->c.y + area->c.h;
                if (c.x > -1)c.x+=area->c.w;
                if (i == len){
                    if (c.y > -1)c.y+=w->line_height * area->c.h;
                    c.x = w->padding;
                }
    
                start = i;

                area->next = NULL;
                if (last == NULL){
                    first = area;
                }
                else{
                    last->next = area;
                }
                last = area;


                if (buffer[i] == '{'){
                    if (len - i > 2 && buffer[i + 1] == '%'){
                        if (buffer[i + 2] == '%' && buffer[i + 3] == '}'){
                            strcpy(c.font, w->font);
                            c.action_l[0] = '\0';
                            c.action_r[0] = '\0';
                            c.padding = 0;
                            c.type = 0;
                            c.fg = w->fg;
                            c.bg = w->bg;
                            c.w = 0;
                            c.h = 0;
                            if (last != NULL){
                                c.x = last->c.x + last->c.w;
                                c.y = last->c.y;
                            }
                            c.fontsize = w->fontsize;
                            i+=4;
                            start=i;
                        }
                        else{
                            int modifier_start = i + 2;
                            int modifier_end = modifier_start + 2;
                            char value[64];
                            while (modifier_end < len && (buffer[modifier_end] != '%' || buffer[modifier_end + 1] != '}')){
                                if (buffer[modifier_end] == '%'){
                                    strncpy(value, &buffer[modifier_start + 2], modifier_end - modifier_start - 2);
                                    value[modifier_end - modifier_start - 2] = '\0';
                                    parseArea(buffer[modifier_start], value, &c);
                                    modifier_start = modifier_end + 1;
                                    modifier_end+=2;
                                }
                                modifier_end++;
                            }
                            i = modifier_end+2;
                            start = i;
                            strncpy(value, &buffer[modifier_start + 2], modifier_end - modifier_start - 2);
                            value[modifier_end - modifier_start - 2] = '\0';
                            parseArea(buffer[modifier_start], value, &c);
                        }
                    }
                }

            }
        }
        width += w->padding * 2;
        height += w->padding * 2;
        if (width > w->w)w->w = width;
        if (height > w->h)w->h = height;
        pclose(res);
        free(ext);

        const uint32_t dimension[] = {
            w->x,
            w->y,
            w->w,
            w->h
        };

        xcb_configure_window(conn, w->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, dimension);
        cairo_xcb_surface_set_size(w->surface, w->w, w->h);
        xcb_flush(conn);
        if (w->areas != NULL){
            free_area(w->areas);
        }
        w->areas = first;
        renderAreas(w);
        msleep(w->refresh_rate);
    }
    return NULL;
}


//Render
void renderAreas(widget_t* w){
    cairo_text_extents_t *ext = malloc(sizeof(cairo_text_extents_t));

    cairo_set_source_rgba(w->cr, w->bg.r / 255., w->bg.g / 255., w->bg.b / 255., w->bg.a / 255.);
    cairo_paint (w->cr);
    area_t *a = w->areas;
    while(a != NULL){
        cairo_set_source_rgba(w->cr, a->c.bg.r / 255., a->c.bg.g / 255., a->c.bg.b / 255., a->c.bg.a / 255.);
        cairo_rectangle(w->cr, a->c.x, a->c.y, a->c.w, a->c.h);
        cairo_fill(w->cr);
    
        if (a->c.type == 0){
            cairo_set_source_rgba(w->cr, a->c.fg.r / 255, a->c.fg.g / 255, a->c.fg.b / 255, a->c.fg.a / 255);
            cairo_select_font_face(w->cr, a->c.font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(w->cr, a->c.fontsize);
            cairo_text_extents(w->tcr, a->text, ext);

            cairo_move_to(w->cr, a->c.x + (a->c.w - ext->width) / 2, a->c.y + a->c.h - (a->c.h - ext->height) / 2);
            cairo_show_text(w->cr, a->text);
        }
        else if (a->c.type == 1){
            cairo_surface_t *image = cairo_image_surface_create_from_png(a->text);
            int iw = cairo_image_surface_get_width(image);
            int ih = cairo_image_surface_get_height(image);
            int ix = a->c.x + (a->c.w - iw) / 2;
            int iy = a->c.y + (a->c.h - ih) / 2;
                    
            cairo_set_source_surface(w->cr, image, ix, iy);
            cairo_rectangle(w->cr, ix, iy, iw, ih);
            cairo_fill(w->cr);
            cairo_surface_destroy(image);
        }
        a = a->next;
    }
    free(ext);
    cairo_surface_flush(w->surface);
    xcb_flush(conn);
}






//Parse
void parseArea(char type, char* value, config_t *c) {
    switch(type){
        case 'B': c->bg = hextocolor(value);break;
        case 'F': c->fg = hextocolor(value);break;
        case 'L': strcpy(c->action_l, value);break;
        case 'R': strcpy(c->action_r, value);break;
        case 'M': strcpy(c->action_m, value);break;
        case 'U': strcpy(c->action_u, value);break;
        case 'D': strcpy(c->action_d, value);break;
        case 'N': strcpy(c->font, value);break;
        case 'P': c->padding = atoi(value);break;
        case 'S': c->fontsize = atoi(value);break;
        case 'X': c->x = atoi(value);break;
        case 'Y': c->y = atoi(value);break;
        case 'W': c->w = atoi(value);break;
        case 'H': c->h = atoi(value);break;
        case 'I': c->type = 1;break;
    }
}


void parseConfig(){
    FILE *file = fopen(configfile, "r");
    widget_t *current = NULL;
    widget_t *last = NULL;

    char buffer[255];
    char first;
    char *start, *end, *keyend, *value;
    while(fgets(buffer, 255, file)){
        first = buffer[0];
        if (first == '#' || first == ';'){
            continue;
        }
        else{
            start = lgraph(buffer);
            if (start == NULL){
                if (current != NULL){
                    if (last == NULL){
                        last = current;
                        widgets = last;
                    }
                    else{
                        last->next = current;
                        last = current;
                    }
                    current = NULL;
                }
                continue;
            }
            end = rgraph(buffer);
            *(end + 1) = '\0';

            keyend = lngraph(start);
            if (keyend == NULL || end - keyend <= 2 || *(keyend + 1) != '=')continue;
            value = keyend + 3;
            *keyend = '\0';

            if (current == NULL)current = create_widget();

            if (strcmp(start, "x") == 0)current->x = atol(value);
            else if (strcmp(start, "y") == 0)current->y = atol(value);
            else if (strcmp(start, "w") == 0)current->w = atol(value);
            else if (strcmp(start, "h") == 0)current->h = atol(value);
            else if (strcmp(start, "bg") == 0)current->bg = hextocolor(value);
            else if (strcmp(start, "fg") == 0)current->fg = hextocolor(value);
            else if (strcmp(start, "refresh_rate") == 0)current->refresh_rate = strtoul(value,NULL,0);
            else if (strcmp(start, "line_height") == 0)current->line_height = atof(value);
            else if (strcmp(start, "padding") == 0)current->padding = atol(value);
            else if (strcmp(start, "command") == 0)strcpy(current->command, value);
            else if (strcmp(start, "fontsize") == 0)current->fontsize = atoi(value);
            else if (strcmp(start, "font") == 0)strcpy(current->font, value);
            else if (strcmp(start, "title") == 0)strcpy(current->title, value);

        }
    }
    if (current != NULL){
        if (last == NULL){
            widgets = current;
        }
        else{
            last->next = current;
        }
    }

    fclose(file);
}

void parseArgs(int argc, char *argv[]){


    startmode_e startmode = STARTMODE_DEFAULT;

    int ch;
    strcpy(configfile, getenv("HOME"));
    strcat(configfile, "/.config/xwidget");
   
    struct stat st = {0};
    if (stat(configfile, &st) == -1) {
        mkdir(configfile, 0700);
    }   
    strcat(configfile, "/xwidget.conf");
    if (stat(configfile, &st) == -1) {
        FILE  *temp = fopen(configfile, "w");
        fprintf(temp, "# Example config file\ncommand = date | awk '{print $4}'\n");
        fclose(temp);
    }


    static struct option long_options[] =
        {
          {"help",    no_argument,       0, 'h'},
          {"config",  no_argument,       0, 'c'},
          {"version", no_argument,       0, 'v'},
          {"print",   no_argument,       0, 'p'},
          {"reload",  no_argument,       0, 'r'},
          {"fix",  no_argument,       0, 'f'},
          {"timeout",  no_argument,       0, 't'}
        };
    int fix = 0;
    int opt_index = 0;
    int timeout = 0;
    while ((ch = getopt_long(argc, argv, ":hc:vrpft:", long_options, &opt_index)) != -1) {
        switch (ch) {
            case 'h':
                printf ("xwidget version %s\n", "1.1");
                printf ("usage: %s [-h | -v | -c | -p | -r | -f | -t]\n"
                        "\t-h Show this help\n"
                        "\t-f Fix the temporary file and start\n"
                        "\t-v Print the version\n"
                        "\t-p Exit the program successfully or exit with an error, either if an instance of xwidget (with this config) was already started, or does not exist\n"
                        "\t-r Reload the instance with the config\n"
                        "\t-t timeout in seconds\n"
                        "\t-c Set the config file\n", argv[0]);
                exit (EXIT_SUCCESS);
                break;
            case 'v':
                printf ("xwidget version %s\n", "1.1");
                break;
            case 'c':
                strcpy(configfile, optarg);
                break;
            case 'r':
                startmode = STARTMODE_RELOAD;
                break;
            case 'p': //used for some unknown options
                startmode = STARTMODE_CHECK;
                break;
            case 'f': //used for some unknown options
                fix = 1;
                break;
            case 't': //used for some unknown options
                timeout = atoi(optarg);
                break;
        }
    }

    char h[32];
    sprintf(h, "%d", hash(configfile));
    strcpy(fifo_path, "/tmp/xwidget");
    strcat(fifo_path, h);
    if (fix)unlink(fifo_path);
    if (startmode == STARTMODE_DEFAULT){
        if (stat(configfile, &st) == -1) {
            puts("Config file does not exist!");
            exit(EXIT_FAILURE);
        }
        if (access( fifo_path, F_OK ) != -1) {
            fifo_file = open(fifo_path, O_WRONLY);
            write(fifo_file, "f", sizeof("f"));
            close(fifo_file);
            exit(0);
        }
        parseConfig(configfile);
        pthread_create(&fifo_thread, NULL, fifo, NULL);
    }
    else if (startmode == STARTMODE_CHECK){
        if (access( fifo_path, F_OK ) != -1) {
            exit(0);
        }
        else exit(1);
    }
    else if (startmode == STARTMODE_RELOAD){
        if (access( fifo_path, F_OK ) == -1) {
            exit(1);
        }
        fifo_file = open(fifo_path, O_WRONLY);
        write(fifo_file, "r", sizeof("r"));
        close(fifo_file);
        exit(0);
    }

    if (timeout != 0){
        signal(SIGALRM, sighandle);
        alarm(timeout);
    }
}


//Helper method
widget_t *findWidget(xcb_window_t window){
    widget_t *w = widgets;
    while (w != NULL){
        if (w->window == window)return w;
        w = w->next;
    }
    return w;
}

xcb_visualtype_t *get_visual ()
{
    xcb_depth_iterator_t iter;
    iter = xcb_screen_allowed_depths_iterator(dpy);

    while (iter.rem) {
        xcb_visualtype_t *vis = xcb_depth_visuals(iter.data);
        if (iter.data->depth == 32){
            return vis;
        }

        xcb_depth_next(&iter);
    }

    return NULL;
}

void createWindow(widget_t* w){
    xcb_colormap_t colormap = xcb_generate_id(conn);
    xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, colormap, dpy->root, visual->visual_id);

    w->window = xcb_generate_id(conn);
        uint32_t value_mask, value_list[32];
    value_mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
    value_list[0] = colortoargb(w->bg);
    value_list[1] = colortoargb(w->bg);
    value_list[2] = 1;
    value_list[3] =  
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE;
    value_list[4] = colormap;

    xcb_create_window(conn,
        32,
        w->window,
        dpy->root,
        w->x, w->y, 
        w->w, w->h, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        visual->visual_id,
        value_mask, value_list);
 
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;

    cookie = xcb_intern_atom(conn, 0, strlen("_NET_WM_NAME"), "_NET_WM_NAME");
    if ((reply = xcb_intern_atom_reply(conn, cookie, NULL))) {
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w->window, reply->atom, XCB_ATOM_STRING, 8, strlen(w->title), w->title);
        free(reply);
    }
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(w->title), w->title);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(WMCLASS), WMCLASS);

    xcb_map_window (conn, w->window);
    xcb_flush(conn);
}

void *fifo(void* nothing){
    char h[32];
    sprintf(h, "%d", hash(configfile));
    strcpy(fifo_path, "/tmp/xwidget");
    strcat(fifo_path, h);
    mkfifo(fifo_path, 0666);
    char buf[8];
    while(1){
        fifo_file = open(fifo_path, O_RDONLY);
        memset(buf, 0, 8);
        read(fifo_file, buf, 8);
        if (strcmp(buf, "r") == 0){
            reload();
        }
        else if (strcmp(buf, "f") == 0){
            exit(0);
        }
        close(fifo_file);
    }
    
}



//Lifecycle methods
void reload(){
    kill_threads = 1;
    if(widgets!=NULL)free_widget(widgets);
    kill_threads = 0;
    parseConfig();
    widget_threads();
}

void widget_threads(){
    widget_t *widget = widgets;
    while (widget != NULL){
        pthread_create(&(widget->thread), NULL, initWidget, widget);
        widget = widget->next;
    }
    widget = widgets;
}


void init(){
    empty_color = create_color(0,0,0,0);
    black_color = create_color(0,0,0,0xFF);
    white_color = create_color(0xFF,0xFF,0xFF,0xFF);

    atexit(finish);
    signal(SIGINT, sighandle);
    signal(SIGTERM, sighandle);

    conn = xcb_connect(NULL, NULL);
    dpy = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    visual = get_visual();
    if (visual == NULL){
        puts("Your screen does not support ARGB");
        exit(1);
    }
}

void finish(){
    kill_threads = 1;
    if (widgets!=NULL)free_widget(widgets);
    if (fifo_thread != 0){
        close(fifo_file);
        remove(fifo_path);
        pthread_cancel(fifo_thread);

    }
    xcb_disconnect(conn);
}

void sighandle (int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
        exit(EXIT_SUCCESS);
    else if (signal == SIGALRM)exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]){
    init();
    parseArgs(argc, argv);

    widget_threads();

    xcb_generic_event_t *event = NULL;
    while ((event = xcb_wait_for_event(conn))) {
        switch (event->response_type & ~0x80) {
            case XCB_BUTTON_PRESS:
            {
                xcb_button_press_event_t *ep = (xcb_button_press_event_t *)event;
                int x = ep->event_x;
                int y = ep->event_y;
                widget_t* w = findWidget(ep->event);
                area_t *area = w->areas;
                while (area != NULL){
                    if (x >= area->c.x && x <= area->c.x + area->c.w && y >= area->c.y && y <= area->c.y + area->c.h){
                        if (ep->detail == 1 && strlen(area->c.action_l) > 0)system(area->c.action_l);
                        else if (ep->detail == 2 && strlen(area->c.action_m) > 0)system(area->c.action_m);
                        else if (ep->detail == 3 && strlen(area->c.action_r) > 0)system(area->c.action_r);
                        else if (ep->detail == 4 && strlen(area->c.action_u) > 0)system(area->c.action_u);
                        else if (ep->detail == 5 && strlen(area->c.action_d) > 0)system(area->c.action_d);
                    }
                    area = area->next;
                }
                break;
            }
            case XCB_EXPOSE:
            {
                xcb_expose_event_t *ep = (xcb_expose_event_t *)event;
                widget_t *w = findWidget(ep->window);
                if (w != NULL && w->cr == NULL){
                    w->surface = cairo_xcb_surface_create(conn, w->window, visual, w->w, w->h);
                    w->cr = cairo_create(w->surface); 
                    cairo_set_operator (w->cr, CAIRO_OPERATOR_SOURCE);
                }
                break;
            }
            default:
                break;
        }
 
 
        free (event);

    }

}