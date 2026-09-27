# Documentazione Assoluta Riga per Riga (docs.md)

Di seguito l'intera codebase del progetto commentata in ogni sua riga per comprenderne appieno la logica.

## 1. `config.h`
```c
#ifndef CONFIG_H // Previene l'inclusione multipla del file (Include Guard)
#define CONFIG_H // Definisce il token per l'Include Guard

#define SCREEN_WIDTH 400 // Definisce la larghezza di partenza della finestra a 400 pixel
#define SCREEN_HEIGHT 600 // Definisce l'altezza di partenza della finestra a 600 pixel
#define FONT_SIZE 24 // Dimensione di default per i testi dei bottoni
#define SMUSS 0.45f // Livello di arrotondamento. 0.45 = bordi molto smussati
#define SEGM 16 // Qualità dell'arrotondamento (numero di segmenti generati per curva)

#include "raylib.h" // Include le funzionalità della libreria grafica Raylib
#include <stdbool.h> // Aggiunge il supporto per il tipo booleano in C standard (true/false)

// Funzione globale usata per rilevare se il sistema ha un tema dark o light
static inline int IsSystemThemeDark() { // Viene usata la key 'static inline' per evitare conflitti di linking
    // Apriamo un processo per interrogare gsettings, specifico per Gnome o simili (Wayland)
    FILE *fp = popen("gsettings get org.gnome.desktop.interface color-scheme", "r"); 
    if (!fp) return 1; // Se il comando fallisce, diamo per scontato che sia il tema Dark per sicurezza
    char buffer[128]; // Buffer temporaneo per salvare l'output del comando
    if (fgets(buffer, sizeof(buffer), fp) != NULL) { // Leggiamo il risultato ottenuto dal terminale
        pclose(fp); // Chiudiamo subito il processo
        // Se nel risultato troviamo la stringa "dark", ritorna 1 (vero), altrimenti 0
        return (strstr(buffer, "dark") != NULL) ? 1 : 0; 
    }
    pclose(fp); // Chiudiamo il processo se non abbiamo letto nulla
    return 1; // Default a Dark theme
}

#endif // Fine del blocco di Include Guard
```

## 2. `ui/button.h`
```c
#ifndef BUTTON_H // Include guard per button.h
#define BUTTON_H // Definizione token include guard

#include "raylib.h" // Necessario per usare 'Rectangle' e 'Vector2'

// Enum per definire tre possibili layout per i bottoni
typedef enum { 
    BTN_NORMAL, // Bottone standard quadrato 1x1
    BTN_H_LONG, // Bottone allungato in orizzontale (es. il tasto '.')
    BTN_V_LONG  // Bottone allungato in verticale (es. il tasto '=')
} ButtonShape;

// Struttura che definisce ogni pulsante della UI
typedef struct {
    int grid_x; // Coordinata logica orizzontale sulla griglia
    int grid_y; // Coordinata logica verticale sulla griglia
    ButtonShape shape; // Il tipo di layout scelto per questo tasto
    char text[8]; // Array di caratteri che contiene l'etichetta del bottone (es "9", "C", "%")
    Rectangle rect; // Struct di Raylib che salva coordinate reali (x, y) e dimensioni (width, height)
    bool is_hovered; // Flag impostato a true se il mouse ci passa sopra
    bool is_pressed; // Flag impostato a true se l'utente ci ha cliccato
    float visual_press_timer; // Timer che decresce per animare il tasto come premuto quando si usa la tastiera
} Button; // Nome del tipo creato dalla struct

// Dichiara la funzione di disegno del bottone passandogli i suoi dati, la posizione del mouse e il font da usare
void DrawCalcButton(Button* btn, Vector2 mousePos, Font font);

#endif // Fine Include Guard
```

## 3. `ui/button.c`
```c
#include "button.h" // Include la dichiarazione della struct Button
#include "../config.h" // Include le macro (SMUSS) e IsSystemThemeDark
#include <math.h> // Libreria matematica per funzioni grafiche base

void DrawCalcButton(Button* btn, Vector2 mousePos, Font font) { // Corpo della funzione di rendering
    // Controlliamo in tempo reale se il sistema operativo sta usando il tema scuro
    bool isDark = (IsSystemThemeDark() == 1);
    
    // Generiamo l'ombra esterna sfalsandola in basso e a destra (+3, +4 pixel)
    Rectangle shadowRec = { btn->rect.x + 3, btn->rect.y + 4, btn->rect.width, btn->rect.height };
    // Disegniamo l'ombra usando 1.0f di curvatura (massima) con una trasparenza (Alpha 50 per ombre leggere)
    DrawRectangleRounded(shadowRec, 1.0f, SEGM, (Color){ 0, 0, 0, 50 });
    
    // Impostiamo il colore di base del liquido. Variamo in base a tema Scuro o Chiaro. (Alpha 180 = traslucido)
    Color bodyColor = isDark ? (Color){ 40, 45, 55, 180 } : (Color){ 210, 220, 230, 180 };
    if (btn->is_hovered) { // Se il mouse ci passa sopra
        // Schiariamo leggermente la goccia rendendola più opaca (Alpha 200)
        bodyColor = isDark ? (Color){ 60, 65, 75, 200 } : (Color){ 230, 240, 250, 200 };
    }
    // Se il tasto è fisicamente cliccato col mouse OPPURE animato tramite tastiera
    if (btn->is_pressed || btn->visual_press_timer > 0.0f) {
        // Il bottone diventa ancora più luminoso e denso
        bodyColor = isDark ? (Color){ 80, 90, 110, 230 } : (Color){ 190, 210, 230, 230 };
    }

    // Disegniamo la massa principale della goccia, usando bodyColor calcolato poco fa. Raggio 1.0f = tondo perfetto.
    DrawRectangleRounded(btn->rect, 1.0f, SEGM, bodyColor);
    
    // Per ricreare il riflesso della luce (Specular highlight), calcoliamo un piccolo riquadro
    // posizionato leggermente in alto a sinistra, grande solo una frazione del bottone originale.
    Rectangle highlightRec = { 
        btn->rect.x + btn->rect.width * 0.15f,  // Spostato al 15% della larghezza
        btn->rect.y + btn->rect.height * 0.10f, // Spostato al 10% dell'altezza
        btn->rect.width * 0.5f,                 // Largo il 50%
        btn->rect.height * 0.35f                // Alto il 35%
    };
    // Disegniamo l'highlight con colore completamente bianco ma appena percettibile (Alpha 80)
    DrawRectangleRounded(highlightRec, 1.0f, SEGM, (Color){ 255, 255, 255, 80 });

    // Per completare l'effetto "bolla", calcoliamo una riflessione secondaria sul fondo a destra (Caustiche)
    Rectangle bottomReflect = {
        btn->rect.x + btn->rect.width * 0.4f, // Spostato a destra del 40%
        btn->rect.y + btn->rect.height * 0.7f, // Molto in basso, 70%
        btn->rect.width * 0.5f, // Largo il 50%
        btn->rect.height * 0.2f // Schiacciato: alto solo il 20%
    };
    // Lo disegniamo con opacità quasi nulla (Alpha 30) per dare appena profondità
    DrawRectangleRounded(bottomReflect, 1.0f, SEGM, (Color){ 255, 255, 255, 30 });

    // Calcoliamo lo spazio occupato dall'etichetta testuale usando il nostro font Comfortaa
    Vector2 textSize = MeasureTextEx(font, btn->text, FONT_SIZE, 1);
    
    // Matematica di centratura: coordinate bottone + (metà della larghezza rimasta dopo aver tolto il testo)
    Vector2 textPos = {
        btn->rect.x + (btn->rect.width - textSize.x) / 2.0f,
        btn->rect.y + (btn->rect.height - textSize.y) / 2.0f
    };
    // Scegliamo bianco o nero per il testo a seconda del tema di sistema per garantire leggibilità
    Color textColor = isDark ? WHITE : BLACK;
    // Disegniamo il testo sul monitor usando la posizione, dimensione e spaziatura calcolata
    DrawTextEx(font, btn->text, textPos, FONT_SIZE, 1, textColor);
}
```

## 4. `logic/eval.h`
```c
#ifndef EVAL_H // Prevenzione inclusione multipla
#define EVAL_H // Token guard

#include <stdbool.h> // Permette il tipo bool

// Definisce quali tipologie di dati un nodo può ospitare: numeri oppure operatori matematici
typedef enum {
    NODE_NUM,
    NODE_OP
} NodeType;

// Definizione del nodo stesso, base dell'Albero/Lista concatenata
typedef struct ExprNode {
    NodeType type; // Il tipo di contenuto: NODE_NUM o NODE_OP
    union { // Usiamo una 'union' per salvare RAM: il nodo conterrà O un int O un char (condividono stessa memoria)
        int n; // Memorizza la singola cifra
        char op; // Memorizza il simbolo dell'operatore (es. '+')
    };
    struct ExprNode* next; // Puntatore all'elemento successivo della lista
} ExprNode;

// Dichiarazione delle funzioni esportate dal motore logico
void AppendOp(ExprNode** head, char op); // Aggiunge operatore in coda alla lista
void AppendNum(ExprNode** head, int n); // Aggiunge cifra in coda
void PopNode(ExprNode** head); // Rimuove l'ultimo nodo (per il tasto Cancella)
void ClearExpr(ExprNode** head); // Svuota e dealloca intera lista
ExprNode* CopyExpr(ExprNode* head); // Duplica l'intera lista per salvare i dati in Cronologia
double EvaluateExpr(ExprNode* head, bool* error); // Algoritmo per risolvere l'espressione (ritorna double e setta l'errore)
void ExprToString(ExprNode* head, char* buffer, int max_len); // Traduce la lista concatenata in una stringa visibile

extern double lastResult; // Espone globalmente l'ultimo risultato salvato per essere inniettato (la logica "Ans")

#endif // Chiusura header
```

