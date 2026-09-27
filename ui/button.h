#ifndef BUTTON_H
#define BUTTON_H

#include <raylib.h>
#include <stdbool.h>

typedef enum {
    BTN_NORMAL,
    BTN_H_LONG,
    BTN_V_LONG
} ButtonShape;

typedef struct {
    int grid_x;
    int grid_y;
    ButtonShape shape;
    char text[8];
    Rectangle rect;
    bool is_hovered;
    bool is_pressed;
    float visual_press_timer;
} Button;

void DrawCalcButton(Button* btn, Vector2 mousePos, Font font);

#endif
