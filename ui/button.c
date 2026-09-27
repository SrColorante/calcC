#include "button.h"
#include "../config.h"
#include <math.h>

void DrawCalcButton(Button* btn, Vector2 mousePos, Font font) {
    bool isDark = (IsSystemThemeDark() == 1);
    
    // Base colors (Black/Dark for dark mode, White/Light for light mode)
    Color bodyColor = isDark ? (Color){ 20, 20, 25, 255 } : (Color){ 230, 230, 230, 255 };
    
    // Custom color for C button (Electric Blue / Crimson Red)
    if (strcmp(btn->text, "C") == 0) {
        bodyColor = isDark ? (Color){ 0, 120, 220, 255 } : (Color){ 200, 20, 50, 255 };
    }
    
    if (btn->is_hovered) {
        if (strcmp(btn->text, "C") == 0) {
            bodyColor = isDark ? (Color){ 0, 150, 255, 255 } : (Color){ 220, 40, 70, 255 };
        } else {
            bodyColor = isDark ? (Color){ 40, 40, 50, 255 } : (Color){ 210, 210, 210, 255 };
        }
    }
    
    if (btn->is_pressed || btn->visual_press_timer > 0.0f) {
        if (strcmp(btn->text, "C") == 0) {
            bodyColor = isDark ? (Color){ 50, 180, 255, 255 } : (Color){ 240, 60, 90, 255 };
        } else {
            bodyColor = isDark ? (Color){ 0, 100, 180, 255 } : (Color){ 180, 20, 40, 255 }; // Accents when pressed!
        }
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
    if (strcmp(btn->text, "C") == 0) textColor = WHITE; // Always white for C to contrast accent
    DrawTextEx(font, btn->text, textPos, FONT_SIZE, 1, textColor);
}
