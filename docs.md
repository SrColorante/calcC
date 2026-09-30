# calcC — Documentazione tecnica

Riferimento approfondito per **calcC**, calcolatrice desktop in C su raylib con
rendering SDF.

> **Snapshot:** i conteggi di righe e i riferimenti dell'appendice sono quelli
> verificati sul sorgente corrente (660 in `main.c`, 364 in `ui/renderer.c`, 266 in
> `ui/theme.c`, 169 in `ui/button.c`, 332 in `logic/eval.c`). `ui/renderer.c` e il
> thread del tema sono l'aggiunta di questo intervento.
>
> Per una panoramica vedi [`README.md`](./README.md).

---

## Sommario

1. [Che cos'è](#1-che-cosè)
2. [Stack e build](#2-stack-e-build)
3. [Architettura](#3-architettura)
4. [`logic/eval.c` — il motore](#4-logicevalc--il-motore)
5. [`ui/renderer.c` — il renderer SDF](#5-uirendererc--il-renderer-sdf)
6. [`ui/theme.c` — il tema](#6-uithemec--il-tema)
7. [`ui/button.c` — le animazioni](#7-uibuttonc--le-animazioni)
8. [`main.c` — stato, layout, input](#8-mainc--stato-layout-input)
9. [Il protocollo umano della calcolatrice](#9-il-protocollo-umano-della-calcolatrice)
10. [Test](#10-test)
11. [Benchmark](#11-benchmark)
12. [Bug, limiti e codice morto](#12-bug-limiti-e-codice-morto)
13. [Appendice](#13-appendice)

---

## 1. Che cos'è

Una calcolatrice scientifica **desktop**, non un gioco e non un generico
"evaluator". L'evidente fra le parole `eval`/`EvaluateExpr` nel codice si
riferisce alla valutazione di **espressioni matematiche**, non all'esecuzione di
programmi. Il nome "calcC" è semplicemente "Calculator in C".

Prova: `main.c:494` inizializza la finestra con il titolo `"Calculator"`, e
`config.h` imposta 420×640 — le dimensioni di una calcolatrice a tastiera. La
griglia `KEYS[5][4]` è la classica disposizione `C % / *` in testa, quattro righe
di cifre, `=` a doppia altezza e `0` a doppia larghezza.

L'obiettivo di design dichiarato nei commenti del codice è un'app che **si
adatta bene a un compositor a tiling** come niri: tutto il layout è proporzionale
alle dimensioni della finestra, e la finestra è trasparente con un inset di 1px
per far emergere l'ombra dal compositore.

---

## 2. Stack e build

### 2.1 Linguaggio

**C11**, compilato come `-std=gnu11` perché `CMAKE_C_EXTENSIONS` è ON di default.
Flag: `-Wall -Wextra`, **senza `-Werror`**. Nessun C++ nel progetto.

### 2.2 CMake

```cmake
cmake_minimum_required(VERSION 3.14)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)   # solo CMake 4.x
project(CalcC)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)  # se non impostata
```

> **`CMAKE_POLICY_VERSION_MINIMUM` è una variabile di CMake 4.x**, sconosciuta alla
> 3.x. È un workaround perché `FetchContent` deve poter consumare raylib 5.0, che
> dichiara `cmake_minimum_required(VERSION 3.0)`. L'effetto pratico è che il
> progetto è **CMake ≥ 4.0 soltanto**, nonostante la dichiarazione 3.14.

La cache mostra `CMAKE_PROJECT_VERSION:STATIC=3.4.0`, che è in realtà la versione
di **GLFW** che trapela, non quella di CalcC.

### 2.3 Opzioni di build

| Impostazione | Effetto |
|---|---|
| `CMAKE_C_FLAGS += -Wall -Wextra` | Appende, non sostituisce |
| `CMAKE_BUILD_TYPE` default `Release` | `-O3 -DNDEBUG` per l'app **e** per raylib |
| `USE_WAYLAND ON` (FORCE) | Propagato a `GLFW_USE_WAYLAND` |
| `USE_X11 OFF` (FORCE) | **Non ha effetto** — vedi §12.1 |
| `BUILD_EXAMPLES OFF` | Cache di raylib |
| `THREADS_PREFER_PTHREAD_FLAG` + `find_package(Threads)` | Per il thread del tema |
| `target_include_directories(calcC PRIVATE . logic ui)` | La vecchia voce `graphics/` puntava a una directory inesistente |
| `target_link_libraries(calcC PRIVATE raylib m Threads::Threads)` | `m` per `fmod`/`isnan` |
| `URL_HASH SHA256=98f049b9…` | Verifica i 32 MB scaricati; prima mancava del tutto |

### 2.4 Dipendenze

**raylib 5.0**, scaricato al configure via `FetchContent` e compilato come
`libraylib.a` statico, con gli oggetti GLFW incorporati.

> 🔴 **Prima non c'era nessun hash SHA256 sull'URL di download.** Ora
> `FetchContent_Declare` lo dichiara esplicitamente, quindi il configure rifiuta un
> archivio diverso da quello pubblicato per il tag `5.0`.

**GLFW 3.4.0** incluso in raylib, con supporto Wayland, X11, EGL, OSMesa e Null.
Definizioni di compilazione osservate: `-DGRAPHICS_API_OPENGL_33 -DPLATFORM_DESKTOP`.

Librerie di sistema risolte al configure: `libGL.so`, `libGLX.so`, `librt.a`,
`libfreetype.so`, `libfontconfig.so`, `libdl`, `libpthread`.

---

## 3. Architettura

```
                    ┌──────────────────────────────────────┐
                    │              main.c (637)           │
                    │  KEYS[5][4]  ← unica fonte di verità│
                    │  PressKey()   ← semantica           │
                    │  UpdateLayout() ← layout proporz.  │
                    │  PressKey / tastiera / mouse         │
                    └───┬───────────────────┬──────────┬───┘
                        │                   │          │
              ┌─────────▼──────┐  ┌─────────▼────┐  ┌──▼──────────┐
              │ logic/eval.c   │  │ ui/button.c  │  │ ui/theme.c  │
              │ 329            │  │ 169          │  │ 229         │
              │ token list     │  │ molla, hover │  │ palette     │
              │ pool 4096      │  │ ripple       │  │ thread      │
              │ shunting-yard  │  └──────┬───────┘  └──────┬──────┘
              └────────────────┘         │                 │
                                   ┌────▼─────────────────▼───┐
                                   │      ui/renderer.c       │
                                   │  2 shader GLSL 330       │
                                   │  + fallback CPU          │
                                   └──────────────────────────┘
```

| Modulo | Linee di confine |
|---|---|
| `logic/eval.c` | Solo aritmetica. Nessuna conoscenza di finestre, colori o input |
| `ui/theme.c` | Rilevamento e palette. Non sa nulla della calcolatrice |
| `ui/button.c` | Animazione e disegno di un singolo pulsante |
| `ui/renderer.c` | Primitive SDF pure. Non sa nulla dei pulsanti |
| `main.c` | Tutto il resto: stato, layout, semantica, input, main loop |

---

## 4. `logic/eval.c` — il motore

### 4.1 Rappresentazione: lista di token, non di numeri

```c
typedef enum { NODE_DIGIT, NODE_VALUE, NODE_OP } NodeType;

typedef struct ExprNode {
    NodeType type;
    double   val;      // cifra 0-9, oppure un double pronto, oppure il codice ASCII
    struct ExprNode* next;
} ExprNode;

typedef struct {
    ExprNode *head;
    ExprNode *tail;     // append O(1)
    int       count;    // controllo del limite O(1)
} Expr;
```

L'idea di progetto decisiva: un **numero non è un nodo**. `12.5` è cinque nodi
(`1`, `.`, `2`, `.`, `5`); i numeri vengono composti in un `double` al momento
della valutazione. `NODE_VALUE` è la valoche che permette a un `double` già
calcolato (il valore di `Ans`) di stare nello stream come singolo atomo.

`tail` e `count` esistono per una ragione precisa, documentata nell'header:
prima ogni `Append*` percorreva l'intera lista, rendendo la costruzione **O(n²)**.

### 4.2 Pool di nodi

```c
static ExprNode g_pool[NODE_POOL_SIZE];   // 4096
static ExprNode* g_freeList = NULL;
static int g_liveNodes = 0;
```

`PoolInit()` infila l'intero array statico in una free list collegata. `NodeAlloc()`
estrae la testa, `NodeFree()` la reinserisce. **O(1), nessuna `malloc`, nessuna
frammentazione, nessuna syscall.** È il sostituto deliberato del vecchio disegno
con una `malloc` per token.

**Dimensionamento:** `EXPR_MAX_NODES = 160` per espressione × 20 slot di
cronologia + 1 viva = 3216 massimo, contro un pool di 4096: circa 880 nodi di
margine. `eval_test.c` verifica esplicitamente 40 espressioni da 20 nodi
simultanee.

> **L'esaurimento tronca silenziosamente.** `PushNode` restituisce `NULL` e i
> chiamanti scartano il token, con il commento *"pool esaurito: ignora (nessun
> crash)"*. Il pool è un tetto rigido con troncamento silenzioso, non un errore.
> Con i valori attuali non è raggiungibile nell'uso normale.

### 4.3 Complessità per operazione

| Funzione | Complessità | Note |
|---|---|---|
| `ExprDigit` / `ExprValue` / `ExprOp` | **O(1)** | `tail` + `count` |
| `ExprCount` | **O(1)** | Era O(n) |
| `ExprToString` | **O(n)** | Cursore + `memcpy`; rimpiazzato `strcat` che era O(n²) |
| `EvaluateExpr` | **O(n)** | Shunting-yard, singola passata |
| `ExprClear` | O(n) | Dealloca ogni nodo |
| `ExprPop` | O(n) | Cammina fino al penultimo; lista non doppiamente collegata |
| `ExprCopy` | O(n) | Copia profonda in nodi freschi |
| `FormatNumber` | O(1) | `snprintf` + rimozione degli zeri |

`ExprPop` O(n) significa che un backspace fino a vuoto su un'espressione da 160
token costa O(n²) ≈ 12 800 passi. Irrilevante a questa scala.

### 4.4 `EvaluateExpr` — lo shunting-yard

Due array a dimensione fissa, entrambi `EXPR_MAX_NODES`:

```c
double stack[EXPR_MAX_NODES];   // 160
char   ops[EXPR_MAX_NODES];     // 160
```

Circa 1 440 byte di stack, con ogni push controllato nei limiti.

**Il ciclo principale** usa un cursore esplicito `ExprNode* c` invece di un `for`,
perché il ciclo interno di composizione dei numeri consuma più nodi:

1. **Composizione dei numeri** — consuma-avanzato una sequenza di nodi
   `NODE_DIGIT` e `'.'` in un `double`:
   ```c
   if (!inDec) val = val * 10.0 + c->val;
   else { mult *= 0.1; val += c->val * mult; }
   ```
   poi applica l'eventuale negazione unaria pendente, push, e prosegue.

2. **`NODE_VALUE`** — push diretto del `double`. È così che `Ans` entra nello stream.

3. **Operatori**:
   - Rilevamento del meno unario: `if (op == '-' && needOperand)` mette un flag
     invece di spingere `'-'` sullo stack degli operatori. Gestisce `-5`, `3*-2`
     e `5--3`.
   - `if (needOperand) { *error = true; }` cattura `5 * * 3`.
   - Drenaggio per precedenza: `while (opc > 0 && Prec(ops[opc-1]) >= Prec(op))`.
     Il **`>=` è ciò che dà l'associatività a sinistra**, ed è la ragione per cui
     `10-3-2 == 5` e `100/10/2 == 5` (entrambi testati).
   - `Prec`: `* / %` → 2, `+ -` → 1, altrimenti 0.

4. **Epilogo** — rifiuta l'operatore pendente (`5+`), rifiuta il `-` isolato,
   svuota lo stack, e richiede `sp == 1`.

`ApplyOp` restituisce `false` per divisione o modulo per zero e per operatori
sconosciuti: è **l'unico imbuto di errore** per `/0` e `%0`.

### 4.5 `FormatNumber`

Sostituto progettato per `%g`, che tronca a 6 cifre significative ed emette
`1e+06`.

- `isnan` → `"Error"`, `isinf` → `"Infinity"` / `"-Infinity"`.
- Nell'intervallo normale (`|v| == 0` oppure `1e-9 <= |v| < 1e12`): notazione fissa
  con un numero di decimali dipendente dalla grandezza — 6 decimali per `>= 100`,
  10 per `>= 1`, 12 per `>= 0.01`.
- Rimuove gli zeri finali, poi il punto pendente.
- Gestione **deliberatamente corretta** di `-0` → `0`, con un commento che spiega
  che la versione ingenua produrrebbe `"00"`.
- Fuori intervallo: `"%.6e"`.

I contratti esatti sono fissati dai test: `1/3 → "0.333333333333"`,
`1e15 → "1.000000e+15"`, `-0.0 → "0"`.

### 4.6 `ExprToString`

Singola passata con cursore, `memcpy` per token, restituisce i byte scritti. Un
comportamento notevole: **la soppressione dello zero iniziale** avviene solo se il
nodo successivo è a sua volta una cifra, così `05` diventa `5` ma `0.5` resta
`0.5`. Entrambi i casi sono testati.

---

## 5. `ui/renderer.c` — il renderer SDF

È il cambiamento architetturale più importante della storia recente del progetto.

### 5.1 Motivazione

Il commento d'intestazione di `renderer.h` è esplicito: prima ogni pulsante
costava 4 primitive (ombra, corpo, highlight, bordo), cioè circa **100 chiamate
di disegno per frame**, con l'anti-aliasing affidato interamente al MSAA della
finestra.

### 5.2 Due fragment shader su un vertex condiviso

- **`FS_PANEL_SRC`** — pannello arrotondato generico con 10 uniform `vec4`
  (`uP[10]`), che copre: copertura anti-aliased analitica
  (`clamp(0.5 - d, 0, 1)`), ombra esterna morbida (`exp(-sd/…)`), glow colorato,
  gradiente verticale del corpo, banda speculare superiore, griglia di punti,
  anello di ripple della pressione, bordo interno.
- **`FS_BG_SRC`** — sfondo dell'app: gradiente diagonale, glow dietro il display,
  vignettatura, bordo.

### 5.3 Il batching degli uniform

> "Per non passare 40 uniform per frame, tutti i parametri di una forma viaggiano
> in un unico array uniform vec4[10] caricato con una sola chiamata `glUniform4fv`."

```c
BeginShaderMode(g_panel);
SetShaderValue(g_panel, g_uFbPanel, &g_fb, SHADER_UNIFORM_VEC2);
SetShaderValueV(g_panel, g_uPanel, u, SHADER_UNIFORM_VEC4, 10);
DrawRectangleRec(quad, WHITE);
EndShaderMode();
```

Ogni forma costa quindi **2 chiamate GL** invece di 4 primitive. Il commento su
`EndShaderMode` ricorda che `rlSetShader` forza il flush con gli uniform corretti.

### 5.4 Degradazione pulita

`RendererInit` verifica `id > 0 && locs != NULL` per entrambi gli shader e che
tutte le locazioni degli uniform risolvano. Se qualcosa fallisce logga un
avviso, lascia `g_ok = false`, e `DrawPanel`/`DrawBackground` cadono su
`DrawPanelFallback`: rettangoli arrotondati piatti via raylib, senza ombra né
gradiente.

**L'app si avvia e resta accettabile anche senza alcun supporto shader.** È una
delle decisioni più mature del progetto.

### 5.5 Dettagli non ovvi

- **Y-flip**: gli shader convertono l'origine in basso a sinistra di GL in quella
  in alto a sinistra di raylib con
  `vec2 frag = vec2(gl_FragCoord.x, uFb.y - gl_FragCoord.y)`.
  `RendererSetFramebuffer` esce subito se il framebuffer non è cambiato, quindi è
  un no-op nella maggior parte dei frame.
- **Margine del quad**: viene espanso oltre la forma perché ombra e glow non
  vengano ritagliati.
- **Compositing `over()`** scritto a mano in GLSL con matematica premoltiplicata e
  una divisione, così gestisce correttamente l'impilamento di livelli traslucidi.

---

## 6. `ui/theme.c` — il tema

```c
typedef struct { Color top, bottom; ... } Style;
typedef struct { Style panel, display, button, ...; } Theme;
```

`DetectSystemThemeDark()` è **portabile su tre piattaforme**:

| Piattaforma | Meccanismo |
|---|---|
| Windows | Registry `HKCU\...\Themes\Personalize\AppsUseLightTheme` via `RegOpenKeyExA` |
| macOS | `defaults read -g AppleInterfaceStyle` |
| Linux | `gsettings get org.gnome.desktop.interface color-scheme` |

> **Il ramo Windows è codice morto su MSVC**, perché il file include
> `<pthread.h>` e usa `<stdatomic.h>` incondizionatamente e MSVC non ha nessuno
> dei due. Vedi §12.3.

Il **thread di polling** è la correzione di un bug documentato nel codice stesso:
prima `DetectSystemThemeDark()` veniva chiamata **19 volte per frame**, per
570 ms/frame. Oggi il loop di rendering non esegue **zero** interrogazioni; il
tema viene campionato in background.

---

## 7. `ui/button.c` — le animazioni

Tre segnali indipendenti, tutti indipendenti dal framerate:

| Segnale | Modello | Parametri |
|---|---|---|
| `press` | **Molla smorzata**, Eulero semi-implicito | `k = 320.0f`, `c = 26.0f`, clamp `[0, 1.3]`, `dt` clampato a `[0, 0.05]` perché un frame lungo non la faccia esplodere |
| `hover` | Inseguimento esponenziale | `1 - exp(-dt*14)` |
| `ripple` | Avanzamento lineare | `dt*2.6` |

Il clamp di `dt` è un dettaglio di robustezza deliberato: senza, una pausa lunga
o un frame bloccato porterebbero il sistema a divergere.

**Lato render:**

```c
scale  = 1 - 0.055*p;              // il pulsante affonda
lift   = hover*1.6;                // il pulsante si solleva
bright = hover*0.12 - p*0.10;      // hover schiarisce, pressione scurisce
```

Tutto confluisce in un unico `Panel`, quindi un solo disegno shader per pulsante.

`FlashButton` (in `main.c`) fa lookup per etichetta nella tabella `KEYS`, il che
rinforza la fonte di verità unica.

---

## 8. `main.c` — stato, layout, input

### 8.1 Il loop

1. `BeginDrawing()` / `ClearBackground(BLANK)` — `BLANK` è completamente
   trasparente, e la finestra ha `FLAG_WINDOW_TRANSPARENT`, così il compositore
   mostra attraverso l'inset di 1px
2. `RendererSetFramebuffer(...)`
3. `DrawBackground(&bg)` — **una sola chiamata** per l'intero pannello dell'app
4. `DrawDisplay(font, t)` — pannello del display, poi testo in scissor mode
5. `DrawCalcButton(...)` per ogni pulsante
6. Overlay FPS opzionale (F3)
7. `EndDrawing()`

Flag di finestra: sempre `FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_TRANSPARENT`, più
`FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT` in uso normale (vsync disattinato e MSAA
condizionale in benchmark).

**Lo scissor mode** racchiude il testo del display così che il risultato che
scorre dal basso venga ritagliato invece di debordare.

### 8.2 Layout proporzionale

`UpdateLayout(sw, sh)` viene chiamato ogni frame ma **esce subito se le
dimensioni non sono cambiate**:

```c
if (sw == cachedW && sh == cachedH) return;
```

Questa è una correzione rispetto alla versione precedente, che ricalcolava tutto
ogni frame. Quando gira, tutto è **puramente proporzionale**, senza un solo
posizionamento in pixel fisso tranne l'inset di 1px:

- `side = min(sw, sh)`, `gap = side * GRID_GAP` (0.022)
- `availW = sw * (1 - 2*GRID_MARGIN_X)` (0.055)
- `displayRec` a `DISPLAY_TOP` (0.055), alto `DISPLAY_HEIGHT` (0.245)
- `cellW = (availW - 3*gap)/4`, `cellH = (availH - 4*gap)/5`,
  **`size = min(cellW, cellH)`** — è questo il passaggio che mantiene i pulsanti
  **quadrati**, così la pillola con `ROUND_BUTTON = 1.0` resta un cerchio
- La griglia è **centrata** su entrambi gli assi, così sopravvive a qualsiasi
  aspect ratio di tiling

`ResetButton()` ricostruisce tutto lo stato animativo al ridimensionamento,
inizializzando `rippleAt` al centro di ogni pulsante.

### 8.3 Adattamento del testo

`FitFontSize` usa **ricerca binaria** (7 chiamate a `MeasureTextEx`) invece
della vecchia scansione lineare da 1px (~30 chiamate), ed è avvolto da `FitCached`
che memoizza su `(text, maxW)`, rendendolo di fatto gratuito a regime.

### 8.4 Input

**Mouse** — per ogni pulsante, `CheckCollisionPointRec` imposta `hovered`; se
hovered e premuto e il ripple precedente è finito, ri-trigchera
`FlashButtonPress(b, mouse)` così che **il ripple origini dal punto esatto della
pressione**; al rilascio chiama `PressKey`.

**Tastiera** — due canali indipendenti:

- `GetKeyPressed()` in un `while`: `KEY_C`/`KEY_DELETE` → cancella,
  `KEY_BACKSPACE` → backspace, `KEY_EQUAL`/`KEY_KP_EQUAL`/`KEY_ENTER`/`KEY_KP_ENTER`
  → valuta, `KEY_UP`/`KEY_DOWN` → cronologia, `KEY_F3` → FPS
- `IsKeyPressedRepeat(KEY_BACKSPACE)` — auto-repeat, **solo** per il backspace
- `GetCharPressed()` in un `while`: cifre, `+-*/%`, `.`/`,` (virgola decimale
  italiana), `=`/`\n`/`\r`

---

## 9. Il protocollo umano della calcolatrice

Questa sezione merita attenzione perché **non è testabile** e **non è nel
modulo `logic/`**: le regole di comportamento dell'utente sono intrecciate al
ciclo di rendering.

`PressKey` (in `main.c`) implementa:

| Comportamento | Regola |
|---|---|
| `=` | `ShowResult()` |
| `C` | `ClearAll()` |
| **Dopo un risultato, un operatore** | continua da `Ans`: `ExprClear` → `ExprValue(lastResult)` → `ExprOp` |
| **Dopo un risultato, qualsiasi altra cosa** | ricomincia da capo |
| `.` subito dopo un operatore | inietta uno `0` iniziale (`5+` poi `.` → `5+0.`) |
| Due operatori di fila | il primo viene rimosso (`5++3` → `5+3`) |
| Backspace da stato risultato | cancella invece di editare |

> **La principale critica architetturale del progetto.** `PressKey`,
> `ShowResult`, `ClearAll` e `LoadHistory` stanno tutti in `main.c` e non sono
> raggiungibili da `eval_test.c` senza aprire una finestra. Un `logic/keys.c` che
> espone `PressKey` come macchina a stati pura sarebbe direttamente testabile, e
> l'harness esistente lo coprirebbe senza modifiche. È la continuazione
> dell'**Ans** a essere il comportamento più soggetto a bug, ed è proprio quello
> che non ha test.

---

## 10. Test

`eval_test.c`, 146 righe, **a mano**: nessun framework, nessun CTest, nessuna CI.
**Non è in nessun target CMake** — va compilato a mano.

### 10.1 Copertura

| Sezione | Contenuto |
|---|---|
| Aritmetica | 13 casi: operazioni base, **precedenza** (`2+3*4`→14), **associatività a sinistra** (`10-3-2`→5, `100/10/2`→5), modulo, decimali, rumore floating (`0.1+0.2`) |
| Numeri negativi e unario | 5 casi: `-5`, `-5+10`, `3*-2`, `5--3`, `-2*-3` — esercita il percorso `negateNext` |
| Errori di sintassi | 8 casi: `1/0`, `5%0`, `5+`, `5*`, `*5`, `5**3`, `-`, `5+*3` |
| Casi limite / overflow | 300 cifre in un tetto di 160; 100 000 cifre per la troncatura silenziosa |
| Formattazione | 10 contratti di stringa esatta, incluso `-0.0 → "0"` |
| Visualizzazione | 4 casi di `ExprToString`, inclusa la soppressione dello zero iniziale |
| Pool | 200 cicli crea+distruggi; 40 espressioni lunghe simultanee |

### 10.2 Igiene dei test

Due problemi, entrambi onesti ma non bloccanti:

1. **Il contatore di successi è in parte decorativo.** `eval_test.c:93` fa
   `g_pass += 2` per il caso da 300 cifre **senza asserire nulla** — stampa e
   basta. Lo stesso a riga 101, 132 e 140.
2. **Il messaggio del caso da 300 cifre è fuorviante.** `ExprDigit` è limitato a
   `EXPR_MAX_NODES = 160`, quindi vengono creati solo 160 nodi: il test verifica
   **il tetto**, non un overflow da 300 nodi — che è il punto, ma la stringa
   `"300 cifre -> %zu caratteri"` riporta 159 e suggerisce il contrario.

### 10.3 Cosa non è testato

`PressKey`, `ShowResult`, `ClearAll`, `LoadHistory` — tutti in `main.c`,
irraggiungibili senza finestra. E con essi **la logica di continuazione dell'Ans**,
che è il comportamento più delicato del progetto. Inoltre: gli shader e il
fallback CPU, la matematica delle animazioni, `FitFontSize`/`FitCached`, il
rilevamento del tema e il suo thread, la matematica di `UpdateLayout`, `shoot.sh`.

### 10.4 Come eseguirli

```bash
gcc -O2 -o /tmp/eval_test eval_test.c logic/eval.c -lm && /tmp/eval_test
```

Exit 0 = tutto passa, 1 = fallimenti. **È l'unico comando di build testato che
funzioni**, ed è anche l'unico documentato in qualsiasi file del repository.

> **Miglioramento a basso costo e alto valore:** aggiungere
> `enable_testing()` + `add_executable(eval_test …)` + `add_test(…)` al
> `CMakeLists.txt` per far funzionare `ctest`.

---

## 11. Benchmark

### 11.1 `bench_eval.c` (176 righe)

Confronto a parità di condizioni. Contiene una **copia verbatim della vecchia
implementazione** (`ONode`/`OAppendOp`/`OEval`) accanto all'`eval.h` reale, così
il confronto è possibile senza dipendere dalla cronologia git. Il carico è
un'espressione fissa da 44 token:

```c
static const char* SAMPLE = "123456789*987654321+42.5/3-17%5+88*2-31.25+6";
```

Due fasi: confronto vecchio/nuovo a `N = 20000`, e uno **sweep di scalabilità** a
lunghezze 10/20/30/40 con `it = 5000`, riportando µs per ciclo e rapporto di
speedup.

> **Una riserva sullo sweep:** il generatore di espressioni costruisce
> `big[i] = digit; big[i+1] = '+'`, che **sovrascrive** — le stringhe risultanti
> sono sequenze del tipo `1+2+3+4+…` e non hanno i conteggi di token dichiarati.
> `sink` impedisce al compilatore di eliminare il lavoro. Nessun flag `-O` è
> fissato: lo fornisci tu.

### 11.2 `bench_baseline.c` (47 righe)

Sonda molto più stretta: cronometra 100 chiamate `popen("gsettings …")` ed
estrae una proiezione, modellando 19 interrogazioni per frame. Contiene una sua
copia di `IsSystemThemeDark_linux()` perché non può linkare quella reale — il file
deve restare privo di raylib. Esegue anche un ciclo booleano da 1e6 iterazioni
come controllo "un valore in cache è gratuito".

> **Questo benchmark documenta un bug già corretto due volte.** Modella 19
> interrogazioni per frame, che è esattamente ciò che il codice faceva prima e che
> il thread di polling ha eliminato. **Il file è ora puramente storico.**

### 11.3 Benchmark in-app

`CALCC_BENCH=<n>` è quello reale. Distingue il tempo di frame wall-clock dalla
**sola porzione di submission CPU**: `cpuT0` è preso subito prima di
`BeginDrawing()` e `cpu` misurato subito prima di `EndDrawing()`, escludendo
apposta l'attesa del vsync. I primi 30 frame sono scartati come warm-up.
In benchmark il vsync viene spento (altrimenti misurerei il monitor) ma
tutti gli altri flag restano quelli della build normale, MSAA compreso.

```
$ CALCC_BENCH=800 ./build/calcC
== CALCC bench: 800 frame in 419.1 ms ==
   frame completo :   0.502 ms  (1990.9 fps max)
   registrazione  :   0.037 ms media |   0.078 ms picco  (770 frame)
   -> budget frame 60 fps: 16.67 ms | 144 fps: 6.94 ms
```

**Confronto con la versione a primitive vettoriali**, stessa macchina, stessa
identica configurazione di finestra:

| | Frame completo | Registrazione CPU |
|---|---|---|
| 4 primitive per tasto, `-O0` | 0.698 ms | **0.528 ms** media, 0.682 picco |
| 1 draw call per forma, `-O3` | 0.502 ms | **0.037 ms** media, 0.078 picco |

La registrazione CPU è **~14× più bassa**. Il tempo di frame completo scende
meno perché a quel punto non è più il limite: sotto i 0.5 ms domina la
presentazione della finestra trasparente a Wayland, non il nostro lavoro.

Su un run lungo di 40 000 frame (28 s, quindi attraversa due cicli di
aggiornamento del tema) il picco di registrazione resta a 2.96 ms: **non ci
sono più stall**, laddove il vecchio `popen` ne iniettava uno da 4.8 ms ogni
10 secondi.

### 11.4 Variabili d'ambiente

Rilevate una volta sola all'avvio con `getenv`, in 4 righe di `main.c`. Nessuna
attiva un comportamento di default: sono strumenti di misura e di collaudo.

| Variabile | Effetto |
|---|---|
| `CALCC_BENCH=<n>` | Gira `n` frame senza vsync e stampa le statistiche, poi esce |
| `CALCC_BENCH_NOMSA=1` | Nel benchmark, toglie anche MSAA (per isolarne il costo) |
| `CALCC_SHOT=<nome.png>` | Salva uno screenshot della finestra al frame indicato |
| `CALCC_SHOT_FRAME=<n>` | Frame dello screenshot (default 24) |
| `CALCC_SHOT_KEYS="12.5*8="` | Sequenza di tasti da digitare prima dello screenshot |
| `CALCC_SIZE=700x1000` | Apre la finestra a quelle dimensioni, per collaudare il layout |
| `CALCC_THEME=light\|dark` | **Forza il tema**, disabilitando il rilevamento |
| `CALCC_NOSHADER=1` | Disattiva lo shader e collauda il fallback su primitive |

> **`TakeScreenshot` ignora i percorsi.** Raylib prende solo il nome del file e lo
> scrive nel *base path* della finestra: `CALCC_SHOT=/tmp/x.png` finisce nella
> directory di lavoro. Per questo `shoot.sh` avvia l'app con `env -C` sulla
> directory di destinazione.

---

## 12. Bug, limiti e codice morto

### 12.1 Le impostazioni del build non fanno quello che dicono

🔴 **`USE_X11 OFF` non disabilita X11.** raylib non propaga quel flag a
`GLFW_BUILD_X11`: la cache conferma `GLFW_BUILD_X11:BOOL=ON` **e**
`GLFW_USE_WAYLAND:BOOL=ON`, e `link.txt` contiene `x11_init.c.o`, `glx_context.c.o`
**insieme a** `wl_init.c.o`. **Entrambi i backend sono compilati** in
`libraylib.a`, e GLFW fa `dlopen` di `libX11.so.6` o `libwayland-client.so.0` e
sceglie a runtime.

Il commento nel `CMakeLists.txt` — *"Forziamo Raylib a compilarsi SOLO per Wayland
ignorando X11"* — **sovradichiara quello che il codice fa.** Comportamentalmente
è innocuo, perché Wayland è preferito, ma la dichiarazione è falsa. *Il commento
è stato riscritto per dire quello che accade davvero.*

### 12.2 🔴 Nessun hash sull'URL di raylib

`FetchContent_Declare(raylib URL …/5.0.tar.gz)` senza `URL_HASH`. L'archivio è
pinnato per tag ma non verificato, e la build richiede rete al primo configure.

### 12.3 La build Windows ora compila

`ui/theme.c` includeva `<pthread.h>` e usava `<stdatomic.h>` incondizionatamente,
e MSVC non ha nessuno dei due: il ramo di registro Windows era **codice morto su
MSVC**, e la versione precedente, a thread singolo, compilava.

Ora il file è protetto da `CALCC_HAS_THREAD`, che vale 0 su Windows: lì `ThemePollSystem`
rilegge il registro in linea nel main loop con lo stesso intervallo di 10 s. La scelta è
lecita perché su Windows la lettura del registro costa pochi microsecondi, non i 5 ms di
un `popen` — il thread serve a non stallare il frame, e su Windows non c'è niente da
stallare.

### 12.4 Bug reali e spigoli acuti

| # | Problema | Posizione | Stato |
|---|---|---|---|
| 1 | **Il rilascio del mouse attivava un tasto anche se il press era iniziato altrove**: `if (b->hovered && rel) PressKey(...)` senza tracciare dove fosse iniziato. Premi A, trascina su B, rilascia → partiva B | `main.c` | **Corretto**: `pressIndex` ricorda il tasto di partenza, azzerato anche da `UpdateLayout` |
| 2 | **`EvaluateExpr` alloca due array da 160 elementi incondizionatamente** — 1,4 KB di stack per chiamata | `eval.c` | Innocuo qui; sarebbe un problema su uno stack piccolo. Non va in overflow: il test esercita 100 000 token in sicurezza |
| 3 | **L'esaurimento del pool tronca in silenzio** | `eval.c` | Un'espressione troncata è peggio di un errore visibile se le costanti cambiassero |
| 4 | **`FormatNumber` con `cap == 1`**: `end = out + strlen(out) - 1` puntava **prima** del buffer | `eval.c` | **Corretto**: `strlen` viene controllata prima di costruire il puntatore |
| 5 | **`ExprPop` è O(n)** | `eval.c` | Accettabile a n ≤ 160 |
| 6 | `target_include_directories` puntava a `graphics/`, inesistente | `CMakeLists.txt` | **Corretto**: voce rimossa |
| 7 | `frameNo == shotFrame` verificato *dopo* `frameNo++`, quindi off by one | `main.c` | **Corretto**: `frameNo + 1 == shotFrame` |

### 12.5 Limiti progettuali

- ~~**Nessuna dimensione minima di finestra.**~~ *Risolto: `SetWindowMinSize(240, 340)`.*
- **Nessuna parentesi**, nessun `√`, nessun `1/x`, nessun `±`, nessun `M+`/`MR`,
  nessuna copia/incolla dagli appunti (`GetClipboardText`/`SetClipboardText` sono
  un adattamento naturale e sono assenti), nessuna notazione scientifica in
  ingresso — per cui `1e10` è irraggiungibile da tastiera, nonostante
  `FormatNumber` ne supporti pienamente l'output.
- **Nessuna accessibilità**: nessun equivalente ARIA, nessuna navigazione da
  tastiera oltre i tasti fisici, nessuna garanzia di contrasto elevato.
- **L'errore lascia intatta l'espressione** e scrive `Errore` sul display in **rosso**,
  con una **scossa orizzontale smorzata** del riquadro e senza animazione di
  ingresso. Resta però **nessuna indicazione di *dove* sia l'errore**: `1/0` e `5%0`
  sono indistinguibili.
- **Localizzazione solo italiana e hardcoded**: la stringa `Errore` e tutti i
  commenti. Nessun layer i18n.
- **`shoot.sh` ora parte da `CALCC_SHOT`** e usa `grim` solo come ripiego. I due
  meccanismi **non sono equivalenti**: il primo cattura il solo framebuffer GL della
  finestra (niente chrome del compositore, niente altre finestre), il secondo
  cattura l'intero schermo.
- **`shoot.sh` è specifico di niri** e non funziona su altri compositor né su X11.

### 12.6 Codice morto

| Simbolo | Stato |
|---|---|
| `ThemeIsDark()` (`theme.c`) | Definita e **mai chiamata** — `ThemeGet()->dark` la rende ridondante |
| `g_liveNodes` (`eval.c`) | Incrementato e decrementato, **mai letto** — un contatore di debug o un'asserzione di leak mai collegata |
| `bench_baseline.c` | Documenta un bug inesistente; nessun target, nessuna doc |
| `bench_eval.c` | Nessun target né script — va compilato a mano come `eval_test.c` |

### 12.7 Igiene del repository

Nessuna CI, nessun `.github/`, nessuna `LICENSE`, nessun `.clang-format`, nessun
target `install()` nel CMake. `git log` mostra 12 commit di un solo autore; il
working tree è avanti rispetto a `master` con lavoro non committato.

> ~~**La `build/` presente è obsoleta**~~ *Risolto: riconfigurata con
> `CMAKE_BUILD_TYPE=Release`, ora con `Threads` e `ui/renderer.c`.*

---

## 13. Appendice

### 13.1 Riferimenti di codice

| Simbolo | Percorso |
|---|---|
| Tabella dei tasti | `main.c:14` |
| `PressKey` (semantica) | `main.c:209` |
| `UpdateLayout` (layout) | `main.c:86` |
| Cache del layout | `main.c:87` |
| `ResetButton` | `main.c:73` |
| `FitFontSize` / `FitCached` | `main.c:312` / `335` |
| Input mouse (`pressIndex`) | `main.c:556-570` |
| Input tastiera | `main.c:268-313` |
| Loop di rendering | `main.c:583-626` |
| Caricamento font | `main.c:520` |
| `ExprNode` / `NodeType` | `logic/eval.h:16-26` |
| Pool di nodi | `logic/eval.c:13-39` |
| `EvaluateExpr` | `logic/eval.c:248` |
| `ApplyOp` | `logic/eval.c:231` |
| `FormatNumber` | `logic/eval.c:135` |
| `ExprToString` | `logic/eval.c:176` |
| Shader pannello | `ui/renderer.c:34-129` |
| Shader sfondo | `ui/renderer.c:131-197` |
| `RendererInit` | `ui/renderer.c:208` |
| `DrawPanelFallback` | `ui/renderer.c:273` |
| Batch degli uniform | `ui/renderer.c:11-16` |
| `DetectSystemThemeDark` | `ui/theme.c` (Win `:20`, macOS `:33`, Linux `:42`) |
| `ThemeWorker` | `ui/theme.c:70-88` |
| Animazioni pulsante | `ui/button.c:27-62` |
| Layout del pulsante | `ui/button.c:112-169` |
| Costanti di layout | `config.h` |
| FetchContent raylib | `CMakeLists.txt` (riga 18-20) |

### 13.2 Glossario

| Termine | Significato in questo progetto |
|---|---|
| **raylib** | Libreria di grafica immediate-mode basata su OpenGL |
| **Immediate mode** | Lo stato dell'interfaccia vive in variabili e si ridisegna da capo ogni frame, senza scene graph |
| **SDF** | *Signed Distance Field*: funzione che dà la distanza con segno da una forma, usata qui per i bordi arrotondati, le ombre e i glow |
| **Copertura anti-aliased** | In uno shader SDF, `clamp(0.5 - d, 0, 1)` stima quanta parte di un pixel è dentro la forma |
| **MSAA** | *Multi-Sample Anti-Aliasing*, attivo a livello di finestra |
| **Molla smorzata** | Sistema `ẍ = -k·x - c·ẋ`, che converge a zero con oscillazione; qui `k=320`, `c=26` |
| **Shunting-yard** | Algoritmo di valutazione di espressioni che usa due stack ed evita la ricorsione |
| **Associatività a sinistra** | `10-3-2` vale `(10-3)-2`; ottenuta qui con `>=` nel confronto delle precedenze |
| **Free list** | Lista di blocchi liberati, per riallocare senza chiamare il sistema |
| **msaa** / **vsync** | Vedi sopra; il vsync sincronizza il frame con il refresh del monitor |
| **Frustum culling / scissor mode** | Ritaglio del disegno a un rettangolo, usato per il testo del display |
| **niri** | Compositor Wayland a tiling, target esplicito di `shoot.sh` |
| **grim** | Strumento di cattura schermo del compositor Wayland |
| **popen** | Chiamata di sistema C che apre una pipe verso un processo shell — il meccanismo di `DetectSystemThemeDark` su Linux |

### 13.3 Cosa **non** è determinabile dal codice

- Se il vincolo di versione di CMake ≥ 4 sia intenzionale o un effetto collaterale
  del consumo di raylib via `FetchContent`.
- Perché `%` sia scelto al posto delle parentesi come quarto operatore della riga
  superiore: il README precedente lo dava per scontato, nessun commento nel codice
  lo motiva.
- Se l'assenza di `SetWindowMinSize` sia un oversight o una scelta (per esempio per
  permettere il tiling di finestre minuscole).
- Le versioni esatte delle librerie di sistema risolte: nessun file le blocca.

### 13.4 Metodo

Lettura diretta di `CMakeLists.txt`, `config.h`, `main.c`, `logic/eval.{h,c}`,
`ui/{theme,button,renderer}.{h,c}`, `eval_test.c`, `bench_eval.c`,
`bench_baseline.c` e `shoot.sh`; conteggio righe; lettura della cache CMake e del
`link.txt` generato per verificare quali backend GLFW siano realmente compilati;
confronto tra le affermazioni del precedente README e `docs.md` e il sorgente
corrente. Le osservazioni sui riferimenti di riga sono state ricontrollate
immediatamente prima della stesura, dato che l'albero è stato modificato durante
l'indagine.
