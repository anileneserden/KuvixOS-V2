#pragma once
#include <stdint.h>

#include <app/app.h> // app_t, app_vtbl_t

#ifndef DESIGNER_MAX_OBJS
#define DESIGNER_MAX_OBJS 64
#endif

typedef enum {
    DESIGN_OBJ_BUTTON = 1,
    DESIGN_OBJ_LABEL  = 2,
} design_obj_type_t;

typedef enum {
    DESIGN_TOOL_NONE   = 0,
    DESIGN_TOOL_BUTTON = 1,
    DESIGN_TOOL_LABEL  = 2,
} design_tool_t;

typedef struct {
    int id;
    design_obj_type_t type;

    int x, y;   // PREVIEW CLIENT içinde
    int w, h;

    uint32_t color;
    char text[32];
} design_obj_t;

typedef struct {
    int count;
    int next_id;

    design_tool_t tool;

    int selected_id;

    int dragging;
    int drag_id;
    int drag_off_x;
    int drag_off_y;

    design_obj_t objs[DESIGNER_MAX_OBJS];
} designer_t;

extern const app_vtbl_t designer_vtbl;