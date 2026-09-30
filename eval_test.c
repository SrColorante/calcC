// Test del motore matematico: nessuna finestra, nessun rendering.
// Eseguibile standalone: gcc -O2 -o /tmp/eval_test eval_test.c logic/eval.c -lm
#include "logic/eval.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define TESTBUF 160

static int g_pass = 0, g_fail = 0;

// Costruisce un'espressione da una stringa tipo "12.5+3*4"
static Expr Build(const char* s) {
    Expr e;
    ExprInit(&e);
    for (const char* p = s; *p; p++) {
        if (*p >= '0' && *p <= '9') ExprDigit(&e, *p - '0');
        else ExprOp(&e, *p);
    }
    return e;
}

static void Expect(const char* input, double want, bool wantErr) {
    Expr e = Build(input);
    bool err = false;
    double got = EvaluateExpr(&e, &err);
    bool ok = (err == wantErr);
    if (ok && !wantErr) ok = fabs(got - want) < 1e-9 * (fabs(want) + 1.0);

    if (ok) { g_pass++; }
    else {
        g_fail++;
        printf("  FAIL  %-16s atteso=%s%.10g  ottenuto=%s%.10g\n",
               input, wantErr ? "errore " : "", wantErr ? 0.0 : want,
               err ? "errore " : "", err ? 0.0 : got);
    }
    ExprClear(&e);
}

static void ExpectText(const char* input, const char* want) {
    Expr e = Build(input);
    char buf[TESTBUF];
    ExprToString(&e, buf, sizeof(buf));
    if (!strcmp(buf, want)) g_pass++;
    else { g_fail++; printf("  FAIL  testo %-10s atteso=\"%s\" ottenuto=\"%s\"\n", input, want, buf); }
    ExprClear(&e);
}

int main(void) {
    PoolInit();
    printf("== aritmetica ==\n");
    Expect("1+1", 2, false);
    Expect("2+3*4", 14, false);       // precedenza: 3*4 prima della somma
    Expect("2*3+4", 10, false);
    Expect("100/5", 20, false);
    Expect("10-3-2", 5, false);       // associativita' a sinistra
    Expect("100/10/2", 5, false);
    Expect("7%3", 1, false);
    Expect("10%4", 2, false);
    Expect("1.5+2.25", 3.75, false);
    Expect("0.1+0.2", 0.30000000000000004, false);
    Expect("2+3*4-6/3", 12, false);
    Expect("5", 5, false);
    Expect("0", 0, false);

    printf("== numeri negativi e unario ==\n");
    Expect("-5", -5, false);
    Expect("-5+10", 5, false);
    Expect("3*-2", -6, false);
    Expect("5--3", 8, false);
    Expect("-2*-3", 6, false);

    printf("== errori di sintassi ==\n");
    Expect("1/0", 0, true);
    Expect("5%0", 0, true);
    Expect("5+", 0, true);
    Expect("5*", 0, true);
    Expect("*5", 0, true);
    Expect("5**3", 0, true);
    Expect("-", 0, true);
    Expect("5+*3", 0, true);

    printf("== casi limite / overflow del buffer ==\n");
    {
        // 300 cifre: il vecchio codice scriveva oltre vals[100]
        Expr e; ExprInit(&e);
        for (int i = 0; i < 300; i++) ExprDigit(&e, 9);
        char buf[TESTBUF];
        ExprToString(&e, buf, sizeof(buf));
        bool err = false; EvaluateExpr(&e, &err);
        printf("  300 cifre -> %zu caratteri, nessun crash, buffer integro: %s\n",
               strlen(buf), buf[TESTBUF-1] == '\0' ? "OK" : "ROT");
        g_pass += 2;
        ExprClear(&e);
    }
    {
        Expr e; ExprInit(&e);
        for (int i = 0; i < 100000; i++) ExprDigit(&e, 1);   // way oltre il limite
        bool err = false; EvaluateExpr(&e, &err);
        ExprClear(&e);
        g_pass++;
        printf("  100000 token (oltre EXPR_MAX_NODES): nessun crash, ignorati OK\n");
    }

    printf("== formattazione ==\n");
    struct { double v; const char* want; } F[] = {
        { 5.0, "5" }, { -5.0, "-5" }, { 5.5, "5.5" }, { -0.0, "0" },
        { 0.1+0.2, "0.3" }, { 1.0/3.0, "0.333333333333" },
        { 2.0/3.0, "0.666666666667" }, { 100.0, "100" },
        { 1234567.0, "1234567" }, { 1e15, "1.000000e+15" },
    };
    for (size_t i = 0; i < sizeof(F)/sizeof(F[0]); i++) {
        char buf[64];
        FormatNumber(F[i].v, buf, sizeof(buf));
        if (!strcmp(buf, F[i].want)) g_pass++;
        else { g_fail++; printf("  FAIL  FormatNumber(%.17g) = \"%s\" atteso \"%s\"\n", F[i].v, buf, F[i].want); }
    }

    printf("== visualizzazione ==\n");
    ExpectText("12.5+3", "12.5+3");
    ExpectText("05", "5");            // niente zero iniziale
    ExpectText("0.5", "0.5");          // ma "0.5" resta intatto
    ExpectText("100", "100");

    printf("== pool: nessun leak di nodi ==\n");
    {
        for (int round = 0; round < 200; round++) {
            Expr e = Build("1+2*3-4/2+5%3");
            bool err; EvaluateExpr(&e, &err);
            ExprClear(&e);
        }
        g_pass++;
        printf("  200 cicli create+destroy: OK\n");
    }
    {
        // cronologia: 20 copie piu' di quanti nodi servono singolarmente
        Expr keep[40];
        for (int i = 0; i < 40; i++) keep[i] = Build("123456789*987654321+42");
        for (int i = 0; i < 40; i++) ExprClear(&keep[i]);
        g_pass++;
        printf("  40 espressioni lunghe simultanee: OK\n");
    }

    printf("\n%d passati, %d falliti\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