## 5. `logic/eval.c`
```c
#include "eval.h"
#include <stdlib.h> // Libreria per l'allocazione dinamica della memoria (malloc, free)
#include <stdio.h> // Standard I/O (snprintf)
#include <string.h> // Gestione stringhe (strcat, strlen)
#include <math.h> // Necessario per l'operatore fmod (Modulo)

void AppendOp(ExprNode** head, char op) { // Crea e inserisce un nodo Operatore in coda
    ExprNode* node = (ExprNode*)malloc(sizeof(ExprNode)); // Alloca RAM dinamicamente per il nodo
    node->type = NODE_OP; // Setta il tipo
    node->op = op; // Salva il carattere (es '+')
    node->next = NULL; // Il nuovo nodo è il capolinea
    if (!*head) *head = node; // Se la lista è vuota, il nuovo nodo diventa la testa
    // Altrimenti, scorriamo i nodi fino ad arrivare in fondo e attacchiamo il nuovo nodo alla coda
    else { ExprNode* curr = *head; while (curr->next) curr = curr->next; curr->next = node; }
}

void AppendNum(ExprNode** head, int n) { // Crea e inserisce un nodo Numero (cifra singola)
    ExprNode* node = (ExprNode*)malloc(sizeof(ExprNode)); // Alloca RAM
    node->type = NODE_NUM; // Setta il tipo
    node->n = n; // Salva la cifra singola (0-9)
    node->next = NULL; // Capolinea
    if (!*head) *head = node; // Se lista vuota, diventa testa
    // Altrimenti scorre fino in fondo e lo appende
    else { ExprNode* curr = *head; while (curr->next) curr = curr->next; curr->next = node; }
}

double lastResult = 0.0; // Inizializza a zero la variabile globale del risultato

void ClearExpr(ExprNode** head) { // Svuota la lista ed evita Memory Leaks
    ExprNode* curr = *head; // Punta al primo nodo
    while (curr) { // Finché ci sono nodi validi
        ExprNode* next = curr->next; // Salva la reference al successivo
        free(curr); // Distrugge liberando la RAM dell'attuale
        curr = next; // Avanza al nodo salvato
    }
    *head = NULL; // Resetta il puntatore originale a NULL! Importantissimo.
}

ExprNode* CopyExpr(ExprNode* head) { // Deep Copy: clona l'albero nodo per nodo
    if (!head) return NULL; // Ritorna NULL per alberi vuoti
    ExprNode* newHead = NULL; // Crea testa locale
    ExprNode* curr = head; // Copia il reference dell'albero di origine
    while (curr) { // Per ogni nodo...
        if (curr->type == NODE_NUM) AppendNum(&newHead, curr->n); // Duplica usando la logica del motore se numero
        else AppendOp(&newHead, curr->op); // Duplica se operatore
        curr = curr->next; // Scorre
    }
    return newHead; // Ritorna l'albero gemello allocato in RAM separata (utile per la History)
}

void PopNode(ExprNode** head) { // Backspace logico (cancella solo 1 nodo)
    if (!*head) return; // Niente da fare se vuoto
    if (!(*head)->next) { // Se l'albero ha 1 SOLO nodo
        free(*head); // Lo libera
        *head = NULL; // Lo svuota del tutto
        return;
    }
    ExprNode* curr = *head; // Parte dalla testa
    while (curr->next && curr->next->next) { // Scorre finché NON trova il penultimo nodo
        curr = curr->next; 
    }
    free(curr->next); // Il penultimo nodo dealloca il suo "next" (l'ultimo nodo della lista)
    curr->next = NULL; // Il penultimo nodo adesso non ha più figli e diventa a tutti gli effetti l'ultimo
}

double EvaluateExpr(ExprNode* head, bool* error) { // Motore risolutivo
    *error = false; // Presuppone successo
    if (!head) return 0; // Lista vuota = 0
    
    // Dobbiamo estrarre la Lista Concatenata e convertirla in Array per fare i calcoli più comodamente
    double vals[100]; // Conterrà tutti i numeri uniti (Es. nodi [1, ., 5] -> double 1.5)
    char ops[100]; // Conterrà gli operatori
    int v_count = 0, o_count = 0; // Quantità estratti
    
    ExprNode* curr = head; // Puntatore di scansione
    while (curr) { // Finché c'è un nodo...
        // Se troviamo un numero o un Punto decimale...
        if (curr->type == NODE_NUM || (curr->type == NODE_OP && curr->op == '.')) {
            double val = 0; // Accumulatore
            double decimal_mult = 1; // Moltiplicatore decimale (diventa 0.1, 0.01)
            bool in_decimal = false; // Flag se siamo post-virgola
            
            // Finché troviamo consecutivamente numeri o punti...
            while (curr && (curr->type == NODE_NUM || (curr->type == NODE_OP && curr->op == '.'))) {
                if (curr->type == NODE_OP && curr->op == '.') {
                    in_decimal = true; // Sblocca flag decimale
                } else if (curr->type == NODE_NUM) { // È una cifra intera
                    if (!in_decimal) val = val * 10 + curr->n; // Sposta di base (es. accumulato 1, arriva 5 -> 1*10+5 = 15)
                    else { // Se post virgola
                        decimal_mult /= 10.0; // Scaliamo di decimi
                        val = val + curr->n * decimal_mult; // 15 + (5 * 0.1) = 15.5
                    }
                }
                curr = curr->next; // Scorre i nodi agglomerandoli
            }
            vals[v_count++] = val; // Inserisce il numero finale condensato in vals
        } else { // Se era solo un operatore puro (+, *, -, /)
            ops[o_count++] = curr->op; // Viene registrato in ops
            curr = curr->next;
        }
    }
    
    // Validation sintattica (Syntax Error). Se zero numeri ma N operatori o Operatori >= Numeri 
    // Esempio "1+*", avremo 1 val (v_count) e 2 op (+ e *). 2 >= 1 -> ERRORE!
    if (v_count == 0 && o_count > 0) { *error = true; return 0; }
    if (o_count >= v_count) { *error = true; return 0; }
    
    // RISOLUZIONE: MOLTIPLICAZIONI, DIVISIONI E MODULI (Hanno priorità matematica, le facciamo prima)
    for (int i=0; i<o_count; i++) { // Scorriamo l'array operatori 
        if (ops[i] == '*' || ops[i] == '/' || ops[i] == '%') { // Trovato calcolo prioritario
            if (ops[i] == '*') vals[i] = vals[i] * vals[i+1]; // Risolve
            if (ops[i] == '/') {
                if (vals[i+1] == 0) { *error = true; return 0; } // DIVISION BY ZERO = MATH ERROR
                vals[i] = vals[i] / vals[i+1]; // Risolve
            }
            if (ops[i] == '%') { // Modulo (Resto)
                if (vals[i+1] == 0) { *error = true; return 0; }
                vals[i] = fmod(vals[i], vals[i+1]); // fmod fa il calcolo modulo sui floating point in C
            }
            // Collassiamo gli array riempiendo il buco dei valori risolti (es. l'1 e 2 si fondono, il 3 scala giù)
            for (int j=i+1; j<v_count-1; j++) vals[j] = vals[j+1];
            for (int j=i; j<o_count-1; j++) ops[j] = ops[j+1]; // L'operatore sparito fa scorrere gli altri
            v_count--; o_count--; i--; // Decrementiamo i contatori di loop e array
        }
    }
    
    // RISOLUZIONE: ADDIZIONI E SOTTRAZIONI (Senza più rischio di violare BODMAS)
    for (int i=0; i<o_count; i++) {
        if (ops[i] == '+' || ops[i] == '-') {
            if (ops[i] == '+') vals[i] = vals[i] + vals[i+1]; // Risolve e salva al posto del primo termine
            if (ops[i] == '-') vals[i] = vals[i] - vals[i+1];
            // Collassa il resto dell'array identico a sopra
            for (int j=i+1; j<v_count-1; j++) vals[j] = vals[j+1];
            for (int j=i; j<o_count-1; j++) ops[j] = ops[j+1];
            v_count--; o_count--; i--;
        }
    }
    
    if (v_count > 0) return vals[0]; // Restituisce l'unico numero scampato (Il risultato finale!)
    return 0; // Fail-safe
}

// Stampa la lista concatenata a video per poterla mostrare
void ExprToString(ExprNode* head, char* buffer, int max_len) {
    buffer[0] = '\0'; // Resetta il buffer per evitare memorie fantasma passate
    ExprNode* curr = head; // Parte dall'albero
    while (curr) { // Per ogni nodo
        char temp[32]; // Crea testo temporaneo del singolo nodo
        // Usa snprintf che protegge dai buffer overflow per scrivere la stringa convertita
        if (curr->type == NODE_NUM) snprintf(temp, sizeof(temp), "%d", curr->n);
        else {
            if (curr->op == 's') snprintf(temp, sizeof(temp), "sqrt("); // Codice fallback obsoleto
            else snprintf(temp, sizeof(temp), "%c", curr->op);
        }
        // Se unendo buffer + temporaneo stiamo sotto la memoria consentita, concatena
        if (strlen(buffer) + strlen(temp) < (size_t)max_len) strcat(buffer, temp);
        curr = curr->next; // Avanza
    }
}
```

