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

// Smussi
#define ROUND_APP     0.16f
#define ROUND_DISPLAY 0.13f
#define ROUND_BUTTON  1.0f   // 1.0 = pillola/cerchio perfetto
#define SEGM          16

// Frazione della finestra occupata dal display
#define DISPLAY_TOP    0.055f
#define DISPLAY_HEIGHT 0.235f

// Gap fra i tasti, come frazione del lato minimo della finestra
#define GRID_GAP       0.022f
// Margine esterno della griglia, come frazione della larghezza
#define GRID_MARGIN_X  0.055f

// Cronologia
#define MAX_HISTORY 20

// Buffer del display
#define MAX_EXPR_TEXT  160
#define MAX_RESULT      48

// Intervallo minimo fra due interrogazioni del tema di sistema.
// Rilevare il tema AVVIA UN PROCESSO (popen/D-Bus): costa ~30 ms,
// quindi va fatto il piu' raramente possibile, mai per frame.
#define THEME_POLL_SECONDS 10.0

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <stdlib.h>
#endif

// ============================================================
//  RILEVAMENTO TEMA DI SISTEMA  (COSTOSO - non chiamare per frame)
//  Ritorna true se il tema di sistema e' scuro.
//  AVVIA UN PROCESSO: su Linux costa ~30 ms.
//  Implementazione in ui/theme.c.
// ============================================================
bool DetectSystemThemeDark(void);

#endif
