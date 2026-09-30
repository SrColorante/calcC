#include "eval.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================
//  POOL DI NODI
//  Prima ogni token dell'espressione era un malloc() e la
//  cronologia faceva deep-copy -> malloc/freecontinui.
//  Ora i nodi vengono presi da un array statico con free-list:
//  allocazione O(1), nessuna frammentazione, nessun syscall.
// ============================================================
static ExprNode g_pool[NODE_POOL_SIZE];
static ExprNode* g_freeList = NULL;
static int g_liveNodes = 0;

void PoolInit(void) {
    for (int i = 0; i < NODE_POOL_SIZE - 1; i++) {
        g_pool[i].next = &g_pool[i + 1];
    }
    g_pool[NODE_POOL_SIZE - 1].next = NULL;
    g_freeList = &g_pool[0];
    g_liveNodes = 0;
}

static ExprNode* NodeAlloc(void) {
    ExprNode* n = g_freeList;
    if (n) {
        g_freeList = n->next;
        g_liveNodes++;
    }
    return n;
}

static void NodeFree(ExprNode* n) {
    n->next = g_freeList;
    g_freeList = n;
    g_liveNodes--;
}

// ============================================================
//  COSTRUZIONE / DISTRUZIONE  (tutte O(1) o O(n) lineare)
// ============================================================
void ExprInit(Expr* e) { e->head = NULL; e->tail = NULL; e->count = 0; }

void ExprClear(Expr* e) {
    ExprNode* curr = e->head;
    while (curr) {
        ExprNode* next = curr->next;
        NodeFree(curr);
        curr = next;
    }
    e->head = NULL;
    e->tail = NULL;
    e->count = 0;
}

static ExprNode* PushNode(Expr* e, NodeType type, double val) {
    ExprNode* n = NodeAlloc();
    if (!n) return NULL;               // pool esaurito: ignora (nessun crash)
    n->type = type;
    n->val  = val;
    n->next = NULL;
    if (e->tail) e->tail->next = n;    // O(1): append in coda
    else         e->head = n;
    e->tail = n;
    e->count++;
    return n;
}

void ExprDigit(Expr* e, int digit) {
    if (!e || digit < 0 || digit > 9) return;
    if (e->count >= EXPR_MAX_NODES) return;      // O(1), non piu' una scansione
    PushNode(e, NODE_DIGIT, (double)digit);
}

void ExprValue(Expr* e, double v) {
    if (!e) return;
    if (e->count >= EXPR_MAX_NODES) return;
    PushNode(e, NODE_VALUE, v);
}

void ExprOp(Expr* e, char op) {
    if (!e) return;
    if (e->count >= EXPR_MAX_NODES) return;
    PushNode(e, NODE_OP, (double)(unsigned char)op);
}

void ExprPop(Expr* e) {
    if (!e || !e->head) return;
    // rimuove l'ultimo nodo tenendo aggiornata la coda
    if (!e->head->next) {
        NodeFree(e->head);
        e->head = e->tail = NULL;
        e->count = 0;
        return;
    }
    ExprNode* prev = e->head;
    while (prev->next->next) prev = prev->next;
    NodeFree(prev->next);
    prev->next = NULL;
    e->tail = prev;
    e->count--;
}

int ExprCount(const Expr* e) { return e ? e->count : 0; }

Expr ExprCopy(const Expr* src) {
    Expr out;
    ExprInit(&out);
    if (!src) return out;
    for (ExprNode* c = src->head; c; c = c->next) PushNode(&out, c->type, c->val);
    return out;
}

bool ExprLastNumberHasDot(const Expr* e) {
    if (!e || !e->head) return false;
    const ExprNode* c = e->head;
    bool hasDot = false;
    while (c) {
        if (c->type == NODE_OP && (int)c->val == '.') hasDot = true;
        else if (c->type == NODE_OP) hasDot = false;   // nuovo numero -> riparte
        c = c->next;
    }
    return hasDot;
}

// ============================================================
//  FORMATTAZIONE DEI NUMERI
//  %g prima tronca a 6 cifre significative: 1/3 -> 0.333333
//  e produce "1e+06" che in una calcolatrice e' illeggibile.
//  Qui: 12 cifre, notazione decimale per l'uso normale e
//  scientifica solo quando serve davvero.
// ============================================================
void FormatNumber(double v, char* out, size_t cap) {
    if (cap == 0) return;

    if (isnan(v))      { snprintf(out, cap, "Error"); return; }
    if (isinf(v))      { snprintf(out, cap, v > 0 ? "Infinity" : "-Infinity"); return; }

    double a = fabs(v);

    // Intervallo "normale": formato decimale, 12 cifre significative
    if (a == 0.0 || (a >= 1e-9 && a < 1e12)) {
        int decimals = 12;
        if (a >= 100.0)      decimals = 6;
        else if (a >= 1.0)   decimals = 10;
        else if (a >= 0.01)  decimals = 12;

        snprintf(out, cap, "%.*f", decimals, v);

        // rimuove zeri finali e il punto che resta appeso ("5." -> "5")
        size_t len = strlen(out);
        if (len > 0) {                      // len == 0 punterebbe fuori buffer
            char* end = out + len - 1;
            while (end > out && *end == '0') { *end-- = '\0'; }
            if (end > out && *end == '.') { *end = '\0'; }
        }

        // corregge "-0" -> "0" (rimuovendo il segno, non sovrascrivendolo,
        // altrimenti "-0" diventerebbe "00")
        if (out[0] == '-' && out[1] == '0' && out[2] == '\0') {
            out[0] = '0';
            out[1] = '\0';
        }
        return;
    }

    // Molto grande o molto piccolo: notazione scientifica
    snprintf(out, cap, "%.6e", v);
}

