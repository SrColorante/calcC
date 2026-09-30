// Micro-benchmark: costo di IsSystemThemeDark() (popen -> gsettings) vs costo layout/draw
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <string.h>

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static int IsSystemThemeDark_linux(void) {
    char buffer[128] = { 0 };
    FILE *pipe = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r");
    if (!pipe) return 0;
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) { pclose(pipe); return (strstr(buffer, "dark") != NULL) ? 1 : 0; }
    pclose(pipe);
    return 0;
}

int main(void) {
    const int N = 100;

    // 1) singola chiamata tema
    double t0 = now_ms();
    int dark = 0;
    for (int i = 0; i < N; i++) dark += IsSystemThemeDark_linux();
    double t1 = now_ms();
    printf("IsSystemThemeDark() x%d  : %8.3f ms total | %6.3f ms PER CHIAMATA (dark=%d)\n",
           N, t1 - t0, (t1 - t0) / N, dark);

    // 2) quante volte viene chiamata in UN FRAME (1 in main + 1 per ognuno dei 18 bottoni)
    const int per_frame = 1 + 18;
    printf("\nChiamate per FRAME oggi   : %d\n", per_frame);
    printf("Costo tema per FRAME      : %6.3f ms  <-- budget di un frame a 60fps = 16.67 ms\n",
           (t1 - t0) / N * per_frame);
    printf("Frame rate teorico massimo: %6.1f fps\n", 1000.0 / ((t1 - t0) / N * per_frame));

    // 3) confronto: lettura cached (una volta sola)
    t0 = now_ms();
    for (int i = 0; i < 1000000; i++) dark += (i & 1);
    t1 = now_ms();
    printf("\n1e6 iterazioni di un semplice test booleano: %6.3f ms  -> cache del tema ~0 costo\n", t1 - t0);
    return 0;
}
