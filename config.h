#ifndef CONFIG_H
#define CONFIG_H

#include "raylib.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// ============================================================
//  COSTANTI DI LAYOUT (tutto proporzionale: la finestra puo'
//  essere ridimensionata e tile-d senza che nulla si rompa)
// ============================================================
#define SCREEN_WIDTH   420
#define SCREEN_HEIGHT  640

// Frazione della finestra occupata dal display
#define DISPLAY_TOP    0.055f
#define DISPLAY_HEIGHT 0.245f

// Gap fra i tasti, come frazione del lato minimo della finestra
#define GRID_GAP       0.022f
// Margine esterno della griglia, come frazione della larghezza
#define GRID_MARGIN_X  0.055f

// Cronologia
#define MAX_HISTORY 20

// Buffer del display
#define MAX_EXPR_TEXT  160
#define MAX_RESULT      48

// Intervallo fra due interrogazioni del tema di sistema.
// Rilevare il tema AVVIA UN PROCESSO (popen/D-Bus): costa ~5 ms.
// Non e' piu' nel main loop, ma comunque non ha senso chiedere piu'
// di una volta ogni qualche secondo.
#define THEME_POLL_SECONDS  10
// Il thread dorme a scatti di 50 ms: puo' cosi' smettere subito
#define THEME_POLL_TICK_US   50000
#define THEME_POLL_TICKS     ((THEME_POLL_SECONDS*1000000)/THEME_POLL_TICK_US)

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <stdlib.h>
#endif

// ============================================================
//  RILEVAMENTO TEMA DI SISTEMA  (COSTOSO - lancia un processo)
//  Ritorna true se il tema di sistema e' scuro.
//  Va eseguito una volta all'avvio e poi in un thread separato.
//  Implementazione in ui/theme.c.
// ============================================================
bool DetectSystemThemeDark(void);

#endif
