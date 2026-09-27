#include "button.h"
#include "../config.h"
#include <math.h>

void DrawCalcButton(Button* btn, Vector2 mousePos, Font font) {
    // btn->is_hovered is set in main.c, but we can double check here or just use it.
    // The instructions: "bottoni della calcolatrice a cerchio che sembrano delle gocce dacqua sopra un piano"
    bool isDark = (IsSystemThemeDark() == 1);
    
    // Drop body (translucent liquid)
    Color bodyColor = isDark ? (Color){ 40, 45, 55, 180 } : (Color){ 210, 220, 230, 180 };
    if (btn->is_hovered) {
        bodyColor = isDark ? (Color){ 60, 65, 75, 200 } : (Color){ 230, 240, 250, 200 };
    }
    if (btn->is_pressed || btn->visual_press_timer > 0.0f) {
        bodyColor = isDark ? (Color){ 80, 90, 110, 230 } : (Color){ 190, 210, 230, 230 };
    }

    // 1.0f radius creates a perfect circle (if square) or pill (if rectangle)
    DrawRectangleRounded(btn->rect, 1.0f, SEGM, bodyColor);

    // Draw Text
    Vector2 textSize = MeasureTextEx(font, btn->text, FONT_SIZE, 1);
    Vector2 textPos = {
        btn->rect.x + (btn->rect.width - textSize.x) / 2.0f,
        btn->rect.y + (btn->rect.height - textSize.y) / 2.0f
    };
    Color textColor = isDark ? WHITE : BLACK;
    DrawTextEx(font, btn->text, textPos, FONT_SIZE, 1, textColor);
}
