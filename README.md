# Documentazione Progetto: Calcolatrice C / Raylib (Wayland)

Questo documento spiega nel dettaglio ogni componente e singola logica della codebase della calcolatrice.

## Struttura del Progetto
Il progetto è diviso in tre moduli principali per garantire un'architettura pulita e scalabile:
- `main.c`: Il cuore dell'applicazione, gestisce il loop di rendering, gli input della tastiera e del mouse, e le animazioni.
- `logic/eval.c` e `logic/eval.h`: Il motore di parsing e valutazione matematica basato su una Lista Concatenata (Linked List).
- `ui/button.c` e `ui/button.h`: Il sistema grafico responsabile del rendering estetico (design a goccia d'acqua) dei bottoni.

---

## 1. Il Motore Matematico (`logic/eval.c`)
Invece di utilizzare un semplice array di stringhe, la calcolatrice usa una **lista concatenata dinamica**.
Ogni nodo (`ExprNode`) rappresenta un numero o un operatore.

```c
typedef struct ExprNode {
    NodeType type; // NODE_NUM o NODE_OP
    union {
        int n;
        char op;
    };
    struct ExprNode* next;
} ExprNode;
```

### Come viene calcolata l'espressione (`EvaluateExpr`)
Quando l'utente preme `=`, viene invocata la funzione `EvaluateExpr(ExprNode* head, bool* error)`.
1. **Compattazione:** La lista concatenata (che magari contiene i nodi `1`, `.`, `5`) viene percorsa e trasformata in un vero e proprio numero a virgola mobile (`1.5`). I numeri e gli operatori vengono separati in due array paralleli (`vals` e `ops`).
2. **Precedenze (BODMAS):** Il calcolo avviene per fasi.
   - Prima fase: scansiona l'array cercando `s` (radici quadrate) e le risolve.
   - Seconda fase: scansiona l'array per moltiplicazioni `*` e divisioni `/`. Quando ne trova una, unisce `vals[i]` e `vals[i+1]` ed "elimina" l'operatore collassando l'array.
   - Terza fase: risolve addizioni `+` e sottrazioni `-`.
3. Il risultato finale è semplicemente l'ultimo elemento rimasto in `vals[0]`.

### La Logica di Cronologia e `Ans`
La calcolatrice salva ogni risultato in una cronologia `history[20]`. 
Se l'utente preme un operatore (es. `+`) subito dopo un risultato, la funzione `HandleAnsLogic` inietta il risultato precedente convertendolo in stringa carattere per carattere dentro alla lista concatenata, e poi accoda il nuovo operatore. 
Se il numero precedente era negativo, viene prepeso uno `0` (es. `0 - 5`) per aggirare in modo elegante la mancanza nativa di supporto ai numeri negativi unari nel parser!

---

## 2. L'Interfaccia Utente (`main.c`)
Nel file `main.c` viene gestita la finestra Raylib e il Game Loop (che gira sincronizzato al framerate del tuo monitor tramite `FLAG_VSYNC_HINT`).

### Layout Dinamico (`UpdateLayout`)
Per supportare in modo nativo Window Manager come Niri (Wayland) che alterano dinamicamente le proporzioni delle finestre (Tiling), il layout viene ricalcolato ad ogni singolo frame.
```c
float sw = GetScreenWidth();
float sh = GetScreenHeight();
```
Basandosi sulla risoluzione reale passata dal compositor, la funzione mappa la griglia di bottoni calcolando larghezza, altezza e margini per fare in modo che i click del mouse combacino *sempre* con la forma visiva, ovunque Niri posizioni la finestra!

### Il Timer di Pressione Visiva (`visual_press_timer`)
Per supportare i tasti "fisici" della tastiera (come `9`, `+`, `Invio`) senza dipendere da strani hook di sistema, `main.c` legge l'input testuale via `GetCharPressed()`. Quando rileva un carattere valido, chiama `TriggerButtonVisual("9")`.
Questo imposta il timer del bottone a `0.15f` secondi. Nel Game Loop, questo timer diminuisce (`-= GetFrameTime()`) creando un'animazione fluida di pressione anche senza l'uso del mouse!

### Animazioni Testuali (Pop e Slide)
```c
float exprFontSize = 34.0f + (popAnim * 6.0f);
```
Quando si digita un numero, `popAnim` schizza a `1.0` e decade lentamente a 0. Questo viene moltiplicato per `6.0f` pixel e aggiunto alla grandezza del font, creando un "rimbalzo" del testo.
Allo stesso modo, `animSlideUp` sposta le coordinate `Y` (verticali) del testo quando si preme Invio, spingendo l'espressione in alto e facendo apparire il risultato dal basso (`currentResY = resDownY + ... * animSlideUp`).

---

## 3. Il Rendering a Goccia d'Acqua (`ui/button.c`)
Ogni bottone viene renderizzato utilizzando `DrawCalcButton`.
La richiesta di design era "gocce d'acqua sopra un piano". Dato che Raylib non supporta shader per singolo widget facilmente, l'effetto liquid-glass è stato riprodotto stratificando diverse forme geometriche arrotondate con il massimo smusso possibile (`1.0f` che trasforma il quadrato in un cerchio perfetto):

1. **Ombra Esterna (Drop Shadow):** Viene disegnata un'ellisse nera semitrasparente sfalsata in basso a destra (`+3, +4`) per simulare l'elevazione.
2. **Corpo Traslucido:** Viene disegnata la goccia vera e propria con un colore solido ma un `Alpha` basso (`180-230`), per permettere allo sfondo dell'app di trasparire leggermente e simulare la densità liquida.
3. **Punto Luce (Specular Highlight):** Per farla sembrare una superficie bagnata, viene disegnata un'ellisse bianca e stretta in alto a sinistra. Questo simula il riflesso della luce ambientale sulla curvatura della goccia.
4. **Riflesso Interno:** In basso a destra viene disegnata un'altra piccola forma bianca molto tenue per simulare la luce che attraversa il liquido e illumina la base della goccia (caustica).

Tutto questo si aggiorna a 60+ FPS rendendo l'esperienza fluida e ultra-responsiva su ecosistemi Wayland.
