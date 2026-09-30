#include "theme.h"
#include "../config.h"

// ============================================================
//  RILEVAMENTO TEMA DI SISTEMA
//  AVVIA UN PROCESSO (popen / registro di sistema): costa ~30 ms
//  su Linux. Per questo vive qui e viene chiamato al massimo una
//  volta ogni THEME_POLL_SECONDS, mai dentro il loop di disegno.
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
//  TEMA: una volta letto dal sistema resta in memoria.
//  Prima DetectSystemThemeDark() veniva chiamata 19 volte per
//  frame (1 dal main + 18 dai bottoni): 19 * 30 ms = 570 ms/frame.
// ============================================================
static Theme g_theme;
static bool   g_dark = true;
static double g_nextPoll = 0.0;

static void BuildDark(void) {
    Theme t = {0};
    t.dark = true;

    t.bg          = (Color){ 10, 11, 16, 255 };   // nero quasi blu
    t.outline     = (Color){  0, 122, 204, 255 }; // blu elettrico
    t.shadow      = (Color){  0,  0,   0,  90 };

    t.displayBg   = (Color){ 20, 22, 30, 255 };
    t.displayLine = (Color){ 42, 46, 60, 255 };
    t.displayDot  = (Color){ 70, 78, 98, 255 };

    t.text        = (Color){ 236, 238, 245, 255 };
    t.textDim     = (Color){ 138, 146, 168, 255 };
    t.result      = (Color){ 126, 231, 135, 255 }; // verde risultato

    t.keyBg       = (Color){ 30, 33, 43, 255 };
    t.keyBgHover  = (Color){ 44, 48, 61, 255 };
    t.keyText     = (Color){ 236, 238, 245, 255 };

    t.opBg        = (Color){ 38, 45, 66, 255 };   // tonalita' fredda
    t.opBgHover   = (Color){ 52, 63, 90, 255 };
    t.opText      = (Color){ 150, 200, 255, 255 };

    t.eqBg        = (Color){  0, 122, 204, 255 }; // primary = blu
    t.eqBgHover   = (Color){ 32, 152, 235, 255 };
    t.eqText      = (Color){ 255, 255, 255, 255 };

    t.dangerBg    = (Color){ 48, 30, 38, 255 };   // C = rosso
    t.dangerBgHover= (Color){ 70, 42, 54, 255 };
    t.dangerText  = (Color){ 255, 138, 138, 255 };

    g_theme = t;
}

static void BuildLight(void) {
    Theme t = {0};
    t.dark = false;

    t.bg          = (Color){ 250, 250, 252, 255 };
    t.outline     = (Color){ 214, 30, 74, 255 };  // crimson
    t.shadow      = (Color){  20,  20,  35, 45 };

    t.displayBg   = (Color){ 255, 255, 255, 255 };
    t.displayLine = (Color){ 226, 228, 236, 255 };
    t.displayDot  = (Color){ 228, 230, 238, 255 };

    t.text        = (Color){  20,  22,  28, 255 };
    t.textDim     = (Color){ 120, 126, 142, 255 };
    t.result      = (Color){  16, 124,  64, 255 };

    t.keyBg       = (Color){ 255, 255, 255, 255 };
    t.keyBgHover  = (Color){ 236, 238, 244, 255 };
    t.keyText     = (Color){  20,  22,  28, 255 };

    t.opBg        = (Color){ 240, 244, 252, 255 };
    t.opBgHover   = (Color){ 226, 234, 248, 255 };
    t.opText      = (Color){  20,  80, 170, 255 };

    t.eqBg        = (Color){ 214,  30,  74, 255 }; // primary = crimson
    t.eqBgHover   = (Color){ 236,  60, 100, 255 };
    t.eqText      = (Color){ 255, 255, 255, 255 };

    t.dangerBg    = (Color){ 255, 238, 240, 255 };
    t.dangerBgHover= (Color){ 255, 224, 228, 255 };
    t.dangerText  = (Color){ 200,  30,  60, 255 };

    g_theme = t;
}

static void Build(bool dark) {
    g_dark = dark;
    if (dark) BuildDark(); else BuildLight();
}

void ThemeInit(void) {
    g_dark = DetectSystemThemeDark();   // ~30 ms, ma solo una volta
    Build(g_dark);
    g_nextPoll = 0.0;                   // primo poll al frame successivo
}

bool ThemePollSystem(void) {
    double now = GetTime();
    if (now < g_nextPoll) return false;
    g_nextPoll = now + THEME_POLL_SECONDS;

    bool dark = DetectSystemThemeDark();
    if (dark != g_dark) { Build(dark); return true; }
    return false;
}

bool ThemeIsDark(void) { return g_dark; }

const Theme* ThemeGet(void) { return &g_theme; }
