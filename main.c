#include "raylib.h"
#include "config.h"
#include "ui/button.h"
#include "ui/renderer.h"
#include "ui/theme.h"
#include "logic/eval.h"
#include <math.h>
#include <stdlib.h>

// ============================================================
//  TASTIERA  (l'unica fonte di verita': il layout e' derivato
//  da qui, quindi mouse e tastiera non possono disaccordarsi)
// ============================================================
static const char* KEYS[5][4] = {
    { "C",  "%",  "/",  "*"  },
    { "7",  "8",  "9",  "-"  },
    { "4",  "5",  "6",  "+"  },
    { "1",  "2",  "3",  ""   },   // '=' occupa le due righe finali
    { "0",  "",   ".",  ""   }    // '0' occupa due colonne
};

static ButtonKind KindOf(const char* t) {
    if (!strcmp(t, "C")) return BTN_CLEAR;
    if (!strcmp(t, "=")) return BTN_EQ;
    if (!strcmp(t, "+") || !strcmp(t, "-") || !strcmp(t, "*") ||
        !strcmp(t, "/") || !strcmp(t, "%")) return BTN_OP;
    return BTN_DIGIT;
}

// ============================================================
//  STATO APPLICAZIONE
// ============================================================
#define NUM_BUTTONS 18

static Button  buttons[NUM_BUTTONS];
static int      buttonCount = 0;
static int      pressIndex  = -1;   // tasto su cui e' iniziato il press

static Expr     expr;                       // espressione corrente
static double   lastResult = 0.0;
static bool     hasResult = false;          // c'e' un risultato da usare come Ans
static bool     isResultState = false;      // il display sta mostrando un risultato
static bool     isErrorState = false;       // l'ultima valutazione e' fallita

static char     displayBuffer[MAX_EXPR_TEXT];
static char     resultBuffer[MAX_RESULT];
static bool     exprDirty = true;           // il testo va rigenerato?

// Cronologia
static Expr     history[MAX_HISTORY];
static int      historyCount = 0;
static int      historyIdx  = -1;

// Animazioni
static float    animSlide = 0.0f;   // 0 = digitando, 1 = risultato a schermo
static float    animPop   = 0.0f;   // rimbalzo del testo appena digitato
static float    animShake = 0.0f;   // scossa orizzontale in caso di errore
static int      lastDisplayLen = 0;
static double   caretT = 0.0;       // fase del lampeggio del cursore

// Layout in cache: ricalcolato solo quando cambia la dimensione
static int      cachedW = -1, cachedH = -1;
static Rectangle appRec     = { 0 };
static Rectangle displayRec = { 0 };

// Diagnostica
static bool     showFps = false;
static double   frameMsSmoothed = 0.0;

// ============================================================
//  LAYOUT  (ricalcolato solo se la finestra cambia dimensione)
// ============================================================
static void ResetButton(Button* b, const char* label, ButtonKind kind, Rectangle rect) {
    b->text     = label;
    b->kind     = kind;
    b->rect     = rect;
    b->press    = 0.0f;
    b->pressVel = 0.0f;
    b->hover    = 0.0f;
    b->flash    = 0.0f;
    b->ripple   = 0.0f;
    b->rippleAt = (Vector2){ rect.x + rect.width*0.5f, rect.y + rect.height*0.5f };
    b->hovered  = false;
}

static void UpdateLayout(int sw, int sh) {
    if (sw == cachedW && sh == cachedH) return;
    cachedW = sw; cachedH = sh;

    // inset di 1 px: se il pannello toccasse il bordo della finestra
    // l'anti-aliasing del bordo verrebbe tagliato dal compositor
    appRec = (Rectangle){ 1.0f, 1.0f, (float)sw - 2.0f, (float)sh - 2.0f };

    displayRec = (Rectangle){ sw * DISPLAY_TOP, sh * DISPLAY_TOP,
                              sw * (1.0f - 2 * DISPLAY_TOP), sh * DISPLAY_HEIGHT };

    float side   = (sw < sh) ? (float)sw : (float)sh;
    float gap    = side * GRID_GAP;
    float availW = sw * (1.0f - 2 * GRID_MARGIN_X);
    float top    = displayRec.y + displayRec.height + gap * 1.35f;
    float availH = sh - top - gap * 0.55f;

    float cellW = (availW - 3 * gap) / 4.0f;
    float cellH = (availH - 4 * gap) / 5.0f;
    float size  = (cellW < cellH) ? cellW : cellH;

    float gridW = 4 * size + 3 * gap;
    float gridH = 5 * size + 4 * gap;
    float gx    = sw * GRID_MARGIN_X + (availW - gridW) * 0.5f;
    float gy    = top + (availH - gridH) * 0.5f;

    int idx = 0;
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            const char* label = KEYS[r][c];
            if (label[0] == '\0') continue;

            float bx = gx + c * (size + gap);
            float by = gy + r * (size + gap);
            Rectangle rect = (r == 4 && c == 0)         // "0" occupa due colonne
                           ? (Rectangle){ bx, by, 2 * size + gap, size }
                           : (Rectangle){ bx, by, size, size };

            ResetButton(&buttons[idx++], label, KindOf(label), rect);
        }
    }

    // "=" alta due righe, nell'ultima colonna
    ResetButton(&buttons[idx], "=", BTN_EQ,
                (Rectangle){ gx + 3 * (size + gap), gy + 3 * (size + gap), size, 2 * size + gap });
    idx++;

    buttonCount = idx;
    pressIndex  = -1;      // un resize non deve lasciare un press appeso
}