// ============================================================
//  VISUALIZZAZIONE  (O(n) con cursore invece di strcat O(n^2))
// ============================================================
int ExprToString(const Expr* e, char* buf, int cap) {
    if (cap <= 0) return 0;
    buf[0] = '\0';
    if (!e || !e->head) return 0;

    int written = 0;
    bool atStart = true;        // siamo ancora sul primo token scritto?

    for (ExprNode* c = e->head; c; c = c->next) {
        // salta lo zero iniziale se il token successivo e' un'altra cifra:
        // "05" si mostra "5", ma "0.5" resta "0.5"
        if (atStart && c->type == NODE_DIGIT && (int)c->val == 0) {
            const ExprNode* nx = c->next;
            if (nx && nx->type == NODE_DIGIT) continue;   // nodo scartato
        }
        atStart = false;

        char tmp[40];
        int  n;

        switch (c->type) {
            case NODE_DIGIT:
                tmp[0] = (char)('0' + (int)c->val);
                tmp[1] = '\0';
                n = 1;
                break;
            case NODE_OP:
                tmp[0] = (char)(int)c->val;
                tmp[1] = '\0';
                n = 1;
                break;
            case NODE_VALUE:
            default:
                FormatNumber(c->val, tmp, sizeof(tmp));
                n = (int)strlen(tmp);
                break;
        }

        if (written + n >= cap) break;          // troncamento sicuro
        memcpy(buf + written, tmp, (size_t)n);
        written += n;
        buf[written] = '\0';
    }
    return written;
}

// ============================================================
//  VALUTAZIONE  (shunting-yard: O(n), rispetta la precedenza)
// ============================================================
static int Prec(char op) {
    if (op == '*' || op == '/' || op == '%') return 2;
    if (op == '+' || op == '-')              return 1;
    return 0;
}

static bool ApplyOp(char op, double a, double b, double* out) {
    switch (op) {
        case '+': *out = a + b; return true;
        case '-': *out = a - b; return true;
        case '*': *out = a * b; return true;
        case '/':
            if (b == 0.0) return false;        // divisione per zero
            *out = a / b;
            return true;
        case '%':
            if (b == 0.0) return false;        // modulo per zero
            *out = fmod(a, b);
            return true;
        default: return false;
    }
}

double EvaluateExpr(const Expr* e, bool* error) {
    *error = false;
    if (!e || !e->head) return 0.0;

    double stack[EXPR_MAX_NODES];
    char   ops[EXPR_MAX_NODES];
    int    sp = 0;          // elementi nello stack dei valori
    int    opc = 0;         // elementi nello stack degli operatori
    bool   needOperand = true;   // siamo all'inizio o dopo un operatore?
    bool   negateNext = false;   // c'è un '-' unario in attesa (es. "-5", "3*-2")

    // cursore esplicito: il ciclo interno consuma piu' token, quindi
    // l'avanzamento e' gestito qui e NON dall'incremento del for
    ExprNode* c = e->head;

    while (c) {
        // ---- Costruisce un numero da una sequenza cifre/'.' ----
        if (c->type == NODE_DIGIT || (c->type == NODE_OP && (int)c->val == '.')) {
            double val = 0.0, mult = 1.0;
            bool inDec = false;

            while (c && (c->type == NODE_DIGIT || (c->type == NODE_OP && (int)c->val == '.'))) {
                if (c->type == NODE_OP) inDec = true;
                else if (!inDec)        val = val * 10.0 + c->val;
                else { mult *= 0.1; val += c->val * mult; }
                c = c->next;
            }

            if (negateNext) { val = -val; negateNext = false; }

            if (sp >= EXPR_MAX_NODES) { *error = true; return 0.0; }
            stack[sp++] = val;
            needOperand = false;
            continue;              // c e' gia' posizionato sul token successivo
        }

        if (c->type == NODE_VALUE) {
            double v = negateNext ? -c->val : c->val;
            negateNext = false;
            if (sp >= EXPR_MAX_NODES) { *error = true; return 0.0; }
            stack[sp++] = v;
            needOperand = false;
            c = c->next;
            continue;
        }

        // ---- Operatore ----
        char op = (char)(int)c->val;

        // Meno unario: all'inizio ("-5") o dopo un operatore ("3*-2").
        // Non entra nello stack: viene applicato al prossimo valore.
        if (op == '-' && needOperand) { negateNext = true; c = c->next; continue; }

        if (needOperand) { *error = true; return 0.0; }   // "5 * * 3"

        while (opc > 0 && Prec(ops[opc - 1]) >= Prec(op)) {
            char  top = ops[--opc];
            if (sp < 2) { *error = true; return 0.0; }
            double r;
            if (!ApplyOp(top, stack[sp - 2], stack[sp - 1], &r)) { *error = true; return 0.0; }
            sp -= 2;
            stack[sp++] = r;
        }
        if (opc >= EXPR_MAX_NODES) { *error = true; return 0.0; }
        ops[opc++] = op;
        needOperand = true;
        c = c->next;
    }

    if (needOperand && !negateNext) { *error = true; return 0.0; }   // "5+" o "5*"
    if (negateNext)                 { *error = true; return 0.0; }   // solo "-"

    // ---- Operatori rimasti (l'espressione finiva con "5+") ----
    while (opc > 0) {
        char top = ops[--opc];
        if (sp < 2) { *error = true; return 0.0; }
        double r;
        if (!ApplyOp(top, stack[sp - 2], stack[sp - 1], &r)) { *error = true; return 0.0; }
        sp -= 2;
        stack[sp++] = r;
    }

    if (sp != 1) { *error = true; return 0.0; }
    return stack[0];
}
