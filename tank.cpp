#include "tank.h"

/**
 * @brief Tạo xe tăng vật lý trong môi trường Box2D.
 *
 * Hình dáng vật lý (RigidBody) của xe tăng gồm 2 khối (Fixture) ghép lại:
 * 1 khối chữ nhật to làm Thân (Hull) và 1 khối nhỏ dài làm Nòng (Barrel).
 */
Tank::Tank(b2World& world, int _playerIndex) {
    playerIndex = _playerIndex;
    shootCooldownTimer = 0.0f;
    isDestroyed = false;
    currentWeapon = ItemType::NORMAL;
    ammo = 0;
    hasShield = false;
    shieldTimer = 0.0f;
    shieldCooldownTimer = 0.0f;

    b2BodyDef tankDef;
    tankDef.type = b2_dynamicBody;
    tankDef.position.Set((SCREEN_WIDTH / 2.0f) / SCALE,
        (SCREEN_HEIGHT / 2.0f) / SCALE);
    tankDef.fixedRotation = false;
    body = world.CreateBody(&tankDef);

    b2PolygonShape hullShape;
    hullShape.SetAsBox(14.0f / SCALE, 14.0f / SCALE, b2Vec2(0.0f, -7.0f / SCALE),
        0.0f);
    b2FixtureDef hullFix;
    hullFix.shape = &hullShape;
    hullFix.density = 1.0f;
    hullFix.friction = 0.0f;
    hullFix.restitution = 0.0f;
    body->CreateFixture(&hullFix);

    b2PolygonShape barrelShape;
    barrelShape.SetAsBox(3.0f / SCALE, 7.0f / SCALE, b2Vec2(0.0f, 14.0f / SCALE),
        0.0f);
    b2FixtureDef barrelFix;
    barrelFix.shape = &barrelShape;
    barrelFix.density = 0.2f;
    barrelFix.friction = 0.0f;
    barrelFix.restitution = 0.0f;
    body->CreateFixture(&barrelFix);
}

/**
 * @brief Callback RayCast kiểm tra tường (dùng cho laser sight và spawn đạn)
 */
class WallRayCastCallback : public b2RayCastCallback {
public:
    bool hitWall = false;
    b2Vec2 hitPoint;
    float closestFraction = 1.0f;
    float ReportFixture(b2Fixture* fixture, const b2Vec2& point,
        const b2Vec2& normal, float fraction) override {
        if (fixture->GetBody()->GetType() == b2_staticBody) {
            hitWall = true;
            if (fraction < closestFraction) {
                closestFraction = fraction;
                hitPoint = point;
            }
            return fraction;
        }
        return -1.0f;
    }
};

/**
 * @brief Cập nhật trạng thái xe tăng (1 Frame) theo Action truyền vào.
 *
 * ĐIỂM CHÚ Ý CHO RL:
 * Hàm này KHÔNG check "Nếu phím W được bấm". Nó check `actions.forward`.
 * Nghĩa là nếu File Python RL truyền `actions.forward = true` vào C++,
 * xe tăng sẽ nhận lệnh và tiến lên y như người thật đang chơi.
 */
void Tank::Update(b2World& world, std::vector<Bullet*>& bullets,
    std::vector<Item*>& items, const TankActions& actions,
    float dt, bool shieldsEnabled) {
    if (shieldCooldownTimer > 0.0f)
        shieldCooldownTimer -= dt;
    if (shieldTimer > 0.0f) {
        shieldTimer -= dt;
        if (shieldTimer <= 0.0f)
            hasShield = false;
    }
    if (shootCooldownTimer > 0.0f)
        shootCooldownTimer -= dt;

    if (shieldsEnabled && actions.shield && shieldCooldownTimer <= 0.0f) {
        hasShield = true;
        shieldTimer = 5.0f;
        shieldCooldownTimer = 15.0f;
    }

    HandleMovement(actions);
    FireWeapon(world, bullets, actions);
    CheckCollisions(bullets, items);
}

/**
 * @brief Di chuyển xe tăng dựa trên TankActions (forward/backward/turn).
 */
void Tank::HandleMovement(const TankActions& actions) {
    float moveSpeed = 3.0f, turnSpeed = 3.0f;
    float angularVel = 0.0f;

    if (actions.turnLeft)
        angularVel += turnSpeed;
    if (actions.turnRight)
        angularVel -= turnSpeed;
    body->SetAngularVelocity(angularVel);

    // Vector vận tốc (velocity) mặc định là (0,0)
    b2Vec2 vel(0.0f, 0.0f);

    // Tính toán Vector Hướng Mũi Xe (Forward Direction). 
    // Góc 0 độ của Box2D là hướng sang Phải (Trục X). Do thiết kế màn hình, 
    // ta dùng sin/cos để tìm ra vector đơn vị chỉ chuẩn hướng mũi xe.
    float currentAngle = body->GetAngle();
    b2Vec2 forwardDir(-sinf(currentAngle), cosf(currentAngle));

    // Áp dụng Vận tốc Tịnh tiến (Vectơ Tốc độ)
    if (actions.forward) {
        vel.x += forwardDir.x * moveSpeed;
        vel.y += forwardDir.y * moveSpeed;
    }
    if (actions.backward) {
        vel.x -= forwardDir.x * moveSpeed;
        vel.y -= forwardDir.y * moveSpeed;
    }

    // Đưa lệnh vận tốc vào Engine Vật Lý. 
    // Sau lệnh này, phương thức world.Step() ở Game::Update() sẽ tính toán va chạm.
    body->SetLinearVelocity(vel);
}

