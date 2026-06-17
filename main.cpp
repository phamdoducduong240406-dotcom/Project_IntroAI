#include <raylib.h>
#include <ctime>
#include <cstdlib>
#include "game.h"
#include "renderer.h"
#include "ui.h"

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
    srand((unsigned int)time(NULL));

    Game game;
    // Cài đặt phím mặc định cho 4 người chơi (Raylib key codes)
    game.configs[0] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_Q, KEY_E};
    game.configs[1] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SLASH, KEY_PERIOD};
    game.configs[2] = {KEY_I, KEY_K, KEY_J, KEY_L, KEY_U, KEY_O};
    game.configs[3] = {KEY_KP_8, KEY_KP_5, KEY_KP_4, KEY_KP_6, KEY_KP_7, KEY_KP_9};

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZ Game");
    SetTargetFPS(60);
    UI::Init();

    while (!WindowShouldClose()) {
        // --- Xử lý Settings UI --- 
        game.itemsEnabled = 0;
        game.portalsEnabled = 0;
        game.shieldsEnabled = 0;
        if (UI::CheckSettingsButtonClicked()) {
            int oldNumPlayers = game.numPlayers;

           

            UI::ShowSettingsScreen(game.numPlayers, game.portalsEnabled, game.itemsEnabled, game.shieldsEnabled, game.configs);
            if (game.numPlayers != oldNumPlayers) {
                for (int i = 0; i < 4; i++) game.playerScores[i] = 0;
            }
            game.needsRestart = true;
        }

        if (game.needsRestart) game.ResetMatch();

        // -------------------------------------------------------------
        // BƯỚC QUAN TRỌNG CHỐT LÕI (AI-READY DECISION)
        // -------------------------------------------------------------
        // Thay vì quăng thẳng Raylib Key Event vào trong Logic Game,
        // Ta thu thập dữ liệu bàn phím và đóng gói thành struct TankActions.
        // 
        // LỢI ÍCH: Nếu bạn chạy môi trường AI (RL), bạn sẽ XÓA TOÀN BỘ FILE NÀY
        // (chỉ giữ lại `game.cpp/h`) và viết 1 file Python/C++ mới, không cần Raylib.
        // Trong đó, mảng `actions` sẽ do Mạng Neural Network sinh ra (Dự đoán!).
        // -------------------------------------------------------------
        std::vector<TankActions> actions(game.numPlayers);
        for (int i = 0; i < game.numPlayers; i++) {
            PlayerConfig& cfg = game.configs[i];
            actions[i].forward = IsKeyDown(cfg.fw);
            actions[i].backward = IsKeyDown(cfg.bw);
            actions[i].turnLeft = IsKeyDown(cfg.tl);
            actions[i].turnRight = IsKeyDown(cfg.tr);
            actions[i].shoot = IsKeyPressed(cfg.sh);
            actions[i].shield = IsKeyPressed(cfg.shieldKey);
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

    UI::Cleanup();
    CloseWindow();
    return 0;
}