#pragma once
#include "constants.h"

/**
 * @class UI
 * @brief Lớp tĩnh quản lý giao diện người dùng (HUD, Settings). Phụ thuộc Raylib.
 */
class UI {
public:
    /// Khởi tạo font tùy chỉnh (gọi SAU InitWindow)
    static void Init();

    /// Giải phóng font (gọi TRƯỚC CloseWindow)
    static void Cleanup();

    /// Mở màn hình Cài đặt tổng hợp
    static void ShowSettingsScreen(int& numPlayers, bool& portalsEnabled, bool& itemsEnabled,
        bool& shieldsEnabled, std::vector<PlayerConfig>& configs,
        std::vector<bool>& isBot, std::vector<bool>& isAI, std::vector<bool>& isAstar);

    /// Mở màn hình gán phím cho 1 người chơi (6 phím)
    static void ShowKeyBindingScreen(int& fw, int& bw, int& tl, int& tr, int& sh, int& shield, int playerIndex);

    /// Vẽ HUD: nút bánh răng + bảng điểm
    static void DrawHUD(const int playerScores[], int numPlayers);

    /// Kiểm tra click vào nút bánh răng
    static bool CheckSettingsButtonClicked();
};