#pragma once
#include <box2d/box2d.h>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>

using namespace std;

// ========================================================================
// Hằng số toán học cơ sở (Hoạt động hoàn toàn độc lập với thư viện đồ họa Raylib)
// Việc tự định nghĩa các hằng số này giúp Core Logic của Game không bị phụ thuộc
// vào bất kỳ bộ công cụ vẽ hình nào, cực kỳ tối ưu khi chạy không màn hình (Headless / RL).
// ========================================================================
#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef DEG2RAD
#define DEG2RAD (PI / 180.0f)
#endif
#ifndef RAD2DEG
#define RAD2DEG (180.0f / PI)
#endif

// ========================================================================
// Thông số Thế giới Vật lý (Box2D)
// ------------------------------------------------------------------------
// Box2D là Engine vật lý dùng hệ mét (metters), trong khi màn hình dùng Pixel.
// SCALE chính là tỷ lệ quy đổi. Ví dụ: SCALE = 30.0f nghĩa là 30 pixels đồ họa
// tương đương 1 mét trong mô phỏng vật lý Box2D.
// ========================================================================
const float SCALE = 30.0f;         // Tỷ lệ pixel (Raylib) <-> mét (Box2D)
const int SCREEN_WIDTH = 1024;     // Chiều rộng thế giới game
const int SCREEN_HEIGHT = 768;     // Chiều cao thế giới game

/**
 * @struct PlayerConfig
 * @brief Lưu mã phím cấu hình cho từng người chơi (chỉ dùng ở chế độ human play)
 */
struct PlayerConfig { int fw, bw, tl, tr, sh, shieldKey; };

/**
 * @struct TankActions
 * @brief Đại diện cho "Hành động" (Action Space) của Xe Tăng.
 * 
 * Đây là chìa khóa của Kiến trúc AI-Ready.
 * Thay vì để class Xe tăng tự động lệnh đọc bàn phím (như IsKeyDown(KEY_W)),
 * ta tạo một Struct chứa thuần túy logic ĐÚNG / SAI.
 * 
 * - Nếu người chơi người bấm: main.cpp quét phím rồi set biến thành `true`.
 * - Nếu AI (RL) chơi: Mạng Neural ra dự đoán [1, 0, 0, 1, 0, 1], ta set các biến tương ứng.
 * Sự tách biệt này giúp AI hay Con người đều điều khiển chung 1 logic game cốt lõi.
 */
struct TankActions {
    bool forward = false;
    bool backward = false;
    bool turnLeft = false;
    bool turnRight = false;
    bool shoot = false;
    bool shield = false;
};

/**
 * @struct DeathEvent
 * @brief Ghi nhận lại vị trí xe tăng tử nạn để phục vụ riêng cho khâu "Vẽ Đồ họa".
 * 
 * Nếu để Game Logic tự vẽ "Vụ nổ lửa" (Explosion), thư viện Box2D sẽ bị dính với Raylib.
 * Giải pháp thiết kế tối ưu: Game Logic chỉ ghi lại (Log) tọa độ chỗ nào có xe chết.
 * Sau đó, Renderer (nếu đang chạy có màn hình) sẽ đọc cái Log này và vẽ vụ nổ.
 * Nếu đang train AI không màn hình (Headless), danh sách này được clear đi mà không lỗi lầm gì.
 */
struct DeathEvent {
    b2Vec2 position;
    int playerIndex;
};