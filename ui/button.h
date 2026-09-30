#ifndef BUTTON_H
#define BUTTON_H

#include <raylib.h>
#include <stdbool.h>
#include "theme.h"

typedef enum {
    BTN_DIGIT,
    BTN_OP,
    BTN_EQ,
    BTN_CLEAR
} ButtonKind;

typedef struct {
    ButtonKind kind;
    const char* text;      // puntatore a stringa statica: niente copia
    Rectangle  rect;
    float      press;      // 0..1, quanto e' premuto (animazione)
    bool       hovered;
} Button;

// Calcola il font size del tasto dalla sua altezza, cosi' il
// testo resta proporzionato a qualunque dimensione di finestra.
float ButtonFontSize(const Button* b);

// Disegna un tasto: ombra, corpo, riflesso ed etichetta.
void DrawCalcButton(Button* b, Font font, const Theme* t);

// Disegna l'etichetta centrata (usata anche dal display).
void DrawCenteredText(Font font, const char* text, Rectangle box, float size, Color color);

#endif