static Button* FindButton(const char* label) {
    for (int i = 0; i < buttonCount; i++)
        if (!strcmp(buttons[i].text, label)) return &buttons[i];
    return NULL;
}

// Tasto premuto da tastiera: stessa animazione del mouse, con
// l'onda che parte dal centro del tasto.
static void FlashButton(const char* label) {
    Button* b = FindButton(label);
    if (b) FlashButtonPress(b, (Vector2){ b->rect.x + b->rect.width*0.5f,
                                          b->rect.y + b->rect.height*0.5f });
}

// ============================================================
//  LOGICA DI CALCOLO
// ============================================================
static void PushToHistory(void) {
    if (ExprCount(&expr) == 0) return;
    if (historyCount == MAX_HISTORY) {
        ExprClear(&history[0]);                       // libera i nodi del piu' vecchio
        for (int i = 1; i < MAX_HISTORY; i++) history[i - 1] = history[i];
        historyCount = MAX_HISTORY - 1;
    }
    history[historyCount++] = ExprCopy(&expr);
    historyIdx = historyCount;
}

static void ShowResult(void) {
    if (ExprCount(&expr) == 0) { isResultState = false; return; }

    bool err = false;
    double v = EvaluateExpr(&expr, &err);

    if (err) {
        // Prima l'errore spariva e non si capiva cosa fosse successo.
        // Ora resta sul display, in rosso, e il riquadro scuote.
        snprintf(resultBuffer, sizeof(resultBuffer), "Errore");
        isResultState = true;
        isErrorState  = true;
        animSlide     = 0.0f;
        animShake     = 1.0f;
        return;
    }

    isErrorState = false;
    PushToHistory();

    lastResult  = v;
    hasResult   = true;
    FormatNumber(lastResult, resultBuffer, sizeof(resultBuffer));
    isResultState = true;
    animSlide    = 0.0f;
}

static void ClearAll(void) {
    ExprClear(&expr);
    resultBuffer[0] = '\0';
    isResultState = false;
    isErrorState  = false;
}

static void LoadHistory(int idx) {
    ExprClear(&expr);
    if (idx >= 0 && idx < historyCount) expr = ExprCopy(&history[idx]);
    isResultState = false;
    isErrorState  = false;
    resultBuffer[0] = '\0';
    exprDirty = true;
}

// Un tasto premuto (mouse o tastiera).
static void PressKey(const char* key) {
    if (!key || !key[0]) return;
    FlashButton(key);
    exprDirty = true;                 // il display va rigenerato questo frame
    isErrorState = false;

    if (!strcmp(key, "=")) { ShowResult(); return; }
    if (!strcmp(key, "C")) { ClearAll();  return; }

    // ---- dopo un risultato: un operatore continua da Ans,
    //      un numero/dot inizia una nuova espressione ----
    if (isResultState) {
        if (strchr("+-*/%", key[0])) {
            ExprClear(&expr);
            if (hasResult) ExprValue(&expr, lastResult);
            isResultState = false;
            resultBuffer[0] = '\0';
            ExprOp(&expr, key[0]);
        } else {
            ClearAll();
        }
    }

    if (key[0] >= '0' && key[0] <= '9') {
        ExprDigit(&expr, key[0] - '0');
        return;
    }

    if (!strcmp(key, ".")) {
        bool afterOp = expr.tail && expr.tail->type == NODE_OP;
        if (afterOp) ExprDigit(&expr, 0);          // "5+" poi "." -> "5+0."
        if (!ExprLastNumberHasDot(&expr)) ExprOp(&expr, '.');
        return;
    }

    if (strchr("+-*/%", key[0])) {
        // non accumulare due operatori di fila: "5++3" -> "5+3"
        if (expr.tail && expr.tail->type == NODE_OP) ExprPop(&expr);
        ExprOp(&expr, key[0]);
    }
}

