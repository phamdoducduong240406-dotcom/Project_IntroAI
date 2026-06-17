#include "ui.h"
#include <raylib.h>
#include <cstring>

// ========================================================================
// Hệ thống Font tùy chỉnh
// ========================================================================
static Font gameFont = {0};
static bool fontReady = false;
static const float FONT_SPACING = 1.0f;

void UI::Init() {
    // Thử tải font từ nhiều đường dẫn (tùy vào thư mục chạy exe)
    gameFont = LoadFontEx("fonts/GameFont.ttf", 64, NULL, 0);
    if (gameFont.glyphCount == 0) {
        gameFont = LoadFontEx("../fonts/GameFont.ttf", 64, NULL, 0);
    }
    if (gameFont.glyphCount > 0) {
        SetTextureFilter(gameFont.texture, TEXTURE_FILTER_BILINEAR);
        fontReady = true;
    }
}

void UI::Cleanup() {
    if (fontReady) {
        UnloadFont(gameFont);
        fontReady = false;
    }
}

// ========================================================================
// Helper: Vẽ chữ bằng font tùy chỉnh (fallback về font mặc định nếu lỗi)
// ========================================================================
static void DrawGameText(const char* text, float x, float y, float fontSize, Color color) {
    if (!text) return;
    if (fontReady) {
        DrawTextEx(gameFont, text, {x, y}, fontSize, FONT_SPACING, color);
    } else {
        DrawText(text, (int)x, (int)y, (int)fontSize, color);
    }
}

// ========================================================================
// Helper: Đo chiều rộng chữ bằng font tùy chỉnh
// ========================================================================
static int MeasureGameText(const char* text, float fontSize) {
    if (!text) return 0;
    if (fontReady) {
        return (int)MeasureTextEx(gameFont, text, fontSize, FONT_SPACING).x;
    }
    return MeasureText(text, (int)fontSize);
}

// ========================================================================
// Helper: Chuyển đổi mã phím Raylib thành tên hiển thị cho người dùng
// ========================================================================
static const char* GetKeyDisplayName(int key) {
    switch(key) {
        case KEY_SPACE: return "SPACE";
        case KEY_ENTER: return "ENTER";
        case KEY_TAB: return "TAB";
        case KEY_BACKSPACE: return "BKSP";
        case KEY_DELETE: return "DEL";
        case KEY_RIGHT: return "RIGHT";
        case KEY_LEFT: return "LEFT";
        case KEY_DOWN: return "DOWN";
        case KEY_UP: return "UP";
        case KEY_LEFT_SHIFT: return "LSHIFT";
        case KEY_RIGHT_SHIFT: return "RSHIFT";
        case KEY_LEFT_CONTROL: return "LCTRL";
        case KEY_RIGHT_CONTROL: return "RCTRL";
        case KEY_LEFT_ALT: return "LALT";
        case KEY_RIGHT_ALT: return "RALT";
        case KEY_SLASH: return "/";
        case KEY_BACKSLASH: return "\\";
        case KEY_PERIOD: return ".";
        case KEY_COMMA: return ",";
        case KEY_SEMICOLON: return ";";
        case KEY_MINUS: return "-";
        case KEY_EQUAL: return "=";
        case KEY_LEFT_BRACKET: return "[";
        case KEY_RIGHT_BRACKET: return "]";
        case KEY_KP_0: return "NUM0";
        case KEY_KP_1: return "NUM1";
        case KEY_KP_2: return "NUM2";
        case KEY_KP_3: return "NUM3";
        case KEY_KP_4: return "NUM4";
        case KEY_KP_5: return "NUM5";
        case KEY_KP_6: return "NUM6";
        case KEY_KP_7: return "NUM7";
        case KEY_KP_8: return "NUM8";
        case KEY_KP_9: return "NUM9";
        default:
            if (key >= 32 && key <= 126) return TextFormat("%c", (char)key);
            return "?";
    }
}