## 6. `main.c`
```c
#include "raylib.h"
#include "config.h"
#include "ui/button.h"
#include "logic/eval.h"
#include <stdio.h>
#include <string.h>

#define NUM_BUTTONS 20 // Quanti bottoni massimi supporta la griglia
Button buttons[NUM_BUTTONS]; // Crea l'array per salvarne i dati
ExprNode* exprList = NULL; // Puntatore Radice / Testa del parser espressioni. Parte a NULL.
char displayBuffer[256] = ""; // Stringa di testo dove salvare l'espressione da mostrare su schermo
char resultBuffer[256] = ""; // Stringa di testo dove salvare il risultato calcolato da mostrare in verde

// Stati usati nel Game Loop per governare animazioni
float animSlideUp = 0.0f; // Misura lo slancio verticale dei calcoli (da 0.0 a 1.0)
bool isResultState = false; // Vero se l'utente ha calcolato il risultato
float popAnim = 0.0f; // Moltiplicatore rimbalzo testo per feedback meccanico
int lastDisplayLen = 0; // Memorizza quanto era lungo il testo un frame fa per scatenare animazioni

// Trova un bottone in base al testo e imposta il suo timer
void TriggerButtonVisual(const char* label) {
    for (int i=0; i<NUM_BUTTONS; i++) { // Scorre i 20 bottoni
        if (strcmp(buttons[i].text, label) == 0) { // Controlla uguaglianza
            buttons[i].visual_press_timer = 0.15f; // Lo farà rimanere illuminato per 150 millisecondi
            break; // Ottimizzazione per uscire dal loop quando lo troviamo
        }
    }
}

// Layout Dinamico (Risponde in tempo reale al tiling di Wayland)
void UpdateLayout(void) {
    float sw = GetScreenWidth(); // Larghezza della finestra al frame corrente 
    float sh = GetScreenHeight(); // Altezza effettiva al frame
    
    float displayHeight = sh * 0.30f; // Il display LCD riserva 30% del monitor
    float buttonsAreaHeight = sh * 0.60f; // La zona bottoni ottiene il 60%
    
    float marginX = sw * 0.05f; // Margine Orizzontale al 5%
    float marginY = sh * 0.05f; // Margine Verticale 5%
    
    float availableWidth = sw - 2*marginX; // La larghezza sfruttabile della griglia
    float pad = 16.0f; // Distanza fissa per distanziare le gocce d'acqua
    // Algoritmo: lo spazio per N bottoni è: (Spazio - tutti i pad centrali(3)) / Colonne(4)
    float w = (availableWidth - 3*pad) / 4.0f; 
    float h = (buttonsAreaHeight - 4*pad) / 5.0f;
    
    float startX = marginX; // Cursore di partenza asse X
    float startY = displayHeight + marginY; // Cursore di partenza asse Y
    
    // Mappatura fissa 2D testuale della nostra tastiera (Notare i null negli spot speciali vuoti)
    const char* labels[5][4] = {
        {"C", "%", "*", "/"},
        {"7", "8", "9", "-"},
        {"4", "5", "6", "+"},
        {"1", "2", "3", "="},
        {"0", ".", "", ""} 
    };
    
    int btn_idx = 0; // Cursore per avanzare nell'array di struct bottoni reali
    for (int r = 0; r < 5; r++) { // 5 Righe
        for (int c = 0; c < 4; c++) { // 4 Colonne
            if (r == 4 && c == 2) continue; // Salta il vuoto per colpa di .= (prendono 2 spot)
            if (r == 4 && c == 3) continue; // Salta l'altro vuoto
            
            buttons[btn_idx].grid_x = c; // Salva logica
            buttons[btn_idx].grid_y = r; // Salva logica
            strcpy(buttons[btn_idx].text, labels[r][c]); // Copia l'etichetta
            
            // X: offset iniziale + salto colonna *(Larghezza bottone + Margine)
            float bx = startX + c * (w + pad); 
            float by = startY + r * (h + pad);
            
            if (r == 4 && c == 1) { // Caso speciale: lo Zero
                buttons[btn_idx].shape = BTN_H_LONG; // Tasto extra largo!
                buttons[btn_idx].rect = (Rectangle){ bx, by, w*2 + pad, h }; // Eredita la forma x2+margine
            } else if (r == 3 && c == 3) { // Caso speciale: L'Uguale
                buttons[btn_idx].shape = BTN_V_LONG; // Tasto extra Alto!
                buttons[btn_idx].rect = (Rectangle){ bx, by, w, h*2 + pad };
            } else {
                buttons[btn_idx].shape = BTN_NORMAL; // Gocce standard
                buttons[btn_idx].rect = (Rectangle){ bx, by, w, h };
            }
            btn_idx++; // Passa allo spot successivo
        }
    }
}

// 20 Slot per salvare le espressioni digitate e recuperarle con frecce SU/GIÙ
#define MAX_HISTORY 20
ExprNode* history[MAX_HISTORY] = {0}; // Vettore di alberi logici inizializzati a NULL (0)
int history_count = 0; // Quanti slot sono riempiti
int history_idx = -1; // Indice del quale stiamo visualizzando

// La funzione Ans: Logica complessa che decide quando un bottone avvia un calcolo nuovo o usa il risultato del precedente
void HandleAnsLogic(bool is_operator, char ch, int num) {
    char lbl[2] = {0, 0}; // Vettore testuale per estrarre la label
    if (num != -1) { // Se stiamo analizzando un numero
        lbl[0] = '0' + num; // Converte Int a Stringa usando Ascii
        TriggerButtonVisual(lbl); // Simula pressione bottone visivo!
    } else if (ch != '\0') { // Se è un operatore (in formato Char)
        lbl[0] = ch; // Appende il char all'array
        TriggerButtonVisual(lbl); // Simula pressione
    }

    if (isResultState) { // Se il programma aveva un calcolo concluso (schermo verde e testo alzato)
        if (history_count < MAX_HISTORY) { // Se c'è spazio nello storico
            history[history_count++] = CopyExpr(exprList); // Duplica l'albero con CopyExpr e lo salva
            history_idx = history_count; // Riporta indice storico al fondo
        } else { // Se la cronologia è piena, "Shifta" l'array
            ClearExpr(&history[0]); // Cancella il ricordo più vecchio distruggendolo
            for (int i=1; i<MAX_HISTORY; i++) history[i-1] = history[i]; // Sposta tutti indietro di 1 
            history[MAX_HISTORY-1] = CopyExpr(exprList); // Sbatte l'ultimo clone alla fine
            history_idx = MAX_HISTORY; // Indice allineato
        }

        if (is_operator) { // Se ho premuto + DOPO un Risultato
            ClearExpr(&exprList); // Cancella l'espressione, ma dobbiamo riniettare Ans!
            char ansStr[64];
            snprintf(ansStr, sizeof(ansStr), "%g", lastResult); // %g parsa i Float eliminando gli zero inutili
            if (lastResult < 0) AppendNum(&exprList, 0); // Hack Fix: Se il risultato precedente era -5, inietta uno '0' per risolvere "0-5" al posto del "-" puro che spacca BODMAS.
            for (size_t i=0; i<strlen(ansStr); i++) { // Ricrea il parser della stringa simulando la digitazione manuale dell'utente! 
                if (ansStr[i] == '.') AppendOp(&exprList, '.');
                else if (ansStr[i] == '-') AppendOp(&exprList, '-');
                else if (ansStr[i] >= '0' && ansStr[i] <= '9') AppendNum(&exprList, ansStr[i] - '0');
            }
            if (ch != '\0') AppendOp(&exprList, ch); // Conclude attaccando il "+" (o altro) finale dell'operatore che ha scatenato la condizione
        } else { // Se ho premuto 5 DOPO un Risultato
            ClearExpr(&exprList); // Capisce che non ti interessava Ans
            if (num != -1) AppendNum(&exprList, num); // E comincia inserendo subito la tua cifra per un nuovo calcolo!
            else if (ch != '\0') AppendOp(&exprList, ch);
        }
        isResultState = false; // Resetta lo stato di vittoria (Result = off)
        strcpy(resultBuffer, ""); // Svuota lo string del display verde
    } else { // Se è normale utilizzo (non c'è alcun = stato eseguito)
        if (num != -1) AppendNum(&exprList, num); // Aggiungi brutalmente il numero...
        else if (ch != '\0') AppendOp(&exprList, ch); // O il tasto alla coda.
    }
}

// Scansiona Eventi Tastiera 
void HandleKeyboardInput() {
    int key = GetKeyPressed(); // Cerca tasti controllo e speciali
    while (key != 0) { // Finché il buffer Raylib è sporco
        // KEY REPEAT ATTIVO PER BACKSPACE E CANC! (Fondamentale in Wayland)
        if (key == KEY_BACKSPACE || IsKeyPressedRepeat(KEY_BACKSPACE)) { 
            if (isResultState) { // Se l'ho fatto DOPO aver calcolato un risultato
                isResultState = false; // Semplice svuotamento
                strcpy(resultBuffer, "");
            } else {
                PopNode(&exprList); // Cancella l'ultimo pezzo usando la funzione Pop sicura di eval.c
            }
        } else if (key == KEY_C || key == KEY_DELETE) { // Cancellare TUTTO
            ClearExpr(&exprList);
            strcpy(resultBuffer, "");
            isResultState = false;
            TriggerButtonVisual("C"); // Illumina la bolla C
        // Se premi invio in un qualsiasi format della tua tastiera
        } else if (key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_EQUAL || key == KEY_KP_EQUAL) {
            bool err;
            lastResult = EvaluateExpr(exprList, &err); // Lancia il Motore Matematico!
            if (err) strcpy(resultBuffer, "Error"); // Syntax error
            else snprintf(resultBuffer, sizeof(resultBuffer), "%g", lastResult); // Success
            isResultState = true; // Sblocca layout vittorioso
            animSlideUp = 0.0f; // Azzerra l'animazione per farla ripartire da sotto verso il centro
            TriggerButtonVisual("=");
        } else if (key == KEY_UP) { // Freccia SU: Torna nel Passato!
            if (history_count > 0 && history_idx > 0) {
                history_idx--; // Sposta indietro la telecamera
                ClearExpr(&exprList); // Cancella
                exprList = CopyExpr(history[history_idx]); // E popola clonando la storia a questo frame!
                isResultState = false;
                strcpy(resultBuffer, "");
            }
        } else if (key == KEY_DOWN) { // Freccia GIÙ: Avanza verso i calcoli del Futuro
            if (history_idx < history_count - 1) { // Idem, ma a salire
                history_idx++;
                ClearExpr(&exprList);
                exprList = CopyExpr(history[history_idx]);
                isResultState = false;
                strcpy(resultBuffer, "");
            } else if (history_idx == history_count - 1) { // Se arrivi in cima, pulisci per il foglio bianco
                history_idx++;
                ClearExpr(&exprList);
                isResultState = false;
                strcpy(resultBuffer, "");
            }
        }
        key = GetKeyPressed(); // Assorbe il key per non iterare all'infinito
    }
    
    // Per gestire il fastidioso comportamento del "Tengo premuto Shift e premiamo 7 -> esce / ma la calcolatrice inserisce 7 e /"
    // Ho bypassato l'inserimento fisico usando la via del CharPressed Unicode (il SO di sistema gestisce già il demultiplex dei Keycodes unendoli)
    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= '0' && ch <= '9') HandleAnsLogic(false, '\0', ch - '0'); // Se era numero (senza shift)
        else if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%') HandleAnsLogic(true, (char)ch, -1);
        else if (ch == '.') { // Se ci serve la virgola
            HandleAnsLogic(false, (char)ch, -1);
        }
        ch = GetCharPressed();
    }
}

// Viene scatenato quando il Mouse Clicca e rilascia uno dei bottoni. Stessa logica della tastiera (Mappata a HandleAnsLogic e EvaluateExpr!)
void HandleButtonPress(Button* b) {
    if (strcmp(b->text, "=") == 0) {
        bool err;
        lastResult = EvaluateExpr(exprList, &err);
        if (err) strcpy(resultBuffer, "Error");
        else snprintf(resultBuffer, sizeof(resultBuffer), "%g", lastResult);
        isResultState = true;
        animSlideUp = 0.0f;
    } else {
        if (b->text[0] >= '0' && b->text[0] <= '9') {
            HandleAnsLogic(false, '\0', b->text[0] - '0');
        } else if (strcmp(b->text, ".") == 0) {
            HandleAnsLogic(false, '.', -1);
        } else if (strcmp(b->text, "%") == 0) {
            HandleAnsLogic(true, '%', -1);
        } else if (strcmp(b->text, "C") == 0) {
            ClearExpr(&exprList);
            strcpy(resultBuffer, "");
            isResultState = false;
        } else {
            HandleAnsLogic(true, b->text[0], -1);
        }
    }
}

int main() { // Entry point
    // Applica RESIZABLE per combattere chi cerca di ingabbiare l'app (Tiling WM). 
    // Applica VSYNC per usare l'engine mailbox di Wayland che ferma i tearing grafici.
    // Applica MSAA 4X HINT per antialiasing sulle ellissi per non vederne le scalettature
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Calculator"); // Lancia finestra!
    
    if (!IsWindowReady()) { // Se fallisce crashiamo e comunichiamo in log
        printf("Error: Failed to initialize window.\n");
        return 1;
    }
    
    UpdateLayout(); // Genera i quadrati la PRIMISSIMA VOLTA
    
    // Passiamo un font Google! È importante mettere 0,0 alla fine che indica a raylib di usare tutti i Charset Unicode nativi del TTF (non limitarsi al latin!)
    Font customFont = LoadFontEx("assets/Comfortaa.ttf", 64, 0, 0);
    
    while (!WindowShouldClose()) { // Loop infinito Game Engine
        if (IsKeyPressed(KEY_ESCAPE)) break; // Esce in sicurezza se Escape!
        
        // Se qualcuno, come il mouse o Niri (il window manager) ci ha ridimensionati o forzati...
        if (IsWindowResized()) UpdateLayout(); // ... Ricalcola la griglia logica per centrare il tutto!
        
        // IMPORTANTISSIMO! GetFrameTime calcola letteralmente la frazione di millisecondi (in virgola mobile) passati dal precedente Game Loop. 
        // Lavorando in questo modo, un operazione X che richiede (0.15s), scalando il valore di "dt", si concluderà nello stesso momento (0.15s di vita reale)
        // a PRESCINDERE che il tuo monitor giri a 60 hz, 144 hz, o se il processore laggasse per mezzo secondo. (Frame-Independent Time Scale).
        float dt = GetFrameTime(); 
        Vector2 mouse = GetMousePosition(); // Intercetta X e Y del mouse passata da Wayland
        
        HandleKeyboardInput(); // Legge Buffer Input
        
        for (int i=0; i<NUM_BUTTONS; i++) { // Scorre iteratori UI
            if (buttons[i].text[0] == '\0') continue; // Salta iteratore nullo 
            
            // Applica il decadimento (usando il delta temporale 'dt'!) in modo costante del timer visuale premuto
            if (buttons[i].visual_press_timer > 0.0f) {
                buttons[i].visual_press_timer -= dt;
            }
            
            buttons[i].is_pressed = false;
            // Controlla se il mouse Logico passa sopra il Rettangolo del nostro UI Button Layout
            buttons[i].is_hovered = CheckCollisionPointRec(mouse, buttons[i].rect);
            if (buttons[i].is_hovered) { // Se entra nel raggio d'azione 
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) { // Down serve a tenere schiacciato graficamente
                    buttons[i].is_pressed = true;
                }
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) { // Released attiva l'azione vera e propria scatenando Handler
                    HandleButtonPress(&buttons[i]);
                }
            }
        }
        
        // Passa l'albero al Buffer testuale
        ExprToString(exprList, displayBuffer, sizeof(displayBuffer));
        
        // Misura istantanea di rimbalzo elastico
        int currentLen = strlen(displayBuffer); // Calcola la nuova lunghezza generata prima
        if (currentLen > lastDisplayLen) { // Se è cresciuta per aver inserito una nuova cifra..
            popAnim = 1.0f; // Attiva la spinta animata che andrà a scalare il textFont size (trigger)
        }
        lastDisplayLen = currentLen;
        
        // Aggiorna posizioni di Y dinamiche
        if (isResultState) {
            animSlideUp += dt * 10.0f; // Avanza a velocità incredibile di 10 frame temporali verso l'alto (Snappy e Instant)
            if (animSlideUp > 1.0f) animSlideUp = 1.0f; // Cap limite per dire che l'animazione ha raggiunto il bordo
        } else {
            animSlideUp -= dt * 10.0f; // Opposto
            if (animSlideUp < 0.0f) animSlideUp = 0.0f;
        }
        if (popAnim > 0.0f) {
            popAnim -= dt * 10.0f; // Si sgonfia velocemente
            if (popAnim < 0.0f) popAnim = 0.0f;
        }

        // FASE DI INIZIALIZZAZIONE DISEGNO GPU RAYLIB (Sfrutta pipeline Vulkan / OpenGL e si blocca in VSYNC)
        BeginDrawing();
        ClearBackground(BLANK); // Ripulisce i detriti del frame precedente (E essendo Wayland, BLANK=Trasparenza pura OS, senza FLAG)
        
        float appSmuss = 0.15f;
        Rectangle appRec = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        // Colora la base dell'applicativo (Finestra Niri) calcolando isDark (Color) usando (R,G,B, Alpha). 
        // 20,20,20, 180 farà intravedere lo sfondo del desktop sotto la calcolatrice dando l'effetto Vetro (Glass).
        Color appBg = (IsSystemThemeDark() == 1) ? (Color){20, 20, 20, 180} : (Color){240, 240, 240, 180};
        DrawRectangleRounded(appRec, appSmuss, SEGM, appBg); // Stampa
        
        // Rendering Testo Dinamico Display (Stampa usando algoritmi di smussatura Raylib)
        Rectangle displayRec = {
            SCREEN_WIDTH * 0.05f, 
            SCREEN_HEIGHT * 0.05f, 
            SCREEN_WIDTH * 0.90f, 
            SCREEN_HEIGHT * 0.25f
        };
        Color displayBg = (IsSystemThemeDark() == 1) ? (Color){ 10, 10, 10, 150 } : (Color){ 255, 255, 255, 150 };
        DrawRectangleRounded(displayRec, SMUSS, SEGM, displayBg); // Sfondo LCD Calcolatrice
        // E vi pone i margini tracciando solo il riquadro. GRAY : DARKGRAY crea contrasti netti
        DrawRectangleRoundedLines(displayRec, SMUSS, SEGM, 2, (IsSystemThemeDark() == 1) ? GRAY : DARKGRAY);
        
        // Calcola grandezza fluttuante dovuta a (popAnim). Di norma è 34.0, ma sotto effetto pop sale elasticamente a 40.0.
        float exprFontSize = 34.0f + (popAnim * 6.0f);
        float resultFontSize = 46.0f;
        
        // Misura usando Comfortaa.ttf l'impatto a schermo generato (Per fare un Allineamento a Destra Perfetto).
        Vector2 exprSize = MeasureTextEx(customFont, displayBuffer, exprFontSize, 1);
        Vector2 resSize = MeasureTextEx(customFont, resultBuffer, resultFontSize, 1);
        
        // Punto di partenza (Centro verticale del nostro LCD finto)
        float exprCenterY = displayRec.y + displayRec.height/2.0f - exprSize.y/2.0f;
        
        // Interpolazione lineare per sollevare l'espressione quando esce il risultato e far spuntare dal bordo il testo in verde.
        float exprUpY = displayRec.y + 15.0f; // Dove andrà
        float resCenterY = displayRec.y + displayRec.height/2.0f - resSize.y/2.0f + 10.0f; // La destinazione finale
        float resDownY = displayRec.y + displayRec.height; // Da dove parte (Fuori dalla visuale del Box, ma non disegnata grazie a ScissorMode)
        
        // Valori finali interpolati per l'animazione Frame-per-Frame (Math_Lerp)
        float currentExprY = exprCenterY + (exprUpY - exprCenterY) * animSlideUp;
        float currentResY = resDownY + (resCenterY - resDownY) * animSlideUp;
        
        // Valori X che non cambiano mai, tenuti fissi verso Destra LCD
        float exprX = displayRec.x + displayRec.width - 15.0f - exprSize.x;
        float resX = displayRec.x + displayRec.width - 15.0f - resSize.x;
        
        Color exprColor = (IsSystemThemeDark() == 1) ? WHITE : BLACK;
        Color resColor = (IsSystemThemeDark() == 1) ? GREEN : DARKGREEN;
        // Dissolve text l'opacità per farlo svanire!
        exprColor.a = 255 - (unsigned char)(animSlideUp * 100);
        resColor.a = (unsigned char)(animSlideUp * 255); // Da 0 a 255 (Fade In del risultato)
        
        // Utilizziamo un clipping grafico Vulkan/Opengl (Scissor Mode), così il verde non trabocca fisicamente e 
        // non disegna lettere fuori dalla UI della calcolatrice quando non ancora spawnate
        BeginScissorMode((int)displayRec.x, (int)displayRec.y, (int)displayRec.width, (int)displayRec.height);
        DrawTextEx(customFont, displayBuffer, (Vector2){exprX, currentExprY}, exprFontSize, 1, exprColor); // Applica stringhe
        if (animSlideUp > 0.01f) {
            DrawTextEx(customFont, resultBuffer, (Vector2){resX, currentResY}, resultFontSize, 1, resColor);
        }
        EndScissorMode(); // Blocca l'imbuto di clipping
        
        for (int i=0; i<NUM_BUTTONS; i++) {
            if (buttons[i].text[0] != '\0') {
                // Passa ogni singolo bottone e lascia che Button.c ci disegni la goccia visiva con le speculari e caustiche che avevamo discusso
                DrawCalcButton(&buttons[i], mouse, customFont); 
            }
        }
        
        EndDrawing(); // Fine della pipeline di rendering e attesa VSync per il prossimo tick 144hz/60hz
    }
    
    // Dealloca Garbage per non affaticare Wayland
    ClearExpr(&exprList);
    CloseWindow();
    return 0; // Bye Bye
}
```
