#include "button.h"
#include "renderer.h"
#include "../config.h"
#include <math.h>
#include <string.h>

// ============================================================
//  ANIMAZIONI
//
//  Tre segnali distinti, tutti indipendenti dal framerate:
//
//    press  molla smorzata (stiffness 320, damping 26): il tasto
//           rientra e rimbalza. Prima era una discesa lineare che
//           finiva sempre a zero, senza vita.
//    hover  smorzamento esponenziale: solleva il tasto di 1.6 px
//           e lo illumina un po'.
//    ripple onda che parte dal punto premuto e si espande.
// ============================================================

void FlashButtonPress(Button* b, Vector2 at) {
    if (!b) return;
    b->flash   = 0.11f;                 // lampeggio breve da tastiera
    b->ripple  = 0.001f;                // (ri)partenza dell'onda
    b->rippleAt = at;
}

void UpdateButtonAnim(Button* b, bool mouseDown, float dt) {
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.05f) dt = 0.05f;         // un frame lunghissimo non deve far esplodere la molla

    // ----bersaglio della molla ----
    float target = (b->hovered && mouseDown) ? 1.0f : 0.0f;
    if (b->flash > 0.0f) {
        b->flash -= dt;
        if (b->flash < 0.0f) b->flash = 0.0f;
        target = 1.0f;
    }

    // sem implicito di Eulero: stabile anche a 20 fps
    const float k = 320.0f, c = 26.0f;
    b->pressVel += ((target - b->press)*k - b->pressVel*c)*dt;
    b->press    += b->pressVel*dt;
    if (b->press < 0.0f) { b->press = 0.0f; if (b->pressVel < 0.0f) b->pressVel = 0.0f; }
    if (b->press > 1.3f)  { b->press = 1.3f;  if (b->pressVel > 0.0f) b->pressVel = 0.0f; }

    // ---- hover: inseguimento esponenziale, identico a 30 e 144 fps ----
    float ht = b->hovered ? 1.0f : 0.0f;
    b->hover += (ht - b->hover)*(1.0f - expf(-dt*14.0f));

    // ---- onda di pressione ----
    if (b->ripple > 0.0f && b->ripple < 1.0f) b->ripple += dt*2.6f;
}

// ============================================================
//  UTILITA' DI COLORE
// ============================================================
static unsigned char Ch(unsigned char v, float k) {
    float x = (float)v*k;
    if (x > 255.0f) x = 255.0f;
    if (x < 0.0f)   x = 0.0f;
    return (unsigned char)x;
}

// Schiarisce (+k) o scurisce (-k) un colore, lasciando l'alpha.
static Color Shade(Color c, float k) {
    Color r;
    r.r = Ch(c.r, k); r.g = Ch(c.g, k); r.b = Ch(c.b, k);
    r.a = c.a;
    return r;
}

static Color WithAlpha(Color c, float a) {
    c.a = (unsigned char)(a*255.0f + 0.5f);
    return c;
}

static const Style* StyleOf(ButtonKind k, const Theme* t) {
    switch (k) {
        case BTN_EQ:    return &t->eq;
        case BTN_CLEAR: return &t->danger;
        case BTN_OP:    return &t->op;
        case BTN_DIGIT:
        default:        return &t->key;
    }
}

// ============================================================
//  TESTO
// ============================================================
void DrawCenteredText(Font font, const char* text, Rectangle box, float size, Color color) {
    if (size <= 0.0f || !text || !text[0]) return;
    Vector2 s = MeasureTextEx(font, text, size, 1.0f);
    // Allineamento ottico: il testo va un filo piu' in alto del centro
    // geometrico, altrimenti sembra affondare nel tasto.
    Vector2 p = {
        box.x + (box.width  - s.x)*0.5f,
        box.y + (box.height - s.y)*0.5f - s.y*0.06f
    };
    DrawTextEx(font, text, p, size, 1.0f, color);
}

float ButtonFontSize(Rectangle rect) {
    // ~42% del lato minore: le cifre restano ben proporzionate
    // sia a 400x640 sia a una finestra molto piu' grande.
    float side = (rect.width < rect.height) ? rect.width : rect.height;
    return side*0.42f;
}

// ============================================================
//  DISEGNO
// ============================================================
void DrawCalcButton(Button* b, Font font, const Theme* t) {
    const Style* st = StyleOf(b->kind, t);

    float p = b->press;
    if (p < 0.0f) p = 0.0f;

    // --- geometria animata: la pressione affonda il tasto ---
    float scale = 1.0f - 0.055f*p;
    float lift  = b->hover*1.6f;              // sollevamento all'hover

    Rectangle r = b->rect;
    float cx = r.x + r.width*0.5f;
    float cy = r.y + r.height*0.5f;
    r.x = cx - r.width *scale*0.5f;
    r.y = cy - r.height*scale*0.5f - lift;
    r.width  *= scale;
    r.height *= scale;

    float side = (r.width < r.height) ? r.width : r.height;

    // --- colore: hover schiarisce, pressione scurisce ---
    float bright = b->hover*0.12f - p*0.10f;

    Panel pn;
    memset(&pn, 0, sizeof(pn));
    pn.rect   = r;
    pn.radius = side*0.5f;                   // cerchio/pillola perfetti
    pn.top    = Shade(st->top,    1.0f + bright);
    pn.bottom = Shade(st->bottom, 1.0f + bright);

    pn.highlight    = st->highlight*(1.0f - 0.35f*p);
    pn.shadow       = st->shadow*(1.0f - 0.55f*p);
    pn.shadowSpread = (side*0.20f < 11.0f ? side*0.20f : 11.0f);
    pn.shadowColor  = t->displayShadow;

    pn.border  = WithAlpha(st->border, (float)st->border.a/255.0f*(1.0f + b->hover*0.5f));
    pn.borderW = st->borderW;

    if (st->glowStrength > 0.0f) {
        pn.glow       = WithAlpha(st->glowColor, st->glowStrength*(0.75f + 0.45f*p));
        pn.glowRadius = side*0.55f;
    }

    if (b->ripple > 0.0f && b->ripple < 1.0f) {
        float f  = 1.0f - b->ripple;
        pn.rippleA = f*f*0.9f;
        pn.rippleR = b->ripple*side*0.85f;
        pn.rippleX = b->rippleAt.x - r.x;
        pn.rippleY = b->rippleAt.y - r.y;
    }

    DrawPanel(&pn);

    // --- etichetta: si restringe insieme al tasto ---
    Color fg = st->text;
    if (p > 0.0f) fg = Shade(fg, 1.0f - 0.10f*p);
    DrawCenteredText(font, b->text, r, ButtonFontSize(r), fg);
}
