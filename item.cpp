#include "item.h"

/**
 * @brief Tạo hộp vũ khí tại vị trí chỉ định. Sensor body (không ngăn di chuyển).
 */
Item::Item(b2World& world, b2Vec2 position, ItemType _type) {
    type = _type;
    isDestroyed = false;

    b2BodyDef def;
    def.type = b2_staticBody;
    def.position = position;
    body = world.CreateBody(&def);

    b2PolygonShape shape;
    shape.SetAsBox(10.0f / SCALE, 10.0f / SCALE);

    b2FixtureDef fix;
    fix.shape = &shape;
    fix.isSensor = true;
    body->CreateFixture(&fix);
}