// ========================================================================
// Helper: Vẽ biểu tượng bánh răng (Gear Icon)
// ========================================================================
static void DrawGearIcon(float cx, float cy, float r, Color color, Color holeColor) {
    float bar = r * 1.9f;
    float thick = r * 0.38f;
    DrawRectanglePro({cx, cy, bar, thick}, {bar / 2.0f, thick / 2.0f}, 0.0f, color);
    DrawRectanglePro({cx, cy, bar, thick}, {bar / 2.0f, thick / 2.0f}, 60.0f, color);
    DrawRectanglePro({cx, cy, bar, thick}, {bar / 2.0f, thick / 2.0f}, 120.0f, color);
    DrawCircle((int)cx, (int)cy, r * 0.58f, color);
    DrawCircle((int)cx, (int)cy, r * 0.22f, holeColor);
}

// ========================================================================
// Kiểm tra nút Settings (bánh răng) có bị click không
// ========================================================================
bool UI::CheckSettingsButtonClicked() {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mp = GetMousePosition();
        float cx = SCREEN_WIDTH - 30.0f;
        float cy = 28.0f;
        float dx = mp.x - cx;
        float dy = mp.y - cy;
        return (dx * dx + dy * dy) <= 22.0f * 22.0f;
    }
    return false;
}

// ========================================================================
// Vẽ HUD: Nút bánh răng (có hiệu ứng hover) + Bảng điểm
// ========================================================================
void UI::DrawHUD(const int scores[], int numPlayers) {
    // --- Nút bánh răng ---
    Vector2 mouse = GetMousePosition();
    float gearCx = SCREEN_WIDTH - 30.0f;
    float gearCy = 28.0f;
    float dx = mouse.x - gearCx;
    float dy = mouse.y - gearCy;
    bool hovered = (dx * dx + dy * dy) <= 22.0f * 22.0f;
    DrawGearIcon(gearCx, gearCy, hovered ? 16.0f : 14.0f, hovered ? DARKGRAY : GRAY, RAYWHITE);

    // --- Bảng điểm ---
    Color playerColors[] = {DARKGREEN, DARKBLUE, MAROON, GOLD};
    int secW = SCREEN_WIDTH / numPlayers;
    for (int i = 0; i < numPlayers; i++) {
        const char* txt = TextFormat("Player %d: %d", i + 1, scores[i]);
        int tw = MeasureGameText(txt, 24);
        DrawGameText(txt, (float)((i * secW) + (secW / 2) - tw / 2), (float)(SCREEN_HEIGHT - 40), 24, playerColors[i]);
    }
}

// ========================================================================
// Màn hình cài đặt phím cho từng người chơi (6 phím)
// ========================================================================
void UI::ShowKeyBindingScreen(int& fw, int& bw, int& tl, int& tr, int& sh, int& shield, int playerIndex) {
    int backup[6] = {fw, bw, tl, tr, sh, shield};
    int* refs[] = {&fw, &bw, &tl, &tr, &sh, &shield};
    const char* prompts[] = {
        "AN PHIM: DI TIEN", "AN PHIM: DI LUI",
        "AN PHIM: XOAY TRAI", "AN PHIM: XOAY PHAI",
        "AN PHIM: BAN DAN", "AN PHIM: KHIEN"
    };
    const char* labels[] = {"DI TIEN", "DI LUI", "XOAY TRAI", "XOAY PHAI", "BAN DAN", "KHIEN"};

    int state = 0;
    Color accentColors[] = {{40, 130, 50, 255}, {40, 60, 160, 255}, {140, 30, 30, 255}, {180, 150, 20, 255}};
    Color accent = accentColors[playerIndex - 1];

    while (!WindowShouldClose() && state < 6) {
        int key = GetKeyPressed();
        if (key == KEY_ESCAPE) {
            fw = backup[0]; bw = backup[1]; tl = backup[2];
            tr = backup[3]; sh = backup[4]; shield = backup[5];
            return;
        }
        if (key > 0) {
            *refs[state] = key;
            state++;
        }

        BeginDrawing();
        ClearBackground({40, 42, 54, 255});

        float panelW = 460, panelH = 420;
        float panelX = (SCREEN_WIDTH - panelW) / 2.0f;
        float panelY = (SCREEN_HEIGHT - panelH) / 2.0f;

        // Panel nền
        DrawRectangleRounded({panelX - 2, panelY - 2, panelW + 4, panelH + 4}, 0.03f, 10, {70, 72, 90, 255});
        DrawRectangleRounded({panelX, panelY, panelW, panelH}, 0.03f, 10, {248, 248, 252, 255});

        // Tiêu đề
        const char* title = TextFormat("PHIM - NGUOI CHOI %d", playerIndex);
        int titleW = MeasureGameText(title, 28);
        DrawGameText(title, SCREEN_WIDTH / 2.0f - titleW / 2.0f, panelY + 24, 28, accent);
        DrawLine((int)(panelX + 30), (int)(panelY + 62), (int)(panelX + panelW - 30), (int)(panelY + 62), {210, 210, 220, 255});

        // Phím đang chờ nhập
        if (state < 6) {
            DrawRectangleRounded({panelX + 40, panelY + 78, panelW - 80, 36}, 0.3f, 8, {255, 240, 240, 255});
            int pw = MeasureGameText(prompts[state], 22);
            DrawGameText(prompts[state], SCREEN_WIDTH / 2.0f - pw / 2.0f, panelY + 85, 22, {200, 50, 50, 255});
        }

        // Danh sách phím
        float listY = panelY + 135;
        for (int i = 0; i < 6; i++) {
            Color rowColor;
            if (i < state) rowColor = {40, 130, 50, 255};
            else if (i == state) rowColor = {200, 50, 50, 255};
            else rowColor = {170, 170, 180, 255};

            if (i == state) {
                DrawRectangleRounded({panelX + 45, listY + i * 38.0f - 3, panelW - 90, 32}, 0.2f, 6, {252, 245, 245, 255});
            }

            DrawGameText(labels[i], panelX + 60, listY + i * 38, 20, rowColor);

            if (i < state) {
                DrawGameText(GetKeyDisplayName(*refs[i]), panelX + 300, listY + i * 38, 20, rowColor);
            } else {
                DrawGameText(i == state ? "_" : "?", panelX + 300, listY + i * 38, 20, rowColor);
            }
        }

        // Chỉ dẫn ESC
        const char* hint = "ESC de huy";
        int hw = MeasureGameText(hint, 16);
        DrawGameText(hint, SCREEN_WIDTH / 2.0f - hw / 2.0f, panelY + panelH - 35, 16, {150, 150, 160, 255});

        EndDrawing();
    }
}

// ========================================================================
// Màn hình Cài đặt chính (Settings Screen)
// ========================================================================
void UI::ShowSettingsScreen(int& numPlayers, bool& portalsEnabled, bool& itemsEnabled,
    bool& shieldsEnabled, std::vector<PlayerConfig>& configs) {

    SetExitKey(0);

    float panelW = 520, panelH = 610;
    float panelX = (SCREEN_WIDTH - panelW) / 2.0f;
    float panelY = (SCREEN_HEIGHT - panelH) / 2.0f;

    Color playerColors[] = {{40, 130, 50, 255}, {40, 60, 160, 255}, {140, 30, 30, 255}, {180, 150, 20, 255}};

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) break;

        Vector2 mouse = GetMousePosition();
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        // ---- Layout ----
        float labelX = panelX + 40;
        float controlX = panelX + 320;
        float rowStart = panelY + 88;
        float rowH = 50;

        float row0Y = rowStart;
        Rectangle btnLeft = {controlX, row0Y, 32, 32};
        Rectangle btnRight = {controlX + 85, row0Y, 32, 32};

        float row1Y = rowStart + rowH;
        Rectangle btnPortal = {controlX, row1Y, 85, 32};

        float row2Y = rowStart + 2 * rowH;
        Rectangle btnItem = {controlX, row2Y, 85, 32};

        float row3Y = rowStart + 3 * rowH;
        Rectangle btnShield = {controlX, row3Y, 85, 32};

        float keySectionY = rowStart + 4 * rowH + 8;
        Rectangle keyBtns[4];
        for (int i = 0; i < 4; i++) {
            keyBtns[i] = {panelX + 30, keySectionY + 34 + i * 46.0f, panelW - 60, 40};
        }

        Rectangle btnDone = {panelX + panelW / 2 - 70, panelY + panelH - 62, 140, 42};

        // ---- Xử lý click ----
        if (clicked) {
            if (CheckCollisionPointRec(mouse, btnLeft) && numPlayers > 1) numPlayers--;
            if (CheckCollisionPointRec(mouse, btnRight) && numPlayers < 4) numPlayers++;
            if (CheckCollisionPointRec(mouse, btnPortal)) portalsEnabled = !portalsEnabled;
            if (CheckCollisionPointRec(mouse, btnItem)) itemsEnabled = !itemsEnabled;
            if (CheckCollisionPointRec(mouse, btnShield)) shieldsEnabled = !shieldsEnabled;

            for (int i = 0; i < 4; i++) {
                if (i < numPlayers && CheckCollisionPointRec(mouse, keyBtns[i])) {
                    ShowKeyBindingScreen(configs[i].fw, configs[i].bw, configs[i].tl,
                        configs[i].tr, configs[i].sh, configs[i].shieldKey, i + 1);
                }
            }

            if (CheckCollisionPointRec(mouse, btnDone)) break;
        }

        // ---- Vẽ giao diện ----
        BeginDrawing();
        ClearBackground({40, 42, 54, 255});

        // Khung panel
        DrawRectangleRounded({panelX - 3, panelY - 3, panelW + 6, panelH + 6}, 0.03f, 10, {70, 72, 90, 255});
        DrawRectangleRounded({panelX, panelY, panelW, panelH}, 0.03f, 10, {248, 248, 252, 255});

        // Tiêu đề + bánh răng
        const char* title = "CAI DAT";
        int titleW = MeasureGameText(title, 32);
        float titleX = SCREEN_WIDTH / 2.0f - titleW / 2.0f;
        DrawGearIcon(titleX - 28, panelY + 38, 13.0f, {70, 72, 100, 255}, {248, 248, 252, 255});
        DrawGameText(title, titleX, panelY + 22, 32, {60, 62, 82, 255});
        DrawLine((int)labelX, (int)(panelY + 68), (int)(panelX + panelW - 40), (int)(panelY + 68), {210, 210, 220, 255});

        // ---- Row 0: Số lượng người chơi ----
        DrawGameText("So luong nguoi choi:", labelX, row0Y + 7, 20, {50, 52, 62, 255});

        bool hL = CheckCollisionPointRec(mouse, btnLeft);
        DrawRectangleRounded(btnLeft, 0.3f, 8, hL ? Color{190, 195, 210, 255} : Color{215, 218, 228, 255});
        DrawGameText("<", btnLeft.x + 10, btnLeft.y + 5, 22, {50, 52, 62, 255});

        const char* numStr = TextFormat("%d", numPlayers);
        int nw = MeasureGameText(numStr, 26);
        DrawGameText(numStr, controlX + 48 - nw / 2.0f, row0Y + 5, 26, {30, 100, 200, 255});

        bool hR = CheckCollisionPointRec(mouse, btnRight);
        DrawRectangleRounded(btnRight, 0.3f, 8, hR ? Color{190, 195, 210, 255} : Color{215, 218, 228, 255});
        DrawGameText(">", btnRight.x + 10, btnRight.y + 5, 22, {50, 52, 62, 255});

        // ---- Row 1: Cổng dịch chuyển ----
        DrawGameText("Cong dich chuyen:", labelX, row1Y + 7, 20, {50, 52, 62, 255});
        bool hPortal = CheckCollisionPointRec(mouse, btnPortal);
        Color pColor = portalsEnabled ? Color{50, 170, 70, 255} : Color{190, 55, 55, 255};
        if (hPortal) { pColor.r = (unsigned char)fminf(pColor.r + 25, 255); pColor.g = (unsigned char)fminf(pColor.g + 25, 255); pColor.b = (unsigned char)fminf(pColor.b + 25, 255); }
        DrawRectangleRounded(btnPortal, 0.4f, 10, pColor);
        const char* pTxt = portalsEnabled ? "BAT" : "TAT";
        int ptw = MeasureGameText(pTxt, 18);
        DrawGameText(pTxt, btnPortal.x + btnPortal.width / 2 - ptw / 2.0f, btnPortal.y + 8, 18, WHITE);

        // ---- Row 2: Vật phẩm ----
        DrawGameText("Vat pham:", labelX, row2Y + 7, 20, {50, 52, 62, 255});
        bool hItem = CheckCollisionPointRec(mouse, btnItem);
        Color iColor = itemsEnabled ? Color{50, 170, 70, 255} : Color{190, 55, 55, 255};
        if (hItem) { iColor.r = (unsigned char)fminf(iColor.r + 25, 255); iColor.g = (unsigned char)fminf(iColor.g + 25, 255); iColor.b = (unsigned char)fminf(iColor.b + 25, 255); }
        DrawRectangleRounded(btnItem, 0.4f, 10, iColor);
        const char* iTxt = itemsEnabled ? "BAT" : "TAT";
        int itw = MeasureGameText(iTxt, 18);
        DrawGameText(iTxt, btnItem.x + btnItem.width / 2 - itw / 2.0f, btnItem.y + 8, 18, WHITE);

        // ---- Row 3: Khiên ----
        DrawGameText("Khien:", labelX, row3Y + 7, 20, {50, 52, 62, 255});
        bool hShield = CheckCollisionPointRec(mouse, btnShield);
        Color sColor = shieldsEnabled ? Color{50, 170, 70, 255} : Color{190, 55, 55, 255};
        if (hShield) { sColor.r = (unsigned char)fminf(sColor.r + 25, 255); sColor.g = (unsigned char)fminf(sColor.g + 25, 255); sColor.b = (unsigned char)fminf(sColor.b + 25, 255); }
        DrawRectangleRounded(btnShield, 0.4f, 10, sColor);
        const char* sTxt = shieldsEnabled ? "BAT" : "TAT";
        int stw = MeasureGameText(sTxt, 18);
        DrawGameText(sTxt, btnShield.x + btnShield.width / 2 - stw / 2.0f, btnShield.y + 8, 18, WHITE);

        // ---- Section: Phím điều khiển ----
        const char* keyTitle = "PHIM DIEU KHIEN";
        int ktw = MeasureGameText(keyTitle, 20);
        DrawGameText(keyTitle, SCREEN_WIDTH / 2.0f - ktw / 2.0f, keySectionY, 20, {60, 62, 82, 255});
        DrawLine((int)labelX, (int)(keySectionY + 28), (int)(panelX + panelW - 40), (int)(keySectionY + 28), {210, 210, 220, 255});

        for (int i = 0; i < 4; i++) {
            bool active = (i < numPlayers);
            bool hKey = active && CheckCollisionPointRec(mouse, keyBtns[i]);

            Color btnBg, btnBorder, txtColor;
            if (active) {
                btnBg = hKey ? Color{210, 225, 245, 255} : Color{232, 238, 248, 255};
                btnBorder = playerColors[i];
                txtColor = playerColors[i];
            } else {
                btnBg = {235, 235, 238, 255};
                btnBorder = {185, 185, 190, 255};
                txtColor = {165, 165, 170, 255};
            }

            DrawRectangleRounded(keyBtns[i], 0.15f, 8, btnBg);
            DrawRectangleRoundedLinesEx(keyBtns[i], 0.15f, 8, 2.0f, btnBorder);

            // Tên người chơi
            DrawGameText(TextFormat("Nguoi choi %d", i + 1), keyBtns[i].x + 15, keyBtns[i].y + 11, 18, txtColor);

            // Tóm tắt phím hiện tại
            if (active) {
                char summary[96] = "";
                const char* sep = "";
                int keyVals[] = {configs[i].fw, configs[i].bw, configs[i].tl, configs[i].tr, configs[i].sh, configs[i].shieldKey};
                for (int k = 0; k < 6; k++) {
                    strcat(summary, sep);
                    strcat(summary, GetKeyDisplayName(keyVals[k]));
                    sep = "/";
                }
                int sw = MeasureGameText(summary, 13);
                DrawGameText(summary, keyBtns[i].x + keyBtns[i].width - sw - 15, keyBtns[i].y + 14, 13, {120, 125, 140, 255});
            }
        }

        // ---- Nút XONG ----
        bool hDone = CheckCollisionPointRec(mouse, btnDone);
        DrawRectangleRounded(btnDone, 0.3f, 10, hDone ? Color{35, 85, 180, 255} : Color{55, 105, 210, 255});
        const char* doneTxt = "XONG";
        int dw = MeasureGameText(doneTxt, 22);
        DrawGameText(doneTxt, btnDone.x + btnDone.width / 2 - dw / 2.0f, btnDone.y + 11, 22, WHITE);

        // Hint ESC
        const char* escHint = "ESC de dong";
        int ehw = MeasureGameText(escHint, 14);
        DrawGameText(escHint, SCREEN_WIDTH / 2.0f - ehw / 2.0f, panelY + panelH - 18, 14, {150, 152, 165, 255});

        EndDrawing();
    }

    SetExitKey(KEY_ESCAPE);
}