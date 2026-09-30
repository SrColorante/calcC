#ifndef THEME_H
#define THEME_H

#include <raylib.h>
#include <stdbool.h>

// ============================================================
//  PALETTE CENTRALIZZATA
//  Tutti i colori vivono qui: la UI non interpola piu' i colori
//  dentro ogni bottone a ogni frame (erano 18 confronti ternary
//  + 18 strcmp per frame solo per scegliere il colore).
// ============================================================
typedef struct {
    bool dark;

    Color bg;            // sfondo app
    Color outline;       // bordo app
    Color shadow;        // ombra sotto i tasti (rgba, alpha basso)

    Color displayBg;     // riquadro display
    Color displayLine;   // bordo display
    Color displayDot;    // griglia di sfondo del display

    Color text;          // testo primario
    Color textDim;       // espressione, etichette secondarie
    Color result;        // colore del risultato

    Color keyBg;         // tasti numerici
    Color keyBgHover;
    Color keyText;

    Color opBg;          // tasti operatori
    Color opBgHover;
    Color opText;

    Color eqBg;          // tasto '='
    Color eqBgHover;
    Color eqText;

    Color dangerBg;      // tasto 'C'
    Color dangerBgHover;
    Color dangerText;
} Theme;

// Palette inizializzata UNA volta sola all'avvio.
void  ThemeInit(void);

// Rilevamento di sistema, eseguito al massimo una volta ogni
// THEME_POLL_SECONDS e MAI dentro il loop di disegno.
// Ritorna true se il tema e' cambiato (e quindi va ricalcolata la palette).
bool  ThemePollSystem(void);

// true = tema scuro
bool  ThemeIsDark(void);

const Theme* ThemeGet(void);

#endif
