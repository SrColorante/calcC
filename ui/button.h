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

    float press;           // 0..1, quanto e' premuto (molla)
    float pressVel;        // velocita' della molla (da' il rimbalzo)
    float hover;           // 0..1, sfumato: sollevamento + luminosita'
    float flash;           // >0 se premuto da tastiera
    float ripple;          // 0..1, avanzamento dell'onda di pressione
    Vector2 rippleAt;      // origine dell'onda, coordinate schermo

    bool  hovered;
} Button;

// Avanza le animazioni di un tasto verso il bersaglio corrente.
// targetPress: 1 se il mouse e' premuto sopra, 0 altrimenti.
void UpdateButtonAnim(Button* b, bool mouseDown, float dt);

// Fa partire l'onda di pressione (usata sia dal mouse sia dalla
// tastiera, cosi' il feedback e' identico nei due casi).
void FlashButtonPress(Button* b, Vector2 at);

// Dimensione del font dell'etichetta, proporzionata al tasto.
float ButtonFontSize(Rectangle rect);

// Disegna il tasto. Con lo shader SDF e' un draw call; senza
// shader ricade su DrawRectangleRounded.
void DrawCalcButton(Button* b, Font font, const Theme* t);

// Etichetta centrata (allineamento ottico, un filo piu' in alto).
void DrawCenteredText(Font font, const char* text, Rectangle box, float size, Color color);

#endif