/**
 * @brief Bắn đạn theo vũ khí hiện tại khi actions.shoot = true.
 */
void Tank::FireWeapon(b2World& world, std::vector<Bullet*>& bullets,
    const TankActions& actions) {
    int activeMyBullets = 0;
    for (Bullet* b : bullets) {
        if (b->ownerPlayerIndex == playerIndex && !b->isMissile && !b->isFrag)
            activeMyBullets++;
    }

    if (actions.shoot && shootCooldownTimer <= 0.0f) {
        float currentAngle = body->GetAngle();
        b2Vec2 forwardDir(-sinf(currentAngle), cosf(currentAngle));
        b2Vec2 startPos = body->GetPosition();
        b2Vec2 spawnPos = startPos + (30.0f / SCALE) * forwardDir;

        // Kỹ thuật Box2D RayCast: Tránh lỗi bắn đạn xuyên tường
        // Bắn 1 tia (Ray) từ tâm xe tăng ra ngoài mũi xe. Nếu khoảng cách đó
        // có vật thể (tường), tia sẽ chặn lại. Ta dùng kết quả để cấm bắn đạn
        // khi miệng nòng súng đang cắm thẳng vào tường.
        WallRayCastCallback callback;
        world.RayCast(&callback, startPos, spawnPos);

        if (!callback.hitWall) {
            // Kiểm tra frag đang chờ nổ
            bool hasFragActive = false;
            for (Bullet* b : bullets) {
                if (b->isFrag && b->ownerPlayerIndex == playerIndex &&
                    !b->explodeFrag) {
                    b->explodeFrag = true;
                    hasFragActive = true;
                    if (currentWeapon == ItemType::FRAG && ammo > 0) {
                        ammo--;
                        if (ammo <= 0)
                            currentWeapon = ItemType::NORMAL;
                    }
                    shootCooldownTimer = 0.5f;
                    break;
                }
            }

            if (!hasFragActive) {
                switch (currentWeapon) {
                case ItemType::GATLING: {
                    for (int i = 0; i < 5; i++) {
                        float angleOffset = (i - 2) * 0.15f;
                        float cosA = cosf(angleOffset), sinA = sinf(angleOffset);
                        b2Vec2 dir(forwardDir.x * cosA - forwardDir.y * sinA,
                            forwardDir.x * sinA + forwardDir.y * cosA);
                        Bullet* b = new Bullet(world, spawnPos, 10.0f * dir, false, false,
                            false, playerIndex);
                        b->time = 3.0f;
                        bullets.push_back(b);
                    }
                    shootCooldownTimer = 0.5f;
                    break;
                }
                case ItemType::FRAG: {
                    bullets.push_back(new Bullet(world, spawnPos, 5.0f * forwardDir,
                        false, true, false, playerIndex));
                    shootCooldownTimer = 0.5f;
                    break;
                }
                case ItemType::MISSILE: {
                    bullets.push_back(new Bullet(world, spawnPos, 4.5f * forwardDir,
                        false, false, true, playerIndex));
                    shootCooldownTimer = 0.5f;
                    break;
                }
                case ItemType::DEATH_RAY: {
                    bullets.push_back(new Bullet(world, spawnPos, 8.0f * forwardDir, true,
                        false, false, playerIndex));
                    shootCooldownTimer = 0.5f;
                    break;
                }
                default: {
                    if (activeMyBullets < 1) {
                        bullets.push_back(new Bullet(world, spawnPos, 6.0f * forwardDir,
                            false, false, false, playerIndex));
                        shootCooldownTimer = 0.15f;
                    }
                    break;
                }
                }

                if (currentWeapon != ItemType::NORMAL &&
                    currentWeapon != ItemType::FRAG) {
                    ammo--;
                    if (ammo <= 0)
                        currentWeapon = ItemType::NORMAL;
                }
            }
        }
    }
}

/**
 * @brief Kiểm tra va chạm: đạn trúng → chết/mất khiên, item trúng → nhặt vũ
 * khí.
 */
void Tank::CheckCollisions(std::vector<Bullet*>& bullets,
    std::vector<Item*>& items) {
    for (b2ContactEdge* edge = body->GetContactList(); edge; edge = edge->next) {
        if (edge->contact->IsTouching()) {
            b2Body* otherBody = edge->other;

            for (Bullet* bullet : bullets) {
                if (otherBody == bullet->body) {
                    bullet->time = 0.0f;
                    if (hasShield) {
                        hasShield = false;
                        shieldTimer = 0.0f;
                    }
                    else {
                        isDestroyed = true;
                    }
                }
            }

            for (Item* item : items) {
                if (otherBody == item->body && !item->isDestroyed) {
                    item->isDestroyed = true;
                    currentWeapon = item->type;
                    if (currentWeapon == ItemType::GATLING)
                        ammo = 3;
                    else if (currentWeapon == ItemType::FRAG)
                        ammo = 1;
                    else if (currentWeapon == ItemType::MISSILE)
                        ammo = 1;
                    else if (currentWeapon == ItemType::DEATH_RAY)
                        ammo = 1;
                    else
                        ammo = 0;
                }
            }
        }
    }
}
