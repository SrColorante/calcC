#include "raylib.h"
#include "config.h"
#include "ui/button.h"
#include "ui/theme.h"
#include "logic/eval.h"
#include <math.h>

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

static Expr     expr;                       // espressione corrente
static double   lastResult = 0.0;
static bool     hasResult = false;          // c'e' un risultato da usare come Ans
static bool     isResultState = false;      // il display sta mostrando un risultato

static char     displayBuffer[MAX_EXPR_TEXT];
static char     resultBuffer[MAX_RESULT];

// Cronologia
static Expr     history[MAX_HISTORY];
static int      historyCount = 0;
static int      historyIdx  = -1;

// Animazioni
static float    animSlide = 0.0f;   // 0 = digitando, 1 = risultato a schermo
static float    animPop   = 0.0f;   // rimbalzo del testo appena digitato
static int      lastDisplayLen = 0;

// Layout in cache: ricalcolato solo quando cambia la dimensione
static int      cachedW = -1, cachedH = -1;
static Rectangle displayRec = { 0 };

// Diagnostica
static bool     showFps = false;
static double   frameMsSmoothed = 0.0;

// ============================================================
//  LAYOUT  (ricalcolato solo se la finestra cambia dimensione)
// ============================================================
static void UpdateLayout(int sw, int sh) {
    if (sw == cachedW && sh == cachedH) return;
    cachedW = sw; cachedH = sh;

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

            Button* b = &buttons[idx++];
            b->text    = label;
            b->kind    = KindOf(label);
            b->press   = 0.0f;
            b->hovered = false;

            float bx = gx + c * (size + gap);
            float by = gy + r * (size + gap);

            if (r == 4 && c == 0) {                     // "0" occupa due colonne
                b->rect = (Rectangle){ bx, by, 2 * size + gap, size };
            } else {
                b->rect = (Rectangle){ bx, by, size, size };
            }
        }
    }

    // "=" alta due righe, nell'ultima colonna
    Button* eq = &buttons[idx++];
    eq->text    = "=";
    eq->kind    = BTN_EQ;
    eq->press   = 0.0f;
    eq->hovered = false;
    eq->rect    = (Rectangle){ gx + 3 * (size + gap), gy + 3 * (size + gap), size, 2 * size + gap };

    buttonCount = idx;
}

static Button* FindButton(const char* label) {
    for (int i = 0; i < buttonCount; i++)
        if (!strcmp(buttons[i].text, label)) return &buttons[i];
    return NULL;
}

static void FlashButton(const char* label) {
    Button* b = FindButton(label);
    if (b) b->press = 1.0f;      // feedback visivo anche digitando
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
        resultBuffer[0] = '\0';
        isResultState = false;
        return;
    }

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
}

static void LoadHistory(int idx) {
    ExprClear(&expr);
    if (idx >= 0 && idx < historyCount) expr = ExprCopy(&history[idx]);
    isResultState = false;
    resultBuffer[0] = '\0';
}

// Un tasto premuto (mouse o tastiera).
static void PressKey(const char* key) {
    if (!key || !key[0]) return;
    FlashButton(key);

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
static float FitFontSize(Font font, const char* text, float desired, float maxW, float minSize) {
    if (maxW <= 0.0f) return desired;
    float size = desired;
    while (size > minSize && MeasureTextEx(font, text, size, 1.0f).x > maxW) size -= 1.0f;
    return size;
}

static void DrawDisplay(Font font, const Theme* t) {
    DrawRectangleRounded(displayRec, ROUND_DISPLAY, SEGM, t->displayBg);

    // griglia di puntini: texture sottile che da' profondita' al riquadro
    Color dot = t->displayDot;
    dot.a = 70;
    BeginScissorMode((int)displayRec.x, (int)displayRec.y,
                     (int)displayRec.width, (int)displayRec.height);
    for (float y = displayRec.y + 18; y < displayRec.y + displayRec.height; y += 22)
        for (float x = displayRec.x + 18; x < displayRec.x + displayRec.width; x += 22)
            DrawPixel((int)x, (int)y, dot);
    EndScissorMode();

    DrawRectangleRoundedLines(displayRec, ROUND_DISPLAY, SEGM, 1.5f, t->displayLine);

    float padX   = displayRec.width  * 0.075f;
    float padY   = displayRec.height * 0.11f;
    float innerW = displayRec.width - 2 * padX;

    float exprSize   = FitFontSize(font, displayBuffer, displayRec.height * 0.27f, innerW, 10.0f);
    float resultSize = FitFontSize(font, resultBuffer,  displayRec.height * 0.40f, innerW, 12.0f);

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
    Vector2 eSize = MeasureTextEx(font, displayBuffer, exprSize, 1.0f);
    float eCenterY = displayRec.y + displayRec.height * 0.5f - eSize.y * 0.5f;
    float eTopY    = displayRec.y + padY;
    float eY = eCenterY + (eTopY - eCenterY) * e;

    float ex = displayRec.x + displayRec.width - padX - eSize.x;

    // --- risultato: sale dal basso ---
    Vector2 rSize = MeasureTextEx(font, resultBuffer, resultSize, 1.0f);
    float rCenterY = displayRec.y + displayRec.height * 0.5f + displayRec.height * 0.06f;
    float rOffY    = displayRec.y + displayRec.height + rSize.y;
    float rY = rOffY + (rCenterY - rOffY) * e;
    float rx = displayRec.x + displayRec.width - padX - rSize.x;

    BeginScissorMode((int)displayRec.x, (int)displayRec.y,
                     (int)displayRec.width, (int)displayRec.height);

    // etichetta "Ans" quando il risultato e' richiamabile
    if (isResultState && e > 0.15f) {
        const char* ansTag = "= ";
        float tagSize = exprSize * 0.8f;
        Vector2 aSize = MeasureTextEx(font, ansTag, tagSize, 1.0f);
        Color aCol = t->textDim;
        aCol.a = (unsigned char)(150 * e);
        DrawTextEx(font, ansTag,
                   (Vector2){ ex - aSize.x - 4, displayRec.y + padY * 0.6f },
                   tagSize, 1.0f, aCol);
    }

    Color eCol = t->text;
    eCol.a = (unsigned char)(255 - 110 * e);
    DrawTextEx(font, displayBuffer, (Vector2){ ex, eY }, exprSize, 1.0f, eCol);

    if (e > 0.01f && resultBuffer[0]) {
        Color rCol = t->result;
        rCol.a = (unsigned char)(255 * e);
        DrawTextEx(font, resultBuffer, (Vector2){ rx, rY }, resultSize, 1.0f, rCol);
    }
    EndScissorMode();
}

// ============================================================
//  MAIN
// ============================================================
int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_TRANSPARENT |
                   FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Calculator");

    if (!IsWindowReady()) { TraceLog(LOG_ERROR, "InitWindow fallita"); return 1; }

    PoolInit();
    ExprInit(&expr);
    ThemeInit();                              // ~30 ms, ma solo una volta

    Font font = LoadFontEx("assets/Comfortaa.ttf", 96, NULL, 0);

    while (!WindowShouldClose()) {
        double t0 = GetTime();
        float  dt = (float)GetFrameTime();

        // ---- il tema di sistema NON viene interrogato qui dentro ----
        ThemePollSystem();

        UpdateLayout(GetScreenWidth(), GetScreenHeight());
        HandleKeyboard();

        // ---- input mouse ----
        Vector2 mouse = GetMousePosition();
        bool down = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        bool rel  = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

        for (int i = 0; i < buttonCount; i++) {
            Button* b = &buttons[i];
            b->hovered = CheckCollisionPointRec(mouse, b->rect);

            if (b->hovered && down) b->press = 1.0f;
            if (b->hovered && rel)  PressKey(b->text);

            if (b->press > 0.0f) {                // rilascio della pressione
                b->press -= dt * 5.0f;
                if (b->press < 0.0f) b->press = 0.0f;
            }
        }

        ExprToString(&expr, displayBuffer, sizeof(displayBuffer));

        // ---- animazione slide (indipendente dal framerate) ----
        float target = isResultState ? 1.0f : 0.0f;
        float k = dt * 7.0f;
        animSlide += (target - animSlide) * (k > 1.0f ? 1.0f : k);

        const Theme* t = ThemeGet();

        // ================= RENDER =================
        BeginDrawing();
        ClearBackground(BLANK);

        int sw = GetScreenWidth(), sh = GetScreenHeight();

        // sfondo app arrotondato, inset di 2px per non tagliare il bordo
        Rectangle appRec = { 2, 2, (float)sw - 4, (float)sh - 4 };
        DrawRectangleRounded(appRec, ROUND_APP, SEGM, t->bg);
        DrawRectangleRoundedLines(appRec, ROUND_APP, SEGM, 1.5f, t->outline);

        DrawDisplay(font, t);

        for (int i = 0; i < buttonCount; i++)
            DrawCalcButton(&buttons[i], font, t);

        if (showFps) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.0f FPS  %.2f ms", (float)GetFPS(), frameMsSmoothed);
            DrawRectangleRounded((Rectangle){ 10, 10, 160, 28 }, 0.4f, 8,
                                 (Color){ 0, 0, 0, (unsigned char)(t->dark ? 190 : 150) });
            DrawTextEx(font, buf, (Vector2){ 20, 18 }, 15, 1, WHITE);
        }

        EndDrawing();

        double frame = (GetTime() - t0) * 1000.0;
        frameMsSmoothed += (frame - frameMsSmoothed) * 0.1;
    }

    for (int i = 0; i < historyCount; i++) ExprClear(&history[i]);
    ExprClear(&expr);
    CloseWindow();
    return 0;
}