static void PressBackspace(void) {
    exprDirty = true;
    if (isResultState) { ClearAll(); return; }   // dal risultato si torna a digitare
    ExprPop(&expr);
}

static void HandleKeyboard(void) {
    int key = GetKeyPressed();
    while (key != 0) {
        switch (key) {
            case KEY_C: case KEY_DELETE:
                PressKey("C");
                break;
            case KEY_BACKSPACE: PressBackspace(); break;
            case KEY_EQUAL: case KEY_KP_EQUAL:
            case KEY_ENTER: case KEY_KP_ENTER:
                PressKey("=");
                break;
            case KEY_UP:
                if (historyCount > 0 && historyIdx > 0) LoadHistory(--historyIdx);
                break;
            case KEY_DOWN:
                if (historyIdx < historyCount) LoadHistory(++historyIdx);
                break;
            case KEY_F3:
                showFps = !showFps;
                break;
            default: break;
        }
        key = GetKeyPressed();
    }

    // Ripetizione automatica solo per il backspace (tenendo premuto)
    if (IsKeyPressedRepeat(KEY_BACKSPACE)) PressBackspace();

    // Caratteri: copre sia la tastiera principale sia il keypad
    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= '0' && ch <= '9') {
            char k[2] = { (char)ch, 0 };
            PressKey(k);
        } else if (strchr("+-*/%", ch)) {
            char k[2] = { (char)ch, 0 };
            PressKey(k);
        } else if (ch == '.' || ch == ',') {
            PressKey(".");
        } else if (ch == '=' || ch == '\n' || ch == '\r') {
            PressKey("=");
        }
        ch = GetCharPressed();
    }
}

// ============================================================
//  DISPLAY
// ============================================================

// Riduce il font finche' il testo entra nella larghezza disponibile.
// La larghezza e' monotona nel corpo, quindi una ricerca binaria
// (7 interrogazioni) batte la scansione a passi di 1 px, che prima
// faceva fino a ~30 MeasureTextEx per stringa e per frame.
static float FitFontSize(Font font, const char* text, float desired, float maxW, float minSize) {
    if (maxW <= 0.0f) return desired;
    if (MeasureTextEx(font, text, desired, 1.0f).x <= maxW) return desired;

    float lo = minSize, hi = desired;
    for (int i = 0; i < 7; i++) {
        float mid = (lo + hi)*0.5f;
        if (MeasureTextEx(font, text, mid, 1.0f).x > maxW) hi = mid; else lo = mid;
    }
    return lo;
}

// Cache del fit: cambia solo se cambia testo o larghezza disponibile,
// quindi in regime normale non si ricalcola mai.
typedef struct {
    char  text[MAX_RESULT];
    float maxW, size;
    bool  valid;
} FitCache;

static FitCache fitExpr;
static FitCache fitRes;

static float FitCached(FitCache* c, Font font, const char* text,
                       float desired, float maxW, float minSize) {
    if (c->valid && c->maxW == maxW && !strcmp(c->text, text)) return c->size;
    c->size = FitFontSize(font, text, desired, maxW, minSize);
    snprintf(c->text, sizeof(c->text), "%s", text);
    c->maxW  = maxW;
    c->valid = true;
    return c->size;
}

// Pannello del display: gradiente, ombra, griglia di puntini, bordo.
// Con lo shader e' un draw call; senza, arrotondamento classico.
static void DrawDisplayPanel(const Theme* t, float shakeX) {
    Rectangle r = displayRec;
    r.x += shakeX;
    float side = (r.width < r.height) ? r.width : r.height;

    Panel p;
    memset(&p, 0, sizeof(p));
    p.rect         = r;
    p.radius       = side*0.22f;
    p.top          = t->displayTop;
    p.bottom       = t->displayBottom;
    p.highlight    = 0.45f;
    p.shadow       = t->dark ? 0.55f : 0.14f;
    p.shadowSpread = (side*0.09f < 16.0f ? side*0.09f : 16.0f);
    p.shadowColor  = t->displayShadow;
    p.border       = t->displayBorder;
    p.borderW      = 1.0f;
    p.dotStrength  = t->dark ? 0.50f : 0.45f;
    p.dotSpacing   = (side*0.095f > 15.0f ? side*0.095f : 15.0f);
    p.dotColor     = t->displayDot;

    DrawPanel(&p);
}

