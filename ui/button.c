#include "button.h"
#include "../config.h"
#include <math.h>

// Interpolazione lineare fra due colori (alpha inclusa).
static Color Mix(Color a, Color b, float k) {
    Color r;
    r.r = (unsigned char)(a.r + (b.r - a.r) * k);
    r.g = (unsigned char)(a.g + (b.g - a.g) * k);
    r.b = (unsigned char)(a.b + (b.b - a.b) * k);
    r.a = (unsigned char)(a.a + (b.a - a.a) * k);
    return r;
}

// Clampa un canale di colore a 255 (raylib 5.0 non espone piu' MIN/MAX)
static unsigned char Bump(unsigned char v, int d) {
    int r = (int)v + d;
    if (r > 255) r = 255;
    if (r < 0)   r = 0;
    return (unsigned char)r;
}

void DrawCenteredText(Font font, const char* text, Rectangle box, float size, Color color) {
    if (size <= 0.0f) return;
    Vector2 s = MeasureTextEx(font, text, size, 1.0f);
    // Allineamento ottico: il testo va un filo piu' in alto del centro
    // geometrico, altrimenti sembra affondare nel tasto.
    Vector2 p = {
        box.x + (box.width  - s.x) * 0.5f,
        box.y + (box.height - s.y) * 0.5f - s.y * 0.06f
    };
    DrawTextEx(font, text, p, size, 1.0f, color);
}

float ButtonFontSize(const Button* b) {
    // ~42% del lato minore: le cifre restano ben proporzionate
    // sia a 400x640 sia a una finestra molto piu' grande.
    float side = (b->rect.width < b->rect.height) ? b->rect.width : b->rect.height;
    return side * 0.42f;
}

// Corpo + colore del testo in base al tipo di tasto e allo stato.
static void ButtonColors(const Button* b, const Theme* t, Color* bg, Color* fg) {
    Color base, hover, fgBase;

    switch (b->kind) {
        case BTN_EQ:
            base = t->eqBg;     hover = t->eqBgHover;  fgBase = t->eqText;     break;
        case BTN_CLEAR:
            base = t->dangerBg; hover = t->dangerBgHover; fgBase = t->dangerText; break;
        case BTN_OP:
            base = t->opBg;     hover = t->opBgHover;  fgBase = t->opText;     break;
        case BTN_DIGIT:
        default:
            base = t->keyBg;    hover = t->keyBgHover; fgBase = t->keyText;    break;
    }

    // hover rialza il colore, la pressione lo abbassa: feedback immediato
    float k = b->hovered ? 0.55f : 0.0f;
    if (b->press > 0.0f) k *= (1.0f - b->press);

    *bg = Mix(base, hover, k);
    *fg = fgBase;
}

void DrawCalcButton(Button* b, Font font, const Theme* t) {
    Color bg, fg;
    ButtonColors(b, t, &bg, &fg);

    // --- Ombra: stesse forma, sfalsata in basso, alpha basso ---
    Rectangle sh = b->rect;
    sh.y += b->rect.height * 0.055f;
    sh.height *= 0.985f;
    DrawRectangleRounded(sh, ROUND_BUTTON, SEGM, t->shadow);

    // --- Corpo ---
    DrawRectangleRounded(b->rect, ROUND_BUTTON, SEGM, bg);

    // --- Riflesso: una piccola pillola lucida nella meta' superiore.
    //     Un tempo era un rettangolo arrotondato al 44% dell'altezza, ma
    //     lasciava una cucitura a metà tasto: sembrava un errore.
    //     Cosi' e' una "lente" staccata, che si legge come vetro. ---
    Rectangle hi = b->rect;
    hi.x      += b->rect.width * 0.10f;
    hi.y      += b->rect.height * 0.09f;
    hi.width  -= b->rect.width  * 0.20f;
    hi.height  = b->rect.height * 0.34f;
    Color hiCol = bg;
    hiCol.r = Bump(bg.r, 20);
    hiCol.g = Bump(bg.g, 20);
    hiCol.b = Bump(bg.b, 20);
    hiCol.a = (unsigned char)(b->hovered ? 150 : 95);
    DrawRectangleRounded(hi, 1.0f, 12, hiCol);

    // --- Bordo sottile: definisce il contorno anche senza ombra ---
    Rectangle inner = b->rect;
    inner.x += 1.0f; inner.y += 1.0f;
    inner.width  -= 2.0f; inner.height -= 2.0f;
    Color line = fg;
    line.a = (unsigned char)(b->hovered ? 90 : 40);
    DrawRectangleRoundedLines(inner, ROUND_BUTTON, SEGM, 1.0f, line);

    // --- Etichetta ---
    DrawCenteredText(font, b->text, b->rect, ButtonFontSize(b), fg);
}
