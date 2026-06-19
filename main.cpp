#include <raylib.h>
#include <ctime>
#include <cstdlib>
#include "game.h"
#include "renderer.h"
#include "ui.h"
#include "bot.h"
#include "ai_bot.h"
#include "Astar/astar_bot.h"

#ifdef _WIN32
// Khai báo trực tiếp 2 hàm Win32 cần dùng thay vì #include <windows.h>
// để tránh xung đột tên (Rectangle, CloseWindow, ShowCursor) với Raylib.
extern "C" {
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
    __declspec(dllimport) int __stdcall SetCurrentDirectoryA(const char* lpPathName);
}
#define MAX_PATH 260
#endif

/**
 * @brief Entry point cho chế độ human play (có đồ họa).
 * 
 * Kiến trúc:
 * - Game: quản lý logic thuần (Box2D), KHÔNG phụ thuộc Raylib
 * - Renderer: vẽ thế giới game
 * - UI: giao diện cài đặt + HUD
 * - main.cpp: kết nối input (bàn phím → TankActions) và rendering
 * 
 * Để train RL, viết main khác: chỉ dùng Game + TankActions, không cần Renderer/UI.
 */
int main() {
    // ================================================================
    // Tự động chuyển CWD về thư mục chứa file exe.
    // Khi exe nằm trong build/, các đường dẫn fallback "../fonts/",
    // "../models/" sẽ đúng trỏ về thư mục gốc dự án.
    // Giải quyết lỗi "không tìm thấy file" khi chạy từ VS Code.
    // ================================================================
#ifdef _WIN32
    {
        char exePath[MAX_PATH];
        unsigned long len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            char* lastSlash = strrchr(exePath, '\\');
            if (lastSlash) {
                *lastSlash = '\0';
                SetCurrentDirectoryA(exePath);
            }
        }
    }
#endif

    srand((unsigned int)time(NULL));

    Game game;
    // Cài đặt phím mặc định cho 4 người chơi (Raylib key codes)
    game.configs[0] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_Q, KEY_E};
    game.configs[1] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SLASH, KEY_PERIOD};
    game.configs[2] = {KEY_I, KEY_K, KEY_J, KEY_L, KEY_U, KEY_O};
    game.configs[3] = {KEY_KP_8, KEY_KP_5, KEY_KP_4, KEY_KP_6, KEY_KP_7, KEY_KP_9};

    std::vector<bool> isBot   = {false, true, false, false}; // P1 là người, P2 mặc định là Bot
    std::vector<bool> isAI    = {false, false, false, false}; // Không ai là AI (neural net)
    std::vector<bool> isAstar = {false, false, false, false}; // A* pathfinding agent

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZ Game");
    SetTargetFPS(60);
    UI::Init();

    // Khởi tạo các Bot bên ngoài vòng lặp chính để chúng không bị "mất trí nhớ" mỗi frame
    std::vector<Bot*>       bots(4, nullptr);
    std::vector<AIBot*>     aiBots(4, nullptr);
    std::vector<AStarBot*>  astarBots(4, nullptr);
    if (isBot[1] && !isAI[1] && !isAstar[1]) bots[1] = new Bot(7, 1);

    while (!WindowShouldClose()) {
        // --- Xử lý Settings UI ---
        if (UI::CheckSettingsButtonClicked()) {
            int oldNumPlayers = game.numPlayers;
            UI::ShowSettingsScreen(game.numPlayers, game.portalsEnabled, game.itemsEnabled, game.shieldsEnabled, game.configs, isBot, isAI, isAstar);
            if (game.numPlayers != oldNumPlayers) {
                for (int i = 0; i < 4; i++) game.playerScores[i] = 0;
            }
            // Cập nhật lại danh sách bot nếu có thay đổi trong cài đặt
            for (int i = 0; i < 4; i++) {
                // Xóa tất cả instance cũ trước
                auto clearAll = [&]() {
                    if (bots[i])      { delete bots[i];      bots[i]      = nullptr; }
                    if (aiBots[i])   { delete aiBots[i];   aiBots[i]   = nullptr; }
                    if (astarBots[i]){ delete astarBots[i]; astarBots[i] = nullptr; }
                };
                if (isBot[i] && !isAI[i] && !isAstar[i]) {
                    clearAll();
                    bots[i] = new Bot(7, i);          // Rule-based Bot
                } else if (isBot[i] && isAstar[i]) {
                    clearAll();
                    astarBots[i] = new AStarBot(i);   // A* Demo Bot
                } else if (isBot[i] && isAI[i]) {
                    clearAll();
                    aiBots[i] = new AIBot(i);          // Neural Network Bot
                } else {
                    clearAll();                        // Người chơi
                }
            }
            game.needsRestart = true;
        }

        if (game.needsRestart) {
            game.ResetMatch();
            // Reset trạng thái bot khi bắt đầu trận mới
            // Dùng requestPathClear thay vì trực tiếp xoá cachedPath (thread-safe)
            for (int i = 0; i < 4; i++) {
                if (bots[i]) {
                    bots[i]->requestPathClear = true;
                    bots[i]->lastEnemyPos = b2Vec2(0, 0);
                    bots[i]->stuckCounter = 0;
                    bots[i]->idleCounter  = 0;
                }
                if (astarBots[i]) {
                    astarBots[i]->requestPathClear = true;
                    astarBots[i]->lastEnemyPos = b2Vec2(0, 0);
                    astarBots[i]->stuckCounter = 0;
                    astarBots[i]->cachedPath.clear();
                    astarBots[i]->waypointIdx = 0;
                }
            }
        }

        // --- Xử lý Input / Bot AI ---
        std::vector<TankActions> actions(game.numPlayers);
        for (int i = 0; i < game.numPlayers; i++) {
            if (isBot[i] && isAI[i] && aiBots[i]) {
                // AI Neural Network Bot
                actions[i] = aiBots[i]->GetAction(&game);
            } else if (isBot[i] && isAstar[i] && astarBots[i]) {
                // A* Pathfinding Demo Bot
                actions[i] = astarBots[i]->GetAction(&game);
            } else if (isBot[i] && bots[i]) {
                // Rule-based Bot
                actions[i] = bots[i]->GetAction(&game);
            } else {
                // Người chơi
                PlayerConfig& cfg = game.configs[i];
                actions[i].forward    = IsKeyDown(cfg.fw);
                actions[i].backward   = IsKeyDown(cfg.bw);
                actions[i].turnLeft   = IsKeyDown(cfg.tl);
                actions[i].turnRight  = IsKeyDown(cfg.tr);
                actions[i].shoot      = IsKeyPressed(cfg.sh);
                actions[i].shield     = IsKeyPressed(cfg.shieldKey);
            }
        }

        // --- Cập nhật logic game ---
        float dt = GetFrameTime();
        game.Update(actions, dt);

        // --- Cập nhật hiệu ứng đồ họa (explosion particles) ---
        Renderer::Update(game, dt);

        // --- Render ---
        BeginDrawing();
        ClearBackground({245, 240, 230, 255}); // Nền beige ấm
        Renderer::DrawWorld(game);
        UI::DrawHUD(game.playerScores, game.numPlayers);
        EndDrawing();
    }

    // Dọn dẹp bộ nhớ
    for (int i = 0; i < 4; i++) {
        if (bots[i])      delete bots[i];
        if (aiBots[i])   delete aiBots[i];
        if (astarBots[i]) delete astarBots[i];
    }
    UI::Cleanup();
    CloseWindow();
    return 0;
}