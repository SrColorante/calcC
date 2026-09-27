#ifndef CONFIG_H
#define CONFIG_H

#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 600
#define FONT_SIZE 24
#define SMUSS 0.45f
#define SEGM 16

#include "raylib.h"
#include <stdbool.h>

// ==========================================
// 1. IMPORTAZIONI CONDIZIONALI (Includi solo il necessario)
// ==========================================
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h> // Incluso solo se sei su Windows
#elif defined(__APPLE__)
    #include <stdio.h>   // Inclusi solo se sei su macOS
    #include <string.h>
#elif defined(__linux__)
    #include <stdio.h>   // Inclusi solo se sei su Linux (es. ambiente GNOME)
    #include <string.h>
#endif

// ==========================================
// 2. LA FUNZIONE ADATTIVA PER IL TEMA
// ==========================================
static inline int IsSystemThemeDark() {
    
    // CASO WINDOWS
    #if defined(_WIN32) || defined(_WIN64)
        HKEY hKey;
        DWORD value = 1; // Default a Chiaro
        DWORD valueSize = sizeof(value);

        if (RegOpenKeyExA(HKEY_CURRENT_USER, 
                          "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 
                          0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegQueryValueExA(hKey, "AppsUseLightTheme", NULL, NULL, (LPBYTE)&value, &valueSize);
            RegCloseKey(hKey);
        }
        return (value == 0) ? 1 : 0; // 1 per Scuro, 0 per Chiaro
    
    // CASO MAC OS
    #elif defined(__APPLE__)
        char buffer[128] = { 0 };
        FILE *pipe = popen("defaults read -g AppleInterfaceStyle 2>/dev/null", "r");
        if (!pipe) return 0;

        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            pclose(pipe);
            return (strncmp(buffer, "Dark", 4) == 0) ? 1 : 0;
        }
        pclose(pipe);
        return 0;

    // CASO LINUX (Verifica basata su desktop GNOME/Ubuntu standard)
    #elif defined(__linux__)
        char buffer[128] = { 0 };
        FILE *pipe = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r");
        if (!pipe) return 0;

        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            pclose(pipe);
            // GNOME restituisce 'prefer-dark' se la Dark Mode è attiva
            return (strstr(buffer, "dark") != NULL) ? 1 : 0;
        }
        pclose(pipe);
        return 0;

    // CASO FALLBACK (Se l'OS non è tra questi, restituisce Chiaro di default)
    #else
        return 0; 
    #endif
}

#endif
