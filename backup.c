#include <xcb/xcb.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_atom.h>
 

#include <pthread.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>

#include "gesture.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "backup.h"

#include "config.h"

/*

    Draw

*/

void initCairo(){
    surface = cairo_xcb_surface_create(data->conn, data->window, data->visual, data->width, data->height);
    cr = cairo_create(surface);
}

void draw(){
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_font_size(cr, 24);
    cairo_move_to(cr, 50, 50);
    cairo_show_text(cr, "Trolled");
    cairo_surface_flush(surface);
    xcb_flush(data->conn);
}


void init(){
    data = malloc(sizeof(data_t));
    data->conn = xcb_connect(NULL, NULL);
    data->screen = xcb_setup_roots_iterator(xcb_get_setup(data->conn)).data;
}

xcb_visualid_t get_visual ()
{
    xcb_depth_iterator_t iter;

    iter = xcb_screen_allowed_depths_iterator(data->screen);

    // Try to find a RGBA visual
    while (iter.rem) {
        xcb_visualtype_t *vis = xcb_depth_visuals(iter.data);
        if (iter.data->depth == 32){
            data->visual = vis;
            return vis->visual_id;
        }

        xcb_depth_next(&iter);
    }

    // Fallback to the default one
    return data->screen->root_visual;
}

void createWindow(){
    
    data->window = xcb_generate_id(data->conn);
    data->width = data->screen->width_in_pixels;
    data->height = data->screen->height_in_pixels;   
    
    xcb_visualid_t visual = get_visual();
    xcb_colormap_t colormap = xcb_generate_id(data->conn);
    xcb_create_colormap(data->conn, XCB_COLORMAP_ALLOC_NONE, colormap, data->screen->root, visual);
    int depth = (visual == data->screen->root_visual) ? XCB_COPY_FROM_PARENT : 32;
    
    uint32_t value_mask, value_list[32];
    value_mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
    value_list[0] = 0x00000000;
    value_list[1] = 0x80808080;
    value_list[2] = 1;
    value_list[3] =  
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_BUTTON_MOTION;
    value_list[4] = colormap;

    xcb_create_window(data->conn,
        depth,
        data->window,
        data->screen->root,
        0, 0, 
        data->width, data->height, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        visual,
        value_mask, value_list);
 
    xcb_map_window (data->conn, data->window);
    xcb_set_input_focus(data->conn, XCB_INPUT_FOCUS_POINTER_ROOT, data->window, XCB_CURRENT_TIME);
    xcb_flush(data->conn);
}

void finish(){
    cairo_surface_destroy(surface);
    cairo_destroy(cr);
    xcb_disconnect(data->conn);
    free(data);
}
Gesture *right;
Gesture *left;
Gesture *up;
Gesture *down;
Gesture *s;

void hello(int id){
    if (id == right->id)printf("--Right\n");
    else if (id == left->id)printf("--Left\n");
    else if (id == up->id)printf("--Up\n");
    else if (id == down->id)printf("--Down\n");
    else if (id == s->id)printf("--S\n");
}

int main(int argc, char *argv[]){
    init();
    createWindow();
    gesture_init();

    GestureVector *vector;

    right = gesture_create();
    right->action = hello;
    vector = gesture_vector_create();
    vector->x = 200;
    vector->y = 0;
    right->id = 0;
    right->vector = vector;

    left = gesture_create();
    left->action = hello;
    vector = gesture_vector_create();
    vector->x = -200;
    vector->y = 0;
    left->id = 1;
    left->vector = vector;

    up = gesture_create();
    up->action = hello;
    vector = gesture_vector_create();
    vector->x = 0;
    vector->y = -200;
    up->id = 2;
    up->vector = vector;

    down = gesture_create();
    down->action = hello;
    vector = gesture_vector_create();
    vector->x = 0;
    vector->y = 200;
    down->id = 3;
    down->vector = vector;

    s = gesture_create();
    s->action = hello;
    s->id = 4;
    int points[] = {-50,0,-50,50,0,50,0,100,-50,100}; 
    vector = gesture_vector_create_from_array(points, 5);
    s->vector = vector;

    gesture_register(right);
    gesture_register(left);
    gesture_register(up);
    gesture_register(down);
    gesture_register(s);

    xcb_generic_event_t *event = NULL;
    while ((event = xcb_wait_for_event (data->conn))) {
        switch (event->response_type & ~0x80) {
            case XCB_BUTTON_PRESS:
            {
                xcb_button_press_event_t *ep = (xcb_button_press_event_t *)event;
                gesture_start(ep->event_x, ep->event_y);
                break;
            }
            case XCB_BUTTON_RELEASE:
                gesture_stop();
                break;
            case XCB_MOTION_NOTIFY:
            {
                xcb_motion_notify_event_t *em = (xcb_motion_notify_event_t *)event;
                gesture_track(em->event_x, em->event_y);
                break;
            }
            case XCB_EXPOSE:
                if (cr == NULL)initCairo();
                draw();
                break;
            default:
                break;
        }
 
 
        free (event);

    }
    finish();
}