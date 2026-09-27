#include "raylib.h"
#include "config.h"
#include "ui/button.h"
#include "logic/eval.h"
#include <stdio.h>
#include <string.h>

#define NUM_BUTTONS 20
Button buttons[NUM_BUTTONS];
ExprNode* exprList = NULL;
char displayBuffer[256] = "";
char resultBuffer[256] = "";

// Animation state
float animSlideUp = 0.0f;
bool isResultState = false;
float popAnim = 0.0f;
int lastDisplayLen = 0;

void TriggerButtonVisual(const char* label) {
    for (int i=0; i<NUM_BUTTONS; i++) {
        if (strcmp(buttons[i].text, label) == 0) {
            buttons[i].visual_press_timer = 0.15f;
            break;
        }
    }
}

void UpdateLayout(void) {
    float sw = GetScreenWidth();
    float sh = GetScreenHeight();
    
    float displayHeight = sh * 0.30f;
    float buttonsAreaHeight = sh * 0.60f;
    
    float marginX = sw * 0.05f;
    float marginY = sh * 0.05f;
    
    float availableWidth = sw - 2*marginX;
    float pad = 16.0f;
    float w = (availableWidth - 3*pad) / 4.0f;
    float h = (buttonsAreaHeight - 4*pad) / 5.0f;
    
    float startX = marginX;
    float startY = displayHeight + marginY;
    
    const char* labels[5][4] = {
        {"C", "%", "*", "/"},
        {"7", "8", "9", "-"},
        {"4", "5", "6", "+"},
        {"1", "2", "3", "="},
        {"0", ".", "", ""}
    };
    
    int btn_idx = 0;
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            if (r == 4 && c == 2) continue;
            if (r == 4 && c == 3) continue;
            
            buttons[btn_idx].grid_x = c;
            buttons[btn_idx].grid_y = r;
            strcpy(buttons[btn_idx].text, labels[r][c]);
            
            float bx = startX + c * (w + pad);
            float by = startY + r * (h + pad);
            
            if (r == 4 && c == 1) {
                buttons[btn_idx].shape = BTN_H_LONG;
                buttons[btn_idx].rect = (Rectangle){ bx, by, w*2 + pad, h };
            } else if (r == 3 && c == 3) {
                buttons[btn_idx].shape = BTN_V_LONG;
                buttons[btn_idx].rect = (Rectangle){ bx, by, w, h*2 + pad };
            } else {
                buttons[btn_idx].shape = BTN_NORMAL;
                buttons[btn_idx].rect = (Rectangle){ bx, by, w, h };
            }
            btn_idx++;
        }
    }
}

// History
#define MAX_HISTORY 20
ExprNode* history[MAX_HISTORY] = {0};
int history_count = 0;
int history_idx = -1;

void HandleAnsLogic(bool is_operator, char ch, int num) {
    char lbl[2] = {0, 0};
    if (num != -1) {
        lbl[0] = '0' + num;
        TriggerButtonVisual(lbl);
    } else if (ch != '\0') {
        lbl[0] = ch;
        TriggerButtonVisual(lbl);
    }

    if (isResultState) {
        // Save current to history
        if (history_count < MAX_HISTORY) {
            history[history_count++] = CopyExpr(exprList);
            history_idx = history_count;
        } else {
            ClearExpr(&history[0]);
            for (int i=1; i<MAX_HISTORY; i++) history[i-1] = history[i];
            history[MAX_HISTORY-1] = CopyExpr(exprList);
            history_idx = MAX_HISTORY;
        }

        if (is_operator) {
            // Keep lastResult (Ans) and append operator
            ClearExpr(&exprList);
            char ansStr[64];
            snprintf(ansStr, sizeof(ansStr), "%g", lastResult);
            if (lastResult < 0) AppendNum(&exprList, 0); // Handle unary minus by prepending 0
            for (size_t i=0; i<strlen(ansStr); i++) {
                if (ansStr[i] == '.') AppendOp(&exprList, '.');
                else if (ansStr[i] == '-') AppendOp(&exprList, '-');
                else if (ansStr[i] >= '0' && ansStr[i] <= '9') AppendNum(&exprList, ansStr[i] - '0');
            }
            if (ch != '\0') AppendOp(&exprList, ch);
        } else {
            // Clear and start new
            ClearExpr(&exprList);
            if (num != -1) AppendNum(&exprList, num);
            else if (ch != '\0') AppendOp(&exprList, ch);
        }
        isResultState = false;
        strcpy(resultBuffer, "");
    } else {
        if (num != -1) AppendNum(&exprList, num);
        else if (ch != '\0') AppendOp(&exprList, ch);
    }
}

void HandleKeyboardInput() {
    int key = GetKeyPressed();
    while (key != 0) {
        if (key == KEY_BACKSPACE || IsKeyPressedRepeat(KEY_BACKSPACE)) {
            if (isResultState) {
                isResultState = false;
                strcpy(resultBuffer, "");
            } else {
                PopNode(&exprList);
            }
        } else if (key == KEY_C || key == KEY_DELETE) {
            ClearExpr(&exprList);
            strcpy(resultBuffer, "");
            isResultState = false;
            TriggerButtonVisual("C");
        } else if (key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_EQUAL || key == KEY_KP_EQUAL) {
            bool err;
            lastResult = EvaluateExpr(exprList, &err);
            if (err) strcpy(resultBuffer, "Error");
            else snprintf(resultBuffer, sizeof(resultBuffer), "%g", lastResult);
            isResultState = true;
            animSlideUp = 0.0f;
            TriggerButtonVisual("=");
        } else if (key == KEY_UP) {
            if (history_count > 0 && history_idx > 0) {
                history_idx--;
                ClearExpr(&exprList);
                exprList = CopyExpr(history[history_idx]);
                isResultState = false;
                strcpy(resultBuffer, "");
            }
        } else if (key == KEY_DOWN) {
            if (history_idx < history_count - 1) {
                history_idx++;
                ClearExpr(&exprList);
                exprList = CopyExpr(history[history_idx]);
                isResultState = false;
                strcpy(resultBuffer, "");
            } else if (history_idx == history_count - 1) {
                history_idx++;
                ClearExpr(&exprList);
                isResultState = false;
                strcpy(resultBuffer, "");
            }
        }
        key = GetKeyPressed();
    }
    
    // Use GetCharPressed for characters (handles shift properly, avoids double input)
    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= '0' && ch <= '9') HandleAnsLogic(false, '\0', ch - '0');
        else if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%') HandleAnsLogic(true, (char)ch, -1);
        else if (ch == '.') {
            HandleAnsLogic(false, (char)ch, -1);
        }
        ch = GetCharPressed();
    }
}

void HandleButtonPress(Button* b) {
    if (strcmp(b->text, "=") == 0) {
        bool err;
        lastResult = EvaluateExpr(exprList, &err);
        if (err) strcpy(resultBuffer, "Error");
        else snprintf(resultBuffer, sizeof(resultBuffer), "%g", lastResult);
        isResultState = true;
        animSlideUp = 0.0f;
    } else {
        if (b->text[0] >= '0' && b->text[0] <= '9') {
            HandleAnsLogic(false, '\0', b->text[0] - '0');
        } else if (strcmp(b->text, ".") == 0) {
            HandleAnsLogic(false, '.', -1);
        } else if (strcmp(b->text, "%") == 0) {
            HandleAnsLogic(true, '%', -1);
        } else if (strcmp(b->text, "C") == 0) {
            ClearExpr(&exprList);
            strcpy(resultBuffer, "");
            isResultState = false;
        } else {
            HandleAnsLogic(true, b->text[0], -1);
        }
    }
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Calculator");
    
    if (!IsWindowReady()) {
        printf("Error: Failed to initialize window.\n");
        return 1;
    }
    
    UpdateLayout();
    
    Font customFont = LoadFontEx("assets/Comfortaa.ttf", 64, 0, 0);
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) break;
        if (IsWindowResized()) UpdateLayout();
        
        float dt = GetFrameTime();
        Vector2 mouse = GetMousePosition();
        
        HandleKeyboardInput();
        
        for (int i=0; i<NUM_BUTTONS; i++) {
            if (buttons[i].text[0] == '\0') continue;
            
            if (buttons[i].visual_press_timer > 0.0f) {
                buttons[i].visual_press_timer -= dt;
            }
            
            buttons[i].is_pressed = false;
            buttons[i].is_hovered = CheckCollisionPointRec(mouse, buttons[i].rect);
            if (buttons[i].is_hovered) {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    buttons[i].is_pressed = true;
                }
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                    HandleButtonPress(&buttons[i]);
                }
            }
        }
        
        ExprToString(exprList, displayBuffer, sizeof(displayBuffer));
        
        bool isDark = (IsSystemThemeDark() == 1);
        Color appBg = isDark ? (Color){20, 20, 20, 180} : (Color){240, 240, 240, 180};
        
        // Shader needs to be drawn inside the rounded app background
        BeginDrawing();
        ClearBackground(BLANK);
        
        // Draw Main App Background (smaller radius than buttons)
        float appSmuss = 0.15f;
        Rectangle appRec = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        DrawRectangleRounded(appRec, appSmuss, SEGM, appBg);
        
        // Draw Display Area as a rounded box
        Rectangle displayRec = {
            SCREEN_WIDTH * 0.05f, 
            SCREEN_HEIGHT * 0.05f, 
            SCREEN_WIDTH * 0.90f, 
            SCREEN_HEIGHT * 0.25f
        };
        Color displayBg = isDark ? (Color){30, 30, 30, 220} : (Color){220, 220, 220, 220};
        DrawRectangleRounded(displayRec, SMUSS, SEGM, displayBg);
        DrawRectangleRoundedLines(displayRec, SMUSS, SEGM, 2, isDark ? GRAY : DARKGRAY);
        
        // Determine if display length changed for pop animation
        int currentLen = strlen(displayBuffer);
        if (currentLen > lastDisplayLen) {
            popAnim = 1.0f; // trigger pop
        }
        lastDisplayLen = currentLen;
        
        // Update animations
        if (isResultState) {
            animSlideUp += dt * 10.0f;
            if (animSlideUp > 1.0f) animSlideUp = 1.0f;
        } else {
            animSlideUp -= dt * 10.0f;
            if (animSlideUp < 0.0f) animSlideUp = 0.0f;
        }
        if (popAnim > 0.0f) {
            popAnim -= dt * 10.0f;
            if (popAnim < 0.0f) popAnim = 0.0f;
        }

        // Draw Display text (right aligned)
        // Base sizes
        float exprFontSize = 34.0f + (popAnim * 6.0f); // Slight pop effect
        float resultFontSize = 46.0f;
        
        Vector2 exprSize = MeasureTextEx(customFont, displayBuffer, exprFontSize, 1);
        Vector2 resSize = MeasureTextEx(customFont, resultBuffer, resultFontSize, 1);
        
        // Target positions
        // Default: Expression in center
        float exprCenterY = displayRec.y + displayRec.height/2.0f - exprSize.y/2.0f;
        
        // When result: Expression moves up, Result moves to center
        float exprUpY = displayRec.y + 15.0f;
        float resCenterY = displayRec.y + displayRec.height/2.0f - resSize.y/2.0f + 10.0f;
        float resDownY = displayRec.y + displayRec.height; // hidden below
        
        float currentExprY = exprCenterY + (exprUpY - exprCenterY) * animSlideUp;
        float currentResY = resDownY + (resCenterY - resDownY) * animSlideUp;
        
        float exprX = displayRec.x + displayRec.width - 15.0f - exprSize.x;
        float resX = displayRec.x + displayRec.width - 15.0f - resSize.x;
        
        Color exprColor = isDark ? WHITE : BLACK;
        Color resColor = isDark ? GREEN : DARKGREEN;
        // Fade out expression slightly when result is shown
        exprColor.a = 255 - (unsigned char)(animSlideUp * 100);
        resColor.a = (unsigned char)(animSlideUp * 255);
        
        // Scissor mode to prevent text drawing outside the rounded box
        BeginScissorMode((int)displayRec.x, (int)displayRec.y, (int)displayRec.width, (int)displayRec.height);
        DrawTextEx(customFont, displayBuffer, (Vector2){exprX, currentExprY}, exprFontSize, 1, exprColor);
        if (animSlideUp > 0.01f) {
            DrawTextEx(customFont, resultBuffer, (Vector2){resX, currentResY}, resultFontSize, 1, resColor);
        }
        EndScissorMode();
        
        for (int i=0; i<NUM_BUTTONS; i++) {
            if (buttons[i].text[0] != '\0') {
                DrawCalcButton(&buttons[i], mouse, customFont);
            }
        }
        
        EndDrawing();
    }
    
    ClearExpr(&exprList);
    CloseWindow();
    return 0;
}