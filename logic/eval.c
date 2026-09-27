#include "eval.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

void AppendOp(ExprNode** head, char op) {
    ExprNode* node = (ExprNode*)malloc(sizeof(ExprNode));
    node->type = NODE_OP;
    node->op = op;
    node->next = NULL;
    if (!*head) *head = node;
    else { ExprNode* curr = *head; while (curr->next) curr = curr->next; curr->next = node; }
}

void AppendNum(ExprNode** head, int n) {
    ExprNode* node = (ExprNode*)malloc(sizeof(ExprNode));
    node->type = NODE_NUM;
    node->n = n;
    node->next = NULL;
    if (!*head) *head = node;
    else { ExprNode* curr = *head; while (curr->next) curr = curr->next; curr->next = node; }
}

double lastResult = 0.0;

void ClearExpr(ExprNode** head) {
    ExprNode* curr = *head;
    while (curr) { ExprNode* next = curr->next; free(curr); curr = next; }
    *head = NULL;
}

ExprNode* CopyExpr(ExprNode* head) {
    if (!head) return NULL;
    ExprNode* newHead = NULL;
    ExprNode* curr = head;
    while (curr) {
        if (curr->type == NODE_NUM) AppendNum(&newHead, curr->n);
        else AppendOp(&newHead, curr->op);
        curr = curr->next;
    }
    return newHead;
}

void PopNode(ExprNode** head) {
    if (!*head) return;
    if (!(*head)->next) {
        free(*head);
        *head = NULL;
        return;
    }
    ExprNode* curr = *head;
    while (curr->next && curr->next->next) {
        curr = curr->next;
    }
    free(curr->next);
    curr->next = NULL;
}

double EvaluateExpr(ExprNode* head, bool* error) {
    *error = false;
    if (!head) return 0;
    
    // 1. Convert linked list nodes to parallel arrays of doubles/chars.
    // We combine consecutive digits and decimal points into single numbers.
    double vals[100];
    char ops[100];
    int v_count = 0, o_count = 0;
    
    ExprNode* curr = head;
    while (curr) {
        if (curr->type == NODE_NUM) {
            double val = 0;
            double decimal_mult = 1;
            bool in_decimal = false;
            
            // Read full number including decimals
            while (curr && (curr->type == NODE_NUM || (curr->type == NODE_OP && curr->op == '.'))) {
                if (curr->type == NODE_OP && curr->op == '.') {
                    in_decimal = true;
                } else if (curr->type == NODE_NUM) {
                    if (!in_decimal) {
                        val = val * 10 + curr->n;
                    } else {
                        decimal_mult /= 10.0;
                        val = val + curr->n * decimal_mult;
                    }
                }
                curr = curr->next;
            }
            vals[v_count++] = val;
        } else {
            ops[o_count++] = curr->op;
            curr = curr->next;
        }
    }
    
    // 2. Evaluate sqrt
    // Since sqrt is an operator that precedes a number, let's process it.
    for (int i=0; i<o_count; i++) {
        if (ops[i] == 's') {
            // sqrt should apply to the next number
            if (i < v_count) {
                vals[i] = sqrt(vals[i]);
            }
            // Remove 's' from ops
            for (int j=i; j<o_count-1; j++) ops[j] = ops[j+1];
            o_count--;
            i--;
        }
    }

    // 3. Multiply/Divide
    for (int i=0; i<o_count; i++) {
        if (ops[i] == '*' || ops[i] == '/') {
            if (ops[i] == '*') vals[i] = vals[i] * vals[i+1];
            if (ops[i] == '/') {
                if (vals[i+1] == 0) { *error = true; return 0; }
                vals[i] = vals[i] / vals[i+1];
            }
            for (int j=i+1; j<v_count-1; j++) vals[j] = vals[j+1];
            for (int j=i; j<o_count-1; j++) ops[j] = ops[j+1];
            v_count--; o_count--; i--;
        }
    }
    
    // 4. Add/Subtract
    for (int i=0; i<o_count; i++) {
        if (ops[i] == '+' || ops[i] == '-') {
            if (ops[i] == '+') vals[i] = vals[i] + vals[i+1];
            if (ops[i] == '-') vals[i] = vals[i] - vals[i+1];
            for (int j=i+1; j<v_count-1; j++) vals[j] = vals[j+1];
            for (int j=i; j<o_count-1; j++) ops[j] = ops[j+1];
            v_count--; o_count--; i--;
        }
    }
    
    if (v_count > 0) return vals[0];
    return 0;
}

void ExprToString(ExprNode* head, char* buffer, int max_len) {
    buffer[0] = '\0';
    ExprNode* curr = head;
    while (curr) {
        char temp[32];
        if (curr->type == NODE_NUM) snprintf(temp, sizeof(temp), "%d", curr->n);
        else {
            if (curr->op == 's') snprintf(temp, sizeof(temp), "sqrt(");
            else snprintf(temp, sizeof(temp), "%c", curr->op);
        }
        if (strlen(buffer) + strlen(temp) < (size_t)max_len) strcat(buffer, temp);
        curr = curr->next;
    }
}
