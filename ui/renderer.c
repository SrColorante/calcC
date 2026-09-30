#include "renderer.h"
#include <stdlib.h>
#include <string.h>

// ============================================================
//  SHADER SDF
//
//  Un solo vertex shader (identico a quello di raylib) e due
//  fragment shader: uno per i pannelli, uno per lo sfondo.
//
//  Per non passare 40 uniform per frame, tutti i parametri di una
//  forma viaggiano in un unico array uniform vec4[10] caricato con
//  una sola chiamata glUniform4fv. Cosi' ogni forma costa
//  2 chiamate GL (uniform + draw) invece di 4 primitive vettoriali.
// ============================================================

static const char* VS_SRC =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec2 vertexTexCoord;\n"
    "in vec4 vertexColor;\n"
    "out vec2 fragTexCoord;\n"
    "out vec4 fragColor;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "    fragTexCoord = vertexTexCoord;\n"
    "    fragColor = vertexColor;\n"
    "    gl_Position = mvp*vec4(vertexPosition, 1.0);\n"
    "}\n";

// ------------------------------------------------------------------
//  FRAGMENT PANEL
// ------------------------------------------------------------------
static const char* FS_PANEL_SRC =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform vec2 uFb;\n"
    "uniform vec4 uP[10];\n"
    "\n"
    "float sdBox(vec2 p, vec2 b, float r) {\n"
    "    r = min(r, min(b.x, b.y));\n"
    "    vec2 q = abs(p) - b + r;\n"
    "    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;\n"
    "}\n"
    "\n"
    "// src sopra dst, in alpha dritta (blend di raylib)\n"
    "vec4 over(vec4 dst, vec4 src) {\n"
    "    float a = src.a + dst.a*(1.0 - src.a);\n"
    "    if (a <= 0.0) return vec4(0.0);\n"
    "    vec3 c = (src.rgb*src.a + dst.rgb*dst.a*(1.0 - src.a))/a;\n"
    "    return vec4(c, a);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 frag = vec2(gl_FragCoord.x, uFb.y - gl_FragCoord.y);\n"
    "\n"
    "    vec4 R     = uP[0];                 // rect x,y,w,h\n"
    "    if (R.z <= 0.0 || R.w <= 0.0) discard;\n"
    "    vec4 S     = uP[1];                 // radius, borderW, highlight, shadow\n"
    "    vec4 cTop  = uP[2];\n"
    "    vec4 cBot  = uP[3];\n"
    "    vec4 cEdge = uP[4];\n"
    "    vec4 cShad = uP[5];\n"
    "    vec4 cGlow = uP[6];\n"
    "    vec4 G     = uP[7];                 // glowRadius, dotStrength, dotSpacing, -\n"
    "    vec4 cDot  = uP[8];\n"
    "    vec4 RIP   = uP[9];                 // cx, cy, raggio, forza\n"
    "\n"
    "    vec2  center = R.xy + R.zw*0.5;\n"
    "    vec2  hsize  = R.zw*0.5;\n"
    "    float d      = sdBox(frag - center, hsize, S.x);\n"
    "    float sd     = max(d, 0.0);\n"
    "\n"
    "    // niente da disegnare lontano dalla forma: la banda di ombra\n"
    "    // muore entro ~5 spread, li' l'alpha e' gia' 0\n"
    "    if (d > max(S.w, G.x)*5.0 + 4.0) discard;\n"
    "\n"
    "    float cov = clamp(0.5 - d, 0.0, 1.0);   // copertura anti-aliasing\n"
    "\n"
    "    // ---- ombra esterna (alpha decrescente con la distanza) ----\n"
    "    float shA = exp(-sd/max(S.w*0.55, 0.6))*cShad.a*S.w*(1.0 - cov);\n"
    "    vec4  outc = vec4(cShad.rgb, clamp(shA, 0.0, 1.0));\n"
    "\n"
    "    // ---- alone colorato ----\n"
    "    if (cGlow.a > 0.001) {\n"
    "        float glA = exp(-sd/max(G.x, 1.0))*cGlow.a*(1.0 - cov);\n"
    "        outc = over(outc, vec4(cGlow.rgb, clamp(glA, 0.0, 1.0)));\n"
    "    }\n"
    "\n"
    "    // ---- corpo con gradiente verticale ----\n"
    "    float t = clamp((frag.y - R.y)/max(R.w, 1.0), 0.0, 1.0);\n"
    "    t = t*t*(3.0 - 2.0*t);\n"
    "    vec4 body = mix(cTop, cBot, t);\n"
    "    body.rgb += pow(clamp(1.0 - t*2.4, 0.0, 1.0), 2.0)*S.z*0.09;\n"
    "    outc = over(outc, vec4(body.rgb, body.a*cov));\n"
    "\n"
    "    // ---- banda speculare sul bordo superiore (effetto vetro) ----\n"
    "    float inner = 1.0 - smoothstep(0.0, 1.7, -d);\n"
    "    float topM  = clamp(1.0 - (frag.y - R.y)/max(R.w*0.45, 1.0), 0.0, 1.0);\n"
    "    outc = over(outc, vec4(vec3(1.0), pow(topM, 1.7)*inner*S.z*0.30*cov));\n"
    "\n"
    "    // ---- griglia di puntini ----\n"
    "    if (G.y > 0.001 && cov > 0.0) {\n"
    "        vec2  g = mod(frag - R.xy, G.z) - G.z*0.5;\n"
    "        float dd = exp(-dot(g, g)*0.75);\n"
    "        float dep = clamp((-d - 6.0)/18.0, 0.0, 1.0);\n"
    "        outc = over(outc, vec4(cDot.rgb, dd*cDot.a*G.y*dep*cov));\n"
    "    }\n"
    "\n"
    "    // ---- onda di pressione ----\n"
    "    if (RIP.w > 0.001) {\n"
    "        float dr   = length(frag - (R.xy + RIP.xy));\n"
    "        float ring = exp(-pow((dr - RIP.z)/3.5, 2.0));\n"
    "        outc = over(outc, vec4(vec3(1.0), ring*RIP.w*0.26*cov));\n"
    "    }\n"
    "\n"
    "    // ---- bordo interno ----\n"
    "    if (cEdge.a > 0.001) {\n"
    "        float ring = 1.0 - smoothstep(S.y - 0.75, S.y + 0.75, abs(d + S.y*0.5));\n"
    "        outc = over(outc, vec4(cEdge.rgb, cEdge.a*ring*cov));\n"
    "    }\n"
    "\n"
    "    finalColor = outc;\n"
    "}\n";

// ------------------------------------------------------------------
//  FRAGMENT SFONDO
// ------------------------------------------------------------------
static const char* FS_BG_SRC =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform vec2 uFb;\n"
    "uniform vec4 uB[8];\n"
    "\n"
    "float sdBox(vec2 p, vec2 b, float r) {\n"
    "    r = min(r, min(b.x, b.y));\n"
    "    vec2 q = abs(p) - b + r;\n"
    "    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;\n"
    "}\n"
    "\n"
    "vec4 over(vec4 dst, vec4 src) {\n"
    "    float a = src.a + dst.a*(1.0 - src.a);\n"
    "    if (a <= 0.0) return vec4(0.0);\n"
    "    vec3 c = (src.rgb*src.a + dst.rgb*dst.a*(1.0 - src.a))/a;\n"
    "    return vec4(c, a);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 frag = vec2(gl_FragCoord.x, uFb.y - gl_FragCoord.y);\n"
    "\n"
    "    vec4 R    = uB[0];   // rect\n"
    "    vec4 S    = uB[1];   // radius, borderW\n"
    "    vec4 cTop = uB[2];\n"
    "    vec4 cBot = uB[3];\n"
    "    vec4 cEdg = uB[4];\n"
    "    vec4 GL   = uB[5];   // centro alone x,y (0..1), raggio, forza\n"
    "    vec4 cGl  = uB[6];\n"
    "    vec4 cVig = uB[7];   // colore + forza vignettatura\n"
    "\n"
    "    vec2  center = R.xy + R.zw*0.5;\n"
    "    float d      = sdBox(frag - center, R.zw*0.5, S.x);\n"
    "    float cov    = clamp(0.5 - d, 0.0, 1.0);\n"
    "    if (cov <= 0.0) discard;\n"
    "\n"
    "    vec2 uv = (frag - R.xy)/R.zw;\n"
    "\n"
    "    // gradiente diagonale: la luce arriva dall'alto a sinistra\n"
    "    float t = clamp(uv.y*0.80 + (1.0 - uv.x)*0.20, 0.0, 1.0);\n"
    "    vec3  col = mix(cTop.rgb, cBot.rgb, t*t*(3.0 - 2.0*t));\n"
    "\n"
    "    // alone morbido dietro al display\n"
    "    vec2  g = uv - GL.xy;\n"
    "    g.x *= R.z/R.w;\n"
    "    float gl = exp(-dot(g, g)/max(GL.z*GL.z, 1e-5));\n"
    "    col += cGl.rgb*gl*GL.w;\n"
    "\n"
    "    // vignettatura sui bordi\n"
    "    vec2  vc = (uv - vec2(0.5, 0.40))*vec2(1.0, 0.85);\n"
    "    float vg = smoothstep(0.42, 1.15, length(vc));\n"
    "    col = mix(col, cVig.rgb, vg*cVig.a);\n"
    "\n"
    "    vec4 outc = vec4(col, cTop.a*cov);\n"
    "    if (cEdg.a > 0.001) {\n"
    "        float ring = 1.0 - smoothstep(S.y - 0.75, S.y + 0.75, abs(d + S.y*0.5));\n"
    "        outc = over(outc, vec4(cEdg.rgb, cEdg.a*ring*cov));\n"
    "    }\n"
    "    finalColor = outc;\n"
    "}\n";

// ============================================================
//  STATO
// ============================================================
static Shader  g_panel, g_bg;
static int     g_uPanel = -1, g_uBg = -1;
static int     g_uFbPanel = -1, g_uFbBg = -1;   // cache: GetShaderLocation
static bool    g_ok = false;
static Vector2 g_fb = { 1, 1 };

static void Vec4ToF(float* dst, Color c) {
    dst[0] = (float)c.r/255.0f; dst[1] = (float)c.g/255.0f;
    dst[2] = (float)c.b/255.0f; dst[3] = (float)c.a/255.0f;
}

void RendererInit(void) {
    // CALCC_NOSHADER=1 collauda la via di riserva senza toccare la GPU
    const char* noShader = getenv("CALCC_NOSHADER");
    if (noShader && *noShader && *noShader != '0') {
        TraceLog(LOG_WARNING, "Renderer: shader disattivati (CALCC_NOSHADER), uso le primitive");
        g_ok = false;
        return;
    }

    g_panel = LoadShaderFromMemory(VS_SRC, FS_PANEL_SRC);
    g_bg    = LoadShaderFromMemory(VS_SRC, FS_BG_SRC);

    // Se uno dei due non ha prodotto un programma valido, la GPU non
    // sa eseguire i nostro shader: si torna alle primitive vettoriali.
    g_ok = (g_panel.id > 0 && g_panel.locs != NULL &&
            g_bg.id    > 0 && g_bg.locs    != NULL);

    if (!g_ok) {
        TraceLog(LOG_WARNING, "Renderer: shader non disponibili, uso le primitive raylib");
        return;
    }

    g_uPanel    = GetShaderLocation(g_panel, "uP[0]");
    g_uBg       = GetShaderLocation(g_bg,    "uB[0]");
    g_uFbPanel  = GetShaderLocation(g_panel, "uFb");
    g_uFbBg     = GetShaderLocation(g_bg,    "uFb");

    if (g_uPanel < 0 || g_uBg < 0) {
        TraceLog(LOG_WARNING, "Renderer: uniform non trovati, uso le primitive raylib");
        g_ok = false;
    }
}

void RendererShutdown(void) {
    if (g_ok) {
        UnloadShader(g_panel);
        UnloadShader(g_bg);
    }
    g_ok = false;
}

bool RendererAvailable(void) { return g_ok; }

void RendererSetFramebuffer(int width, int height) {
    if (width == (int)g_fb.x && height == (int)g_fb.y) return;
    g_fb.x = (float)width;
    g_fb.y = (float)height;
}

// ============================================================
//  DISEGNO
// ============================================================

// Fallback: niente ombre morbide ne' gradienti, ma la forma c'e'.
// Attenzione: in raylib `roundness` e' una frazione del lato MINORE e
// il raggio reale ne e' la meta', quindi roundness = 1.0 e' la
// pillola piena (era il vecchio ROUND_BUTTON).
static float RoundnessOf(Rectangle r, float radius) {
    float side = (r.width < r.height) ? r.width : r.height;
    if (side <= 0.0f) return 0.0f;
    float round = 2.0f*radius/side;
    return (round > 1.0f) ? 1.0f : round;
}

// Fallback: niente ombre morbide ne' gradienti, ma la forma c'e'.
static void DrawPanelFallback(const Panel* p) {
    float round = RoundnessOf(p->rect, p->radius);

    Color mid = { (unsigned char)(((int)p->top.r   + (int)p->bottom.r)/2),
                  (unsigned char)(((int)p->top.g   + (int)p->bottom.g)/2),
                  (unsigned char)(((int)p->top.b   + (int)p->bottom.b)/2),
                  (unsigned char)(((int)p->top.a   + (int)p->bottom.a)/2) };

    if (p->shadow > 0.0f) {
        Rectangle sh = p->rect;
        sh.y += p->rect.height*0.05f;
        DrawRectangleRounded(sh, round, 16, p->shadowColor);
    }
    DrawRectangleRounded(p->rect, round, 16, mid);
    if (p->border.a > 0) {
        Rectangle in = p->rect;
        in.x += 1.0f; in.y += 1.0f;
        in.width -= 2.0f; in.height -= 2.0f;
        DrawRectangleRoundedLines(in, round, 16, 1.0f, p->border);
    }
}

void DrawPanel(const Panel* p) {
    if (!g_ok || p->rect.width <= 0.0f || p->rect.height <= 0.0f) {
        if (p->rect.width > 0.0f && p->rect.height > 0.0f) DrawPanelFallback(p);
        return;
    }

    float u[40];
    memset(u, 0, sizeof(u));

    u[0]  = p->rect.x;      u[1]  = p->rect.y;
    u[2]  = p->rect.width;  u[3]  = p->rect.height;
    u[4]  = p->radius;      u[5]  = p->borderW;
    u[6]  = p->highlight;  u[7]  = p->shadow;
    Vec4ToF(u + 8,  p->top);
    Vec4ToF(u + 12, p->bottom);
    Vec4ToF(u + 16, p->border);
    Vec4ToF(u + 20, p->shadowColor);
    Vec4ToF(u + 24, p->glow);
    u[28] = p->glowRadius;  u[29] = p->dotStrength;
    u[30] = p->dotSpacing;  u[31] = 0.0f;
    Vec4ToF(u + 32, p->dotColor);
    u[36] = p->rippleX;     u[37] = p->rippleY;
    u[38] = p->rippleR;     u[39] = p->rippleA;

    // Il quad deve contenere ombra e alone, altrimenti vengono tagliati.
    float margin = (p->shadow > 0.0f ? p->shadowSpread*3.0f : 0.0f) +
                   (p->glow.a > 0 ? p->glowRadius*4.0f : 0.0f) + 2.0f;
    Rectangle quad = { p->rect.x - margin, p->rect.y - margin,
                       p->rect.width + 2*margin, p->rect.height + 2*margin };

    BeginShaderMode(g_panel);
    SetShaderValue(g_panel, g_uFbPanel, &g_fb, SHADER_UNIFORM_VEC2);
    SetShaderValueV(g_panel, g_uPanel, u, SHADER_UNIFORM_VEC4, 10);
    DrawRectangleRec(quad, WHITE);
    EndShaderMode();     // rlSetShader forza il flush con gli uniform giusti
}

void DrawBackground(const Background* b) {
    if (!g_ok || b->panel.width <= 0.0f || b->panel.height <= 0.0f) {
        if (b->panel.width > 0.0f && b->panel.height > 0.0f) {
            float round = RoundnessOf(b->panel, b->radius);
            DrawRectangleRounded(b->panel, round, 16, b->top);
            if (b->border.a > 0) DrawRectangleRoundedLines(b->panel, round, 16, 1.0f, b->border);
        }
        return;
    }

    // uB[0]=rect  uB[1]=(radius,borderW)  uB[2]=top  uB[3]=bottom
    // uB[4]=border  uB[5]=alone  uB[6]=colore alone  uB[7]=vignettatura
    float u[32];
    memset(u, 0, sizeof(u));

    u[0]  = b->panel.x;     u[1]  = b->panel.y;
    u[2]  = b->panel.width; u[3]  = b->panel.height;
    u[4]  = b->radius;      u[5]  = b->borderW;
    Vec4ToF(u + 8,  b->top);
    Vec4ToF(u + 12, b->bottom);
    Vec4ToF(u + 16, b->border);
    u[20] = b->glowCenter.x; u[21] = b->glowCenter.y;
    u[22] = b->glowRadius;   u[23] = b->glowStrength;
    Vec4ToF(u + 24, b->glowColor);
    Vec4ToF(u + 28, b->vignetteColor);
    u[31] = b->vignette;        // forza della vignettatura

    BeginShaderMode(g_bg);
    SetShaderValue(g_bg, g_uFbBg, &g_fb, SHADER_UNIFORM_VEC2);
    SetShaderValueV(g_bg, g_uBg, u, SHADER_UNIFORM_VEC4, 8);
    DrawRectangleRec(b->panel, WHITE);
    EndShaderMode();
}
