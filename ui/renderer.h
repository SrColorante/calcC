#ifndef RENDERER_H
#define RENDERER_H

#include "raylib.h"
#include <stdbool.h>

// ============================================================
//  RENDERER SDF (signed distance field)
//
//  Tutte le forme - fondo, display, tasti - passano da uno shader
//  che calcola la distanza con segno dal bordo del rettangolo.
//  Ne segue che, in un solo draw call per forma, si ottengono:
//
//    - bordi morbiori (anti-aliasing analitico, non MSAA)
//    - gradiente verticale reale
//    - ombra esterna morbida
//    - banda speculare sul bordo superiore ("vetro")
//    - alone colorato
//    - griglia di puntini
//    - onda di pressione animata
//
//  Prima ogni tasto costava 4 primitive (ombra, corpo, riflesso,
//  bordo) = ~100 draw call per frame, con smussature che dipendevano
//  interamente dal MSAA 4x della finestra.
// ============================================================

typedef struct {
    Rectangle rect;

    float radius;          // px

    Color top, bottom;     // gradiente verticale del corpo
    float highlight;       // banda lucida sul bordo superiore [0..1]
    float shadow;          // ombra esterna [0..1]
    float shadowSpread;    // ampiezza dell'ombra in px
    Color  shadowColor;    // tinta dell'ombra (di solito nero)
    Color  border;         // bordo interno sottile
    float  borderW;

    Color  glow;           // alone dietro al pannello (alpha = forza)
    float  glowRadius;

    float  dotStrength;    // griglia di puntini (display)
    float  dotSpacing;     // px
    Color  dotColor;

    // Onda di pressione: centro (px relativi al rettangolo), raggio
    // corrente e forza residua. Tutti a zero se non e' attiva.
    float rippleX, rippleY, rippleR, rippleA;
} Panel;

// Sfondo dell'app: pannello arrotondato + gradiente diagonale +
// alone dietro al display + vignettatura. Un solo draw call.
typedef struct {
    Rectangle panel;
    float     radius;
    Color     top, bottom;
    Color     border;
    float     borderW;
    Vector2   glowCenter;  // 0..1 del pannello
    float     glowRadius;  // 0..1 del pannello
    Color     glowColor;   // tinta dell'alone
    float     glowStrength;
    float     vignette;
    Color     vignetteColor;
} Background;

// Compila gli shader. Chiamare una volta dopo InitWindow.
// Se la compilazione fallisce RendererAvailable() restituisce false
// e il disegno ricade automaticamente sulle primitive di raylib.
void  RendererInit(void);
void  RendererShutdown(void);
bool  RendererAvailable(void);

// Aggiorna la dimensione del framebuffer usata per ricavare le
// coordinate dei pixel. Chiama una sola glUniform quando cambia,
// quindi e' sicuro invocarla a ogni frame.
void  RendererSetFramebuffer(int width, int height);

// Deve essere chiamato dentro BeginDrawing/EndDrawing.
void  DrawPanel(const Panel* p);
void  DrawBackground(const Background* b);

#endif
