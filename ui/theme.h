#ifndef THEME_H
#define THEME_H

#include <raylib.h>
#include <stdbool.h>

// ============================================================
//  PALETTE CENTRALIZZATA
//  Un solo posto dove vivono i colori. Ogni elemento e' descritto
//  da una coppia di colori (alto/basso) per il gradiente, non da un
//  colore piatto: e' lo shader a trasformarlo in superficie.
// ============================================================

// Stile di un tasto o di un pannello.
typedef struct {
    Color  top, bottom;     // gradiente verticale del corpo
    Color  text;            // etichetta
    Color  border;          // bordo interno di stacco
    float  borderW;
    float  highlight;       // banda lucida sul bordo superiore [0..1]
    float  shadow;          // ombra esterna [0..1]
    Color  glowColor;       // alone dietro al tasto
    float  glowStrength;    // 0 = nessun alone
} Style;

typedef struct {
    bool dark;

    // --- sfondo dell'app ---
    Color bgTop, bgBottom;
    Color bgBorder;
    Color bgGlow;           // alone dietro al display
    float bgGlowStrength;
    Color bgVignette;

    // --- display ---
    Color displayTop, displayBottom;
    Color displayBorder;
    Color displayDot;
    Color displayShadow;

    // --- testo ---
    Color text;             // espressione corrente
    Color textDim;          // etichette secondarie, "Ans"
    Color result;           // risultato
    Color error;            // risultato non valido
    Color caret;            // cursore di digitazione

    // --- tasti ---
    Style key;              // cifre e '.'
    Style op;               // + - * / %
    Style eq;               // '='
    Style danger;           // 'C'
} Theme;

// Inizializza la palette (una sola volta all'avvio).
void  ThemeInit(void);

// Il rilevamento del tema di sistema lancia un processo esterno e
// costa ~5 ms: viene fatto in un thread separato, cosi' il main loop
// non si blocca mai. Ritorna true se la palette e' cambiata.
bool  ThemePollSystem(void);

void  ThemeStop(void);         // ferma il thread (chiamare alla fine)

bool  ThemeIsDark(void);
const Theme* ThemeGet(void);

#endif
