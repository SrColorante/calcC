#ifndef EVAL_H
#define EVAL_H

#include <stdbool.h>
#include <stddef.h>

// ============================================================
//  LIMITE DI SICUREZZA
//  Prima: EvaluateExpr usava vals[100]/ops[100] SENZA controllare
//  l'indice -> un input da tastiera lungo mandava 100+ numeri e
//  corrompeva lo stack. Ora i limiti sono espliciti e verificati.
// ============================================================
#define EXPR_MAX_NODES 160   // nodi massimi per singola espressione
#define NODE_POOL_SIZE 4096  // nodi totali (espressione + cronologia)

typedef enum {
    NODE_DIGIT,  // una singola cifra 0-9, val = 0..9
    NODE_VALUE,  // un double pronto (es. il valore di "Ans")
    NODE_OP      // operatore o punto decimale, val = codice ASCII
} NodeType;

typedef struct ExprNode {
    NodeType type;
    double   val;
    struct ExprNode* next;
} ExprNode;

// Lista con puntatore di coda: append O(1) invece che O(n).
// Prima ogni Append* percorreva l'intera lista -> costruzione O(n^2).
typedef struct {
    ExprNode *head;
    ExprNode *tail;
} Expr;

void  PoolInit(void);

void  ExprInit(Expr* e);
void  ExprClear(Expr* e);
void  ExprDigit(Expr* e, int digit);
void  ExprValue(Expr* e, double v);   // inserisce un double pronto (Ans)
void  ExprOp(Expr* e, char op);
void  ExprPop(Expr* e);               // backspace: toglie l'ultimo token
Expr  ExprCopy(const Expr* src);      // deep copy

double EvaluateExpr(const Expr* e, bool* error);
int   ExprToString(const Expr* e, char* buf, int cap);   // cap-1 byte scritti
int   ExprCount(const Expr* e);
bool  ExprLastNumberHasDot(const Expr* e);               // blocca "1.2.3"
void  FormatNumber(double v, char* out, size_t cap);

#endif
