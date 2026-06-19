#pragma once
#include "constants.h"

/**
 * @enum ItemType
 * @brief Các loại vũ khí và sức mạnh trong trò chơi
 */
enum class ItemType {
    NORMAL,     ///< Đạn bình thường
    GATLING,    ///< Shotgun: bắn 5 viên chùm
    FRAG,       ///< Mìn nổ chùm: đặt mìn rồi kích nổ thành 8 mảnh
    MISSILE,    ///< Tên lửa đuổi: tự tìm mục tiêu
    DEATH_RAY   ///< Laser: tốc độ cực cao
};

/**
 * @class Item
 * @brief Hộp vũ khí rớt trên sàn đấu. Logic thuần, không đồ họa.
 */
class Item {
public:
    b2Body* body;           ///< Vật lý Box2D (sensor, không cản vật lý)
    ItemType type;          ///< Loại vũ khí trong hộp
    bool isDestroyed;       ///< Cờ đã bị nhặt

    Item(b2World& world, b2Vec2 position, ItemType _type);
};
