// popen/usleep sono estensioni POSIX: dichiararle esplicitamente
// evita che la compilazione si rompa con -std=c11 stretto.
#define _POSIX_C_SOURCE 200809L

#include "theme.h"
#include "../config.h"

#include <stdlib.h>
#include <strings.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
// Su Windows non c'e' pthread: la lettura del registro costa pochi
// microsecondi (non e' un popen), quindi si fa in linea nel main loop.
#define CALCC_HAS_THREAD 0
#else
#include <pthread.h>
#include <stdatomic.h>
#define CALCC_HAS_THREAD 1
#endif

// ============================================================
//  RILEVAMENTO TEMA DI SISTEMA
//  AVVIA UN PROCESSO (popen -> gsettings): costa ~5 ms su Linux.
//  Per questo non sta piu' nel main loop ma in un thread dedicato:
//  il frame non subisce mai uno stall, e il tema si aggiorna
//  comunque ogni THEME_POLL_SECONDS.
// ============================================================
bool DetectSystemThemeDark(void) {
#if defined(_WIN32) || defined(_WIN64)
    HKEY hKey;
    DWORD value = 1;                  // default = tema chiaro
    DWORD valueSize = sizeof(value);

    if (RegOpenKeyExA(HKEY_CURRENT_USER,
                      "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "AppsUseLightTheme", NULL, NULL, (LPBYTE)&value, &valueSize);
        RegCloseKey(hKey);
    }
    return (value == 0);

#elif defined(__APPLE__)
    char buffer[128] = { 0 };
    FILE *pipe = popen("defaults read -g AppleInterfaceStyle 2>/dev/null", "r");
    if (!pipe) return false;
    bool dark = false;
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) dark = (strncmp(buffer, "Dark", 4) == 0);
    pclose(pipe);
    return dark;

#elif defined(__linux__)
    char buffer[128] = { 0 };
    FILE *pipe = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r");
    if (!pipe) return false;
    bool dark = false;
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) dark = (strstr(buffer, "dark") != NULL);
    pclose(pipe);
    return dark;

#else
    return false;
#endif
}

// ============================================================
//  TEMA: letto una volta e tenuto in memoria.
//  Prima DetectSystemThemeDark() veniva chiamata 19 volte per
//  frame (570 ms/frame). Poi una volta ogni 10 s nel main thread
//  (uno stall di 5 ms ogni 10 s). Ora: zero stall.
// ============================================================
static Theme g_theme;
static bool  g_dark = true;
static bool  g_forced = false;
#if !CALCC_HAS_THREAD
static double g_nextPoll = 0.0;   // solo nella via senza thread
#endif

#if CALCC_HAS_THREAD
static atomic_bool g_workerRun = false;
static atomic_bool g_hasNew    = false;
static atomic_int  g_workerDark = 1;
static pthread_t   g_worker;
static bool        g_workerLive = false;

static void* ThemeWorker(void* unused) {
    (void)unused;
    while (atomic_load(&g_workerRun)) {
        int dark = DetectSystemThemeDark() ? 1 : 0;
        atomic_store(&g_workerDark, dark);
        atomic_store(&g_hasNew, true);

        // dorme a scatti brevi: se la finestra si chiude, il thread
        // esce entro 50 ms invece di aspettare 10 secondi interi
        const struct timespec nap = { 0, THEME_POLL_TICK_US*1000L };
        for (int i = 0; i < THEME_POLL_TICKS; i++) {
            if (!atomic_load(&g_workerRun)) return NULL;
            nanosleep(&nap, NULL);
        }
    }
    return NULL;
}
#endif

#define C(r, g, b) ((Color){ (r), (g), (b), 255 })

static void BuildDark(void) {
    Theme t;
    memset(&t, 0, sizeof(t));
    t.dark = true;

    t.bgTop    = C( 10,  12,  19);   // nero quasi blu, in alto
    t.bgBottom = C( 19,  24,  38);   // un filo piu' chiaro, in basso
    t.bgBorder = (Color){  52,  70, 112, 210 };
    t.bgGlow   = C( 47, 111, 228);   // alone blu dietro al display
    t.bgGlowStrength = 0.17f;
    t.bgVignette    = C(  4,   5,   9);

    t.displayTop    = C( 26,  31,  46);
    t.displayBottom = C( 17,  21,  30);
    t.displayBorder = C( 58,  72, 106);
    t.displayDot    = C( 92, 106, 140);
    t.displayShadow = C(  0,   0,   0);

    t.text   = C(233, 237, 247);
    t.textDim= C(133, 143, 166);
    t.result = C( 95, 227, 166);     // verde menta
    t.error  = C(255, 110, 110);
    t.caret  = C( 95, 227, 166);

    t.key.top = C( 38,  44,  60); t.key.bottom = C( 25,  30,  41);
    t.key.text = t.text;  t.key.highlight = 0.55f; t.key.shadow = 0.55f;
    t.key.border = (Color){ 255, 255, 255, 26 };  t.key.borderW = 1.0f;

    t.op.top = C( 42,  58,  90); t.op.bottom = C( 28,  38,  62);
    t.op.text = C(143, 192, 255);  t.op.highlight = 0.62f; t.op.shadow = 0.55f;
    t.op.border = (Color){ 160, 200, 255, 40 };  t.op.borderW = 1.0f;

    t.eq.top = C( 74, 147, 255); t.eq.bottom = C( 26,  84, 209);
    t.eq.text = C(255, 255, 255);  t.eq.highlight = 0.78f; t.eq.shadow = 0.80f;
    t.eq.border = (Color){ 255, 255, 255, 56 };  t.eq.borderW = 1.0f;
    t.eq.glowColor = C( 43, 108, 240); t.eq.glowStrength = 0.34f;

    t.danger.top = C( 60,  36,  48); t.danger.bottom = C( 38,  23,  31);
    t.danger.text = C(255, 154, 166);  t.danger.highlight = 0.50f; t.danger.shadow = 0.50f;
    t.danger.border = (Color){ 255, 160, 170, 38 };  t.danger.borderW = 1.0f;

    g_theme = t;
}

static void BuildLight(void) {
    Theme t;
    memset(&t, 0, sizeof(t));
    t.dark = false;

    t.bgTop    = C(253, 253, 255);
    t.bgBottom = C(233, 238, 247);
    t.bgBorder = (Color){ 198, 208, 226, 255 };
    t.bgGlow   = C(111, 160, 255);
    t.bgGlowStrength = 0.16f;
    t.bgVignette    = C(201, 210, 228);

    t.displayTop    = C(255, 255, 255);
    t.displayBottom = C(246, 248, 252);
    t.displayBorder = C(214, 222, 236);
    t.displayDot    = C(198, 207, 224);
    t.displayShadow = C( 30,  40,  70);

    t.text   = C( 20,  23,  31);
    t.textDim= C(108, 116, 136);
    t.result = C( 12, 155, 108);
    t.error  = C(214,  43,  63);
    t.caret  = C( 12, 155, 108);

    t.key.top = C(255, 255, 255); t.key.bottom = C(238, 241, 247);
    t.key.text = t.text;  t.key.highlight = 0.85f; t.key.shadow = 0.16f;
    t.key.border = (Color){ 120, 132, 158, 44 };  t.key.borderW = 1.0f;

    t.op.top = C(240, 245, 255); t.op.bottom = C(223, 232, 250);
    t.op.text = C( 26,  85, 192);  t.op.highlight = 0.70f; t.op.shadow = 0.16f;
    t.op.border = (Color){  60, 110, 200, 44 };  t.op.borderW = 1.0f;

    t.eq.top = C( 74, 147, 255); t.eq.bottom = C( 26,  84, 209);
    t.eq.text = C(255, 255, 255);  t.eq.highlight = 0.60f; t.eq.shadow = 0.32f;
    t.eq.border = (Color){ 255, 255, 255, 70 };  t.eq.borderW = 1.0f;
    t.eq.glowColor = C( 43, 108, 240); t.eq.glowStrength = 0.22f;

    t.danger.top = C(255, 239, 242); t.danger.bottom = C(253, 220, 226);
    t.danger.text = C(196,  30,  56);  t.danger.highlight = 0.60f; t.danger.shadow = 0.16f;
    t.danger.border = (Color){ 210,  70,  95, 44 };  t.danger.borderW = 1.0f;

    g_theme = t;
}

#undef C

static void Build(bool dark) {
    g_dark = dark;
    if (dark) BuildDark(); else BuildLight();
}

// Forza il tema da riga di comando: CALCC_THEME=light|dark.
// Utile per controllare le due palette senza toccare il sistema,
// e per i compositori che non espongono il tema (Wayland).
static bool EnvForcesDark(bool* forced) {
    const char* v = getenv("CALCC_THEME");
    *forced = false;
    if (!v || !*v) return true;
    if (!strcasecmp(v, "light")) { *forced = true; return false; }
    if (!strcasecmp(v, "dark"))  { *forced = true; return true;  }
    return true;
}

void ThemeInit(void) {
    bool forced = false;
    bool dark = EnvForcesDark(&forced);
    if (!forced) dark = DetectSystemThemeDark();   // ~5 ms, una volta sola
    Build(dark);
    g_forced = forced;

#if CALCC_HAS_THREAD
    atomic_store(&g_workerDark, dark ? 1 : 0);
    atomic_store(&g_hasNew, false);
    atomic_store(&g_workerRun, true);
    if (forced) { atomic_store(&g_workerRun, false); return; }  // tema imposto a mano
    if (pthread_create(&g_worker, NULL, ThemeWorker, NULL) == 0) g_workerLive = true;
    else {
        atomic_store(&g_workerRun, false);
        TraceLog(LOG_WARNING, "Tema: thread non creato, il tema non si aggiornera'");
    }
#endif
}

bool ThemePollSystem(void) {
#if CALCC_HAS_THREAD
    if (!atomic_load(&g_hasNew)) return false;      // costo ~0 nel caso normale
    bool dark = atomic_load(&g_workerDark) != 0;
    atomic_store(&g_hasNew, false);
    if (dark == g_dark) return false;
    Build(dark);
    return true;
#else
    // Senza thread (Windows) si rilegge il registro, che costa poco.
    // Con gsettings costerebbe 5 ms: mai nel loop di rendering.
    if (g_forced) return false;
    if (GetTime() < g_nextPoll) return false;
    g_nextPoll = GetTime() + THEME_POLL_SECONDS;
    bool dark = DetectSystemThemeDark();
    if (dark == g_dark) return false;
    Build(dark);
    return true;
#endif
}

void ThemeStop(void) {
#if CALCC_HAS_THREAD
    if (!g_workerLive) return;
    atomic_store(&g_workerRun, false);
    pthread_join(g_worker, NULL);
    g_workerLive = false;
#endif
}

bool ThemeIsDark(void) { return g_dark; }

const Theme* ThemeGet(void) { return &g_theme; }