static void DrawDisplay(Font font, const Theme* t) {
    // --- scossa in caso di errore, smorzata ---
    float shakeX = 0.0f;
    if (animShake > 0.0f) {
        animShake -= GetFrameTime()*3.2f;
        if (animShake < 0.0f) animShake = 0.0f;
        shakeX = sinf(animShake*46.0f)*animShake*animShake*7.0f;
    }

    DrawDisplayPanel(t, shakeX);

    Rectangle dr = displayRec;
    dr.x += shakeX;

    float padX   = dr.width  * 0.085f;
    float padY   = dr.height * 0.115f;
    float innerW = dr.width - 2*padX;

    float exprSize   = FitCached(&fitExpr, font, displayBuffer, dr.height*0.28f, innerW, 10.0f);
    float resultSize = FitCached(&fitRes,  font, resultBuffer,  dr.height*0.38f, innerW, 12.0f);

    // effetto pop quando la lunghezza cresce
    int curLen = (int)strlen(displayBuffer);
    if (curLen > lastDisplayLen) animPop = 1.0f;
    lastDisplayLen = curLen;
    if (animPop > 0.0f) {
        animPop -= GetFrameTime() * 6.0f;
        if (animPop < 0.0f) animPop = 0.0f;
    }

    float e = animSlide * animSlide * (3.0f - 2.0f * animSlide);   // smoothstep

    // --- espressione: centrata in digitazione, in alto col risultato ---
    float eDraw = exprSize*(1.0f + 0.055f*animPop);
    Vector2 eSize = MeasureTextEx(font, displayBuffer, eDraw, 1.0f);
    float eCenterY = dr.y + dr.height*0.5f - eSize.y*0.5f;
    float eTopY    = dr.y + padY;
    float eY = eCenterY + (eTopY - eCenterY)*e;

    float ex = dr.x + dr.width - padX - eSize.x;

    // --- risultato: sale dal basso e si ingrandisce leggermente ---
    float rScale = 0.92f + 0.08f*e;
    Vector2 rSize = MeasureTextEx(font, resultBuffer, resultSize*rScale, 1.0f);
    float rCenterY = dr.y + dr.height*0.5f + dr.height*0.07f;
    float rOffY    = dr.y + dr.height + rSize.y;
    float rY = rOffY + (rCenterY - rOffY)*e;
    float rx = dr.x + dr.width - padX - rSize.x;

    BeginScissorMode((int)dr.x, (int)dr.y, (int)dr.width, (int)dr.height);

    // etichetta "Ans" quando il risultato e' richiamabile
    if (isResultState && e > 0.15f) {
        const char* ansTag = "= ";
        float tagSize = exprSize * 0.78f;
        Vector2 aSize = MeasureTextEx(font, ansTag, tagSize, 1.0f);
        Color aCol = t->textDim;
        aCol.a = (unsigned char)(150 * e);
        DrawTextEx(font, ansTag,
                   (Vector2){ ex - aSize.x - 5, dr.y + padY * 0.55f },
                   tagSize, 1.0f, aCol);
    }

    Color eCol = t->text;
    eCol.a = (unsigned char)(255 - 115 * e);
    DrawTextEx(font, displayBuffer, (Vector2){ ex, eY }, eDraw, 1.0f, eCol);

    // cursore: lampeggia solo mentre si digita, sparisce col risultato
    if (!isResultState && displayBuffer[0]) {
        caretT += GetFrameTime();
        if (caretT > 1.0) caretT -= 1.0;
        float blink = 1.0f - 0.8f*powf(sinf(caretT*3.1416f), 8.0f);
        float cw = exprSize*0.07f;
        Rectangle caret = { ex + eSize.x + 5.0f, eY + eSize.y*0.10f, cw, eSize.y*0.84f };
        Panel cp;
        memset(&cp, 0, sizeof(cp));
        cp.rect = caret;
        cp.radius = cw*0.5f;
        cp.top = t->caret; cp.bottom = t->caret;
        cp.top.a = cp.bottom.a = (unsigned char)(255*blink);
        DrawPanel(&cp);
    }

    if (e > 0.01f && resultBuffer[0]) {
        Color rCol = isErrorState ? t->error : t->result;
        rCol.a = (unsigned char)(255 * e);
        DrawTextEx(font, resultBuffer, (Vector2){ rx, rY }, resultSize*rScale, 1.0f, rCol);
    }
    EndScissorMode();
}

// ============================================================
//  STRUMENTAZIONE DI SVILUPPO (attiva solo se richiesta via env)
//    CALCC_BENCH=<n>        gira n frame SENZA vsync/MSAA e stampa
//                           il costo reale per frame (CPU+GPU)
//    CALCC_SHOT=<png>       salva uno screenshot della finestra
//    CALCC_SHOT_FRAME=<n>   ... al frame n (default 24)
//    CALCC_SHOT_KEYS="..."  sequenza di tasti da digitare prima dello
//                           shot (utile per vedere il risultato)
//    CALCC_SIZE=900x1400    apre la finestra a quelle dimensioni
//    CALCC_THEME=light|dark forza il tema (lo stesso override vale per
//                           l'utente su compositori che non lo espongono)
//  Serve per misurare le modifiche: niente di tutto questo gira
//  nella build normale, sono 4 chiamate a getenv() all'avvio.
// ============================================================
static int EnvInt(const char* name, int def) {
    const char* v = getenv(name);
    return (v && *v) ? atoi(v) : def;
}

// ============================================================
//  MAIN
// ============================================================
int main(void) {
    int  benchFrames = EnvInt("CALCC_BENCH", 0);
    const char* shotPath = getenv("CALCC_SHOT");
    int  shotFrame = EnvInt("CALCC_SHOT_FRAME", 24);
    const char* shotKeys = getenv("CALCC_SHOT_KEYS");

    // In benchmark si toglie il vsync: altrimenti il limite lo darebbe
    // il monitor (16.7 ms) e non misureremmo nulla. Gli altri flag restano
    // quelli della build normale, cosi' il numero e' quello che gira
    // davvero. CALCC_BENCH_NOMSA=1 confronta anche senza MSAA.
    unsigned int flags = FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_TRANSPARENT;
    if (benchFrames <= 0) flags |= FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT;
    else if (!EnvInt("CALCC_BENCH_NOMSA", 0)) flags |= FLAG_MSAA_4X_HINT;

    SetConfigFlags(flags);

    int winW = SCREEN_WIDTH, winH = SCREEN_HEIGHT;
    const char* size = getenv("CALCC_SIZE");
    if (size) {
        int w = 0, h = 0;
        if (sscanf(size, "%dx%d", &w, &h) == 2 && w > 200 && h > 200) { winW = w; winH = h; }
    }

    InitWindow(winW, winH, "Calculator");

    // Sotto questa soglia la matematica del layout (griglia 4x5 con
    // margini proporzionali) produce celle negative o degeni.
    SetWindowMinSize(240, 340);

    if (!IsWindowReady()) { TraceLog(LOG_ERROR, "InitWindow fallita"); return 1; }

    PoolInit();
    ExprInit(&expr);
    ThemeInit();                              // ~5 ms, e solo una volta
    RendererInit();                           // compila gli shader

    Font font = LoadFontEx("assets/Comfortaa.ttf", 96, NULL, 0);

    int    frameNo = 0;
    double cpuSum = 0.0, cpuMax = 0.0, wallT0 = 0.0;
    int    cpuN = 0;

    while (!WindowShouldClose()) {
        double t0 = GetTime();
        if (frameNo == 0) wallT0 = t0;
        float  dt = (float)GetFrameTime();

        // sequenza di tasti di test (solo per gli screenshot)
        if (frameNo == 1 && shotKeys) {
            for (const char* p = shotKeys; *p; p++) {
                char k[2] = { *p, 0 };
                PressKey(k);
            }
        }

        // ---- il tema di sistema e' gia' pronto: qui solo una lettura ----
        ThemePollSystem();

        int sw = GetScreenWidth(), sh = GetScreenHeight();
        UpdateLayout(sw, sh);
        HandleKeyboard();

        // ---- input mouse ----
        Vector2 mouse = GetMousePosition();
        bool down = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        bool rel  = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

        for (int i = 0; i < buttonCount; i++) {
            Button* b = &buttons[i];
            b->hovered = CheckCollisionPointRec(mouse, b->rect);

            if (b->hovered && down && pressIndex < 0) {
                // Si ricorda SU QUALE tasto e' iniziato il press: senza
                // questo, premere "7" e rilasciare sopra "9" digitava 9.
                pressIndex = i;
                FlashButtonPress(b, mouse);   // onda dal punto premuto
            }
            UpdateButtonAnim(b, down, dt);
        }

        if (rel && pressIndex >= 0) {
            Button* b = &buttons[pressIndex];
            if (CheckCollisionPointRec(mouse, b->rect)) PressKey(b->text);
            pressIndex = -1;
        }

        if (exprDirty) {
            ExprToString(&expr, displayBuffer, sizeof(displayBuffer));
            exprDirty = false;
        }

        // ---- animazione slide (indipendente dal framerate) ----
        float target = isResultState ? 1.0f : 0.0f;
        animSlide += (target - animSlide)*(1.0f - expf(-dt*9.0f));

        const Theme* t = ThemeGet();

        // ================= RENDER =================
        double cpuT0 = GetTime();
        BeginDrawing();
        ClearBackground(BLANK);

        RendererSetFramebuffer(GetRenderWidth(), GetRenderHeight());

        // ---- sfondo: pannello arrotondato con gradiente + alone ----
        Background bg;
        memset(&bg, 0, sizeof(bg));
        bg.panel     = appRec;
        bg.radius    = (appRec.width < appRec.height ? appRec.width : appRec.height)*0.16f;
        bg.top       = t->bgTop;
        bg.bottom    = t->bgBottom;
        bg.border    = t->bgBorder;
        bg.borderW   = 1.0f;
        bg.glowCenter = (Vector2){ 0.5f,
                                   (displayRec.y + displayRec.height*0.5f - appRec.y)/appRec.height };
        bg.glowRadius = 0.85f;
        bg.glowColor  = t->bgGlow;
        bg.glowStrength = t->bgGlowStrength;
        bg.vignette   = t->dark ? 0.55f : 0.35f;
        bg.vignetteColor = t->bgVignette;
        DrawBackground(&bg);

        DrawDisplay(font, t);

        for (int i = 0; i < buttonCount; i++)
            DrawCalcButton(&buttons[i], font, t);

        if (showFps) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.0f FPS  %.2f ms", (float)GetFPS(), frameMsSmoothed);
            Panel fp;
            memset(&fp, 0, sizeof(fp));
            fp.rect = (Rectangle){ 10, 10, 168, 28 };
            fp.radius = 8.0f;
            fp.top = fp.bottom = (Color){ 0, 0, 0, (unsigned char)(t->dark ? 190 : 150) };
            DrawPanel(&fp);
            DrawTextEx(font, buf, (Vector2){ 20, 18 }, 15, 1, WHITE);
        }

        // Costo della sola registrazione del frame lato CPU: e' la
        // parte che possiamo ottimizzare. EndDrawing (che attende il
        // vsync) resta fuori dal conto di proposito.
        double cpu = (GetTime() - cpuT0) * 1000.0;
        EndDrawing();

        double frame = (GetTime() - t0) * 1000.0;
        frameMsSmoothed += (frame - frameMsSmoothed) * 0.1;

        // ---- statistiche (solo benchmark) ----
        if (benchFrames > 0 && frameNo >= 30) {       // scarta il warm-up
            cpuSum += cpu; cpuMax = (cpu > cpuMax ? cpu : cpuMax); cpuN++;
        }
        frameNo++;

        if (shotPath && frameNo + 1 == shotFrame) {
            TakeScreenshot(shotPath);
            TraceLog(LOG_INFO, "screenshot -> %s", shotPath);
        }
        if (benchFrames > 0 && frameNo >= benchFrames) {
            double wall = (GetTime() - wallT0) * 1000.0;
            printf("== CALCC bench: %d frame in %.1f ms ==\n", frameNo, wall);
            printf("   frame completo : %7.3f ms  (%6.1f fps max)\n",
                   wall / frameNo, 1000.0 / (wall / frameNo));
            printf("   registrazione  : %7.3f ms media | %7.3f ms picco  (%d frame)\n",
                   cpuSum / cpuN, cpuMax, cpuN);
            printf("   -> budget frame 60 fps: 16.67 ms | 144 fps: 6.94 ms\n");
            break;
        }
    }

    for (int i = 0; i < historyCount; i++) ExprClear(&history[i]);
    ExprClear(&expr);
    ThemeStop();            // ferma il thread che interroga il tema
    RendererShutdown();
    CloseWindow();
    return 0;
}
