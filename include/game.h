#pragma once
#include "bullet.h"
#include "Constants.h"
#include "item.h"
#include "map.h"
#include "portal.h"
#include "tank.h"

/**
 * @class Game
 * @brief Quản lý toàn bộ trạng thái và logic game. KHÔNG phụ thuộc Raylib.
 *
 * Thiết kế cho cả human play và RL training:
 * - Human play: main.cpp đọc bàn phím → TankActions → Game::Update()
 * - RL train:   agent output → TankActions → Game::Update() (không cần cửa sổ
 * đồ họa)
 *
 * Mọi thành viên public để Renderer và RL agent có thể đọc trạng thái.
 */
class Game {
public:
  // ---- Thành phần vật lý & game objects ----
  b2World world;                 ///< Môi trường mô phỏng Box2D (trọng lực 0)
  std::vector<Tank *> tanks;     ///< Xe tăng đang sống
  std::vector<Bullet *> bullets; ///< Đạn đang bay
  std::vector<Item *> items;     ///< Hộp vũ khí trên sàn
  GameMap map;                   ///< Hệ thống mê cung
  Portal portal;                 ///< Cổng dịch chuyển

  // ---- Thông số & cài đặt ----
  float itemSpawnTimer; ///< Đếm ngược sinh vật phẩm
  int playerScores[4];  ///< Bảng điểm 4 slot
  int numPlayers;       ///< Số lượng người chơi
  bool needsRestart;    ///< Cờ cần reset match
  bool portalsEnabled;  ///< Bật/tắt cổng dịch chuyển
  bool itemsEnabled;    ///< Bật/tắt vật phẩm
  bool shieldsEnabled;  ///< Bật/tắt khiên

  // ---- Cấu hình phím (chỉ dùng cho human play) ----
  std::vector<PlayerConfig> configs;

  // ---- Sự kiện dùng cho hiệu ứng đồ họa (Renderer đọc) ----
  std::vector<DeathEvent> recentDeaths; ///< Xe tăng bị tiêu diệt frame này

  // ---- Lifecycle ----
  Game();
  ~Game();

  /// Khởi tạo bàn đấu mới: dọn sạch, sinh map, spawn xe tăng
  void ResetMatch();

  /// Cập nhật logic game 1 frame. Actions[i] tương ứng với player index i.
  void Update(const std::vector<TankActions> &actions, float dt);

  /// Dọn dẹp đạn hết hạn, xử lý nổ Frag
  void CleanUpBullets();

  /// Dọn dẹp vật phẩm đã bị nhặt
  void CleanUpItems();
};