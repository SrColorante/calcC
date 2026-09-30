// Benchmark del motore: confronta il parser nuovo (shunting-yard, pool)
// con l'implementazione originale a liste concatenate + malloc,
// usando lo stesso carico di lavoro reale (digitazione + valutazione).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

// ================= IMPLEMENTAZIONE ORIGINALE (copia fedele) =========
typedef enum { ONODE_NUM, ONODE_OP } ONodeType;
typedef struct ONode {
    ONodeType type;
    union { int n; char op; };
    struct ONode* next;
} ONode;

static void OAppendOp(ONode** head, char op) {
    ONode* n = (ONode*)malloc(sizeof(ONode));
    n->type = ONODE_OP; n->op = op; n->next = NULL;
    if (!*head) *head = n;
    else { ONode* c = *head; while (c->next) c = c->next; c->next = n; }
}
static void OAppendNum(ONode** head, int num) {
    ONode* n = (ONode*)malloc(sizeof(ONode));
    n->type = ONODE_NUM; n->n = num; n->next = NULL;
    if (!*head) *head = n;
    else { ONode* c = *head; while (c->next) c = c->next; c->next = n; }
}
static void OClear(ONode** head) {
    ONode* c = *head;
    while (c) { ONode* nx = c->next; free(c); c = nx; }
    *head = NULL;
}
static void OToString(ONode* head, char* buffer, int max_len) {
    buffer[0] = '\0';
    ONode* c = head;
    while (c) {
        char t[32];
        if (c->type == ONODE_NUM) snprintf(t, sizeof t, "%d", c->n);
        else snprintf(t, sizeof t, "%c", c->op);
        if (strlen(buffer) + strlen(t) < (size_t)max_len) strcat(buffer, t);
        c = c->next;
    }
}
static double OEval(ONode* head, int* err) {
    *err = 0;
    if (!head) return 0;
    double vals[100]; char ops[100]; int v = 0, o = 0;
    ONode* c = head;
    while (c) {
        if (c->type == ONODE_NUM || (c->type == ONODE_OP && c->op == '.')) {
            double val = 0, mult = 1; int dec = 0;
            while (c && (c->type == ONODE_NUM || (c->type == ONODE_OP && c->op == '.'))) {
                if (c->type == ONODE_OP && c->op == '.') dec = 1;
                else if (!dec) val = val * 10 + c->n;
                else { mult /= 10.0; val += c->n * mult; }
                c = c->next;
            }
            vals[v++] = val;
        } else { ops[o++] = c->op; c = c->next; }
    }
    if (v == 0 && o > 0) { *err = 1; return 0; }
    if (o >= v) { *err = 1; return 0; }
    for (int i = 0; i < o; i++) {
        if (ops[i]=='*'||ops[i]=='/'||ops[i]=='%') {
            if (ops[i]=='*') vals[i]=vals[i]*vals[i+1];
            if (ops[i]=='/') { if(!vals[i+1]){*err=1;return 0;} vals[i]=vals[i]/vals[i+1]; }
            if (ops[i]=='%') { if(!vals[i+1]){*err=1;return 0;} vals[i]=fmod(vals[i],vals[i+1]); }
            for (int j=i+1;j<v-1;j++) vals[j]=vals[j+1];
            for (int j=i;j<o-1;j++) ops[j]=ops[j+1];
            v--; o--; i--;
        }
    }
    for (int i = 0; i < o; i++) {
        if (ops[i]=='+'||ops[i]=='-') {
            if (ops[i]=='+') vals[i]=vals[i]+vals[i+1];
            if (ops[i]=='-') vals[i]=vals[i]-vals[i+1];
            for (int j=i+1;j<v-1;j++) vals[j]=vals[j+1];
            for (int j=i;j<o-1;j++) ops[j]=ops[j+1];
            v--; o--; i--;
        }
    }
    return v > 0 ? vals[0] : 0;
}

// ================= IMPLEMENTAZIONE NUOVA =========================
#include "logic/eval.h"

// Espressione di prova: 40 token, lunga ma realistica
static const char* SAMPLE = "123456789*987654321+42.5/3-17%5+88*2-31.25+6";

static void bench_old(int iters) {
    char buf[160];
    double sink = 0;
    double t0 = now_ms();
    for (int it = 0; it < iters; it++) {
        ONode* e = NULL;
        for (const char* p = SAMPLE; *p; p++) {
            if (*p >= '0' && *p <= '9') OAppendNum(&e, *p - '0');
            else OAppendOp(&e, *p);
        }
        OToString(e, buf, sizeof buf);          // una volta per frame, come in main.c
        int err; sink += OEval(e, &err);
        OClear(&e);
    }
    double t1 = now_ms();
    printf("  ORIGINALE (malloc + lista + doppio collapse O(n^2)) : %8.2f ms  (%d cicli)  sink=%.1f\n",
           t1 - t0, iters, sink);
}

static void bench_new(int iters) {
    char buf[160];
    double sink = 0;
    double t0 = now_ms();
    for (int it = 0; it < iters; it++) {
        Expr e; ExprInit(&e);
        for (const char* p = SAMPLE; *p; p++) {
            if (*p >= '0' && *p <= '9') ExprDigit(&e, *p - '0');
            else ExprOp(&e, *p);
        }
        ExprToString(&e, buf, sizeof buf);
        bool err; sink += EvaluateExpr(&e, &err);
        ExprClear(&e);
    }
    double t1 = now_ms();
    printf("  NUOVO      (pool + coda O(1) + shunting-yard)        : %8.2f ms  (%d cicli)  sink=%.1f\n",
           t1 - t0, iters, sink);
}

int main(void) {
    PoolInit();
    const int N = 20000;

    printf("== costruzione + visualizzazione + valutazione, %d cicli su \"%s\" ==\n", N, SAMPLE);
    bench_old(N);
    bench_new(N);

    // Scalabilita': espressioni sempre piu' lunghe
    printf("\n== scalabilita' (cresce la lunghezza dell'espressione) ==\n");
    printf("  %-10s %14s %14s %10s\n", "token", "originale(ms)", "nuovo(ms)", "speedup");
    for (int len = 10; len <= 40; len += 10) {
        char big[512] = {0};
        for (int i = 0; i < len; i++) {
            big[i] = (char)('0' + (i % 9) + 1);
            if (i + 1 < len) big[i+1] = '+';
        }
        const int it = 5000;

        double t0 = now_ms();
        for (int k = 0; k < it; k++) {
            ONode* e = NULL;
            for (const char* p = big; *p; p++) { if (*p>='0'&&*p<='9') OAppendNum(&e,*p-'0'); else OAppendOp(&e,*p); }
            int err; OEval(e,&err); OClear(&e);
        }
        double t1 = now_ms();

        double t2 = now_ms();
        for (int k = 0; k < it; k++) {
            Expr e; ExprInit(&e);
            for (const char* p = big; *p; p++) { if (*p>='0'&&*p<='9') ExprDigit(&e,*p-'0'); else ExprOp(&e,*p); }
            bool err; EvaluateExpr(&e,&err); ExprClear(&e);
        }
        double t3 = now_ms();

        double old_ms = (t1-t0)/it*1000, new_ms = (t3-t2)/it*1000;
        printf("  %-10d %14.3f %14.3f %9.1fx\n", len, old_ms, new_ms, old_ms/(new_ms>1e-9?new_ms:1e-9));
    }
    return 0;
}
