#ifndef EVAL_H
#define EVAL_H

#include <stdbool.h>

typedef enum {
    NODE_NUM,
    NODE_OP
} NodeType;

typedef struct ExprNode {
    NodeType type;
    union {
        int n;
        char op;
    };
    struct ExprNode* next;
} ExprNode;

void AppendOp(ExprNode** head, char op);
void AppendNum(ExprNode** head, int n);
void PopNode(ExprNode** head);
void ClearExpr(ExprNode** head);
ExprNode* CopyExpr(ExprNode* head);
double EvaluateExpr(ExprNode* head, bool* error);
void ExprToString(ExprNode* head, char* buffer, int max_len);

extern double lastResult;

#endif
