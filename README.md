# calcC

> Progetto in mostra su **[cristianrenosto.party/projects](https://cristianrenosto.party/projects)**

Calcolatrice desktop in **C**, costruita su [raylib](https://www.raylib.com/) con
rendering SDF in GLSL e window manager **Wayland**.

Finestra trasparente ridimensionabile, layout completamente proporzionale,
tema chiaro/scuro rilevato dal sistema operativo, animazioni a molla sui pulsanti.

```
   ┌─────────────────────────┐
   │          1 + 2 =        │   ← display con scroll del risultato
   │─────────────────────────│
   │  C    %    /    *       │
   │  7    8    9    −       │
   │  4    5    6    +       │
   │  1    2    3    ┌────┐ │
   │  0 ──────  .   │  = │ │   ← '0' a doppia colonna, '=' a doppia riga
   └─────────────────────────┘
```

---

## Requisiti

- **CMake ≥ 4.0** (non 3.x — vedi la nota sul `CMAKE_POLICY_VERSION_MINIMUM`)
- Un **sessione Wayland** attiva
- Toolchain C11, pthread
- Accesso a rete **solo al primo configure** (raylib viene scaricato)

raylib 5.0 è scaricato automaticamente via `FetchContent`; GLFW 3.4 è incluso in
raylib. Non c'è un `apt install` da documentare oltre alle librerie di sistema
risolte automaticamente (`libGL`, `libGLX`, freetype, fontconfig).

## Build

```bash
cmake -S . -B build
cmake --build build -j
./build/calcC
```

> **Esegui dalla radice del progetto.** Il font viene caricato con percorso
> relativo (`assets/Comfortaa.ttf`); avviato altrove ricade silenziosamente sul
> font predefinito di raylib, senza messaggio di errore.

`CMAKE_BUILD_TYPE` ha il default `Release` (`-O3 -DNDEBUG`) quando non è
impostato. Se la directory `build/` è preesistente e ha una cache vuota, **rimuovila
e riconfigura**, altrimenti compili a `-O0`.

## Test

L'unico file di test è `eval_test.c` (146 righe), che copre il motore aritmetico
**senza aprire alcuna finestra**:

```bash
gcc -O2 -o /tmp/eval_test eval_test.c logic/eval.c -lm && /tmp/eval_test
```

Exit code 0 se tutto passa, 1 altrimenti. Non è integrato in CMake: non esistono
`enable_testing()` né `add_test()`, quindi `ctest` non funziona.

Copre: aritmetica e precedenza, associatività a sinistra, numeri negativi e unary
minus, 8 errori di sintassi, casi limite di overflow, 10 contratti esatti di
formattazione, visualizzazione, e assenza di leak del pool di nodi.

## Benchmark

```bash
# motore di valutazione: implementazione vecchia (malloc + O(n²)) vs nuova
gcc -O2 -I. -o /tmp/bench_eval bench_eval.c logic/eval.c -lm && /tmp/bench_eval

# costo di DetectSystemThemeDark() rispetto al budget di frame
gcc -O2 -o /tmp/bench_baseline bench_baseline.c && /tmp/bench_baseline
```

`bench_eval` contiene una **copia verbatim del vecchio valutatore** come
riferimento di confronto, e misura sia a parità di workload sia con uno sweep di
scalabilità a lunghezze crescenti.

In-app, tramite variabili d'ambiente lette una volta all'avvio:

```bash
CALCC_BENCH=600 ./build/calcC                        # statistiche di frame-time
CALCC_BENCH=600 CALCC_BENCH_NOMSA=1 ./build/calcC    # anche senza MSAA
CALCC_SHOT=s.png CALCC_SHOT_FRAME=24 ./build/calcC    # screenshot della finestra
CALCC_SHOT=s.png CALCC_SHOT_KEYS="12+3*" ./build/calcC
CALCC_SIZE=700x1000 ./build/calcC                    # collauda il layout ridimensionato
CALCC_THEME=light ./build/calcC                      # forza il tema
CALCC_NOSHADER=1 ./build/calcC                       # collauda il fallback senza shader
```

`CALCC_BENCH` disattiva il vsync, esegue N frame e distingue il tempo di frame
wall-clock dalla sola porzione di submission CPU. I primi 30 frame sono scartati
come warm-up.

> `TakeScreenshot` in raylib **ignora il percorso**: prende solo il nome del file e
> lo scrive nella directory di lavoro. Per questo `shoot.sh` avvia l'app con
> `env -C` sulla directory di destinazione.

Numero misurato su questo progetto (identica configurazione, `-O3`):

```
frame completo : 0.502 ms   registrazione CPU: 0.037 ms media, 0.078 ms picco
```

Prima della conversione a renderer SDF, a `-O0` e con 4 primitive per tasto:
0.698 ms di frame e **0.528 ms** di registrazione — circa **14× di più**.

## Screenshot su compositor

```bash
./shoot.sh ./build/calcC /tmp/calcc.png 4
```

Harness che avvia il binario staccato dalla sessione del terminale, lo mette in
floating su niri, e salva uno screenshot. **Parte da `CALCC_SHOT`** — la finestra
fotografa il proprio framebuffer, quindi l'immagine contiene solo l'app, senza
chrome del compositore né finestre circostanti, e funziona anche dove `grim` non è
installato. `grim` è solo il ripiego, e cattura l'intero schermo.

## Struttura

```
calcC/
├── CMakeLists.txt        C11, Release, Wayland, FetchContent raylib 5.0, Threads
├── config.h              costanti di layout/buffer + rilevamento tema
├── main.c                keymap, layout, stato, input, semantica, main loop
├── logic/
│   ├── eval.h            API dell'AST a token
│   └── eval.c            pool di nodi, free list, shunting-yard, formattazione
├── ui/
│   ├── theme.h/.c        struct Style/Theme, palette chiaro/scuro, thread di polling
│   ├── button.h/.c       animazioni a molla, hover, ripple, layout dell'etichetta
│   └── renderer.h/.c     due shader GLSL 330 (SDF pannello + sfondo) + fallback CPU
├── assets/Comfortaa.ttf  unico asset caricato a runtime
├── eval_test.c           unit test del motore aritmetico
├── bench_eval.c          micro-benchmark vecchio vs nuovo valutatore
├── bench_baseline.c      micro-benchmark del costo del rilevamento tema
└── shoot.sh              screenshot via compositor (niri)
```

| File | Righe | Ruolo |
|---|---|---|
| `main.c` | 637 | Entry point, keymap, layout, semantica della calcolatrice, main loop |
| `ui/renderer.c` | 348 | Due fragment shader GLSL 330, API del renderer, fallback CPU |
| `logic/eval.c` | 329 | Pool di nodi, costruzione/espressione, shunting-yard, formattazione |
| `ui/theme.c` | 229 | Rilevamento tema (Win/macOS/Linux), palette, thread di polling |
| `ui/button.c` | 169 | Animazioni, layout dell'etichetta, delega al renderer |
| `eval_test.c` | 146 | Test del motore aritmetico |
| `ui/renderer.h` | 84 | Struct `Panel`/`Background` e API |
| `ui/theme.h` | 69 | `Style` + `Theme` (coppie di gradienti) |
| `ui/button.h` | 48 | `ButtonKind`, struct `Button`, API |
| `config.h` | 55 | Costanti di layout e buffer |
| `logic/eval.h` | 55 | Tipi `ExprNode`/`Expr`, limiti, dichiarazioni |

## Funzionalità

**Presente:** le quattro operazioni, modulo (`%` come `fmod` binario, **non**
percentuale), decimali con punto **e** con virgola, `Ans` come continuazione del
risultato, cronologia navigabile con `↑`/`↓`, backspace, `C`, una sola operazione
pendente (`5++3` → `5+3`), animazione a molla e ripple sui pulsanti, layout
proporzionale che si adatta a qualsiasi aspect ratio di tiling, tema chiaro/scuro
rilevato dal sistema.

**Assente, per scelta:** **nessuna parentesi**, nessun `√`, nessun `1/x`, nessun
`±`, nessun `M+`/`MR`, nessuna copia/incolla dagli appunti, nessuna notazione
scientifica in ingresso, nessuna localizzazione (la stringa `Errore` e i commenti
sono in italiano). `%=` è un tasto distinto dal `%`.

## Note tecniche

- **Tastiera unica fonte di verità**: la tabella `KEYS[5][4]` in `main.c` genera il
  layout *e* il lookup del flash, quindi mouse e tastiera non possono
  disaccordarsi.
- **Associatività a sinistra** garantita dal `>=` nel drenaggio delle precedenze
  nello shunting-yard: `10-3-2 == 5`, `100/10/2 == 5`.
- **Nessuna `malloc` per token**: pool statico di 4096 nodi con free list,
  O(1), nessuna frammentazione. L'esaurimento del pool ** tronca
  silenziosamente** l'espressione invece di segnalare un errore.
- **Rendering batched**: ogni forma costa 2 chiamate GL, non 4 primitive, perché
  tutti i parametri viaggiano in un unico `uniform vec4[10]` caricato con una
  sola `glUniform4fv`.
- **Degradazione pulita**: se gli shader non si compilano, `DrawPanel` e
  `DrawBackground` cadono su un fallback CPU con rettangoli arrotondati piatti.
  L'app funziona e resta accettabile senza alcun supporto shader.
- **Dimensione minima di finestra**: `SetWindowMinSize(240, 340)`. Sotto quella soglia
  la matematica del layout (griglia 4×5 con margini proporzionali) produrrebbe celle
  negative o degenerate.
- **`USE_X11 OFF` non disabilita davvero X11**: raylib non propaga quel flag a
  `GLFW_BUILD_X11`, quindi **entrambi i backend vengono compilati** in
  `libraylib.a` e GLFW carica quello disponibile a runtime. Wayland è preferito,
  e il commento nel `CMakeLists.txt` ora lo dice senza sovradichiarlo.
- **Rilevamento del tema in background**: `DetectSystemThemeDark()` gira su un
  thread separato, non più 19 volte per frame (570 ms/frame, il bug documentato da
  `bench_baseline.c`).

## Limiti noti

I quattro problemi elencati in precedenza come limiti sono stati **chiusi**:
il rilascio del mouse traccia ormai il tasto di partenza (`pressIndex`),
`FormatNumber` controlla `strlen` prima di costruire il puntatore,
la build Windows ricompila (`CALCC_HAS_THREAD` a 0 → polling in linea, perché
la lettura del registro costa microsecondi e non i 5 ms di un `popen`), e la
voce `graphics/` inesistente è stata rimossa.

Restano aperti:

- **L'errore non dice dove sia**: `1/0` e `5%0` producono entrambi `Errore`, e
  l'espressione resta intatta. Non c'è indicazione di posizione.
- **`ExprPop` è O(n)**: cammina fino al penultimo nodo invece di tenere un
  puntatore al penultimo. Irrilevante con il tetto di 160 token.
- **L'esaurimento del pool tronca in silenzio**: un'espressione troncata è
  peggio di un errore visibile, se le costanti cambiassero mai.
- **`shoot.sh` resta legato a niri** per la messa in floating; il resto del
  percorso funziona anche senza `grim`.

Dettaglio completo in [`docs.md`](./docs.md).

## Documentazione

| File | Contenuto |
|---|---|
| [`docs.md`](./docs.md) | Architettura, il motore shunting-yard, il renderer SDF, layout, test, limiti con riferimenti di codice |
