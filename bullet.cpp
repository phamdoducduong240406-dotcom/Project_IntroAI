#include "bullet.h"
#include "tank.h"

/**
 * @brief Tạo viên đạn với thông số tùy theo loại vũ khí.
 */
Bullet::Bullet(b2World& world, b2Vec2 position, b2Vec2 velocity, bool _isLaser,
    bool _isFrag, bool _isMissile, int _owner) {
    isLaser = _isLaser;
    isFrag = _isFrag;
    isMissile = _isMissile;
    explodeFrag = false;
    ownerPlayerIndex = _owner;
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = position;
    def.bullet = true;
    time = 7.0f;
    body = world.CreateBody(&def);

    b2CircleShape shape;
    shape.m_radius = (isFrag || isMissile) ? 5.0f / SCALE : 3.0f / SCALE;
    b2FixtureDef fix;
    fix.shape = &shape;
    fix.density = 1.0f;
    fix.friction = 0.0f;
    fix.restitution = 1.0f;
    fix.filter.groupIndex = -1;
    body->CreateFixture(&fix);

    if (isLaser) {
        velocity.x *= 8.0f; // Laser đi cực nhanh
        velocity.y *= 8.0f;
        time = 1.5f; // laser tồn tại ngắn
    }
    else if (isMissile) {
        velocity.x *= 1.5f;
        velocity.y *= 1.5f;
        time = 7.0f; // Rút ngắn thời gian tồn tại tên lửa
    }
    else if (isFrag) {
        time = 2.0f; // Đạn to tự động nổ sau 1.5 giây nếu không kích
    }

    body->SetLinearVelocity(velocity);
}

/**
 * @brief Cập nhật đạn: giảm thời gian tồn tại, xử lý logic tên lửa đuổi.
 */
void Bullet::Update(float dt, const std::vector<Tank*>& tanks) {
    time -= dt;
    if (IsDead())
        return;

    // Logic xử lý đạn đuổi tìm mục tiêu (Tên lửa)
    if (isMissile) {
        float elapsed = 5.0f - time;
        b2Vec2 currentVel = body->GetLinearVelocity();
        float currentSpeed = currentVel.Length();

        if (currentSpeed > 0.0f) {
            if (elapsed < 2.0f) {
                // Trong 2.0s đầu tiên, đạn lượn lờ hình sin
                float waveTurn = cosf(elapsed * 12.0f) * 4.0f * dt;
                float angle = atan2f(currentVel.y, currentVel.x) + waveTurn;
                body->SetLinearVelocity(
                    b2Vec2(cosf(angle) * currentSpeed, sinf(angle) * currentSpeed));
            }
            else {
                // Sau 2.0s, tìm kiếm xe tăng gần nhất
                Tank* target = nullptr;
                float minDist = 9999.0f;
                for (Tank* t : tanks) {
                    if (!t->isDestroyed) {
                        float dist =
                            (t->body->GetPosition() - body->GetPosition()).Length();
                        if (dist < minDist) {
                            minDist = dist;
                            target = t;
                        }
                    }
                }

                // Thuật toán điều hướng dần dần về phía mục tiêu
                if (target) {
                    b2Vec2 toTarget = target->body->GetPosition() - body->GetPosition();
                    toTarget.Normalize();
                    b2Vec2 normVel = currentVel;
                    normVel.Normalize();

                    float cross = normVel.x * toTarget.y - normVel.y * toTarget.x;
                    float turnSpeed = 3.5f * dt;

                    float angle = atan2f(normVel.y, normVel.x);
                    if (cross > 0.1f)
                        angle += turnSpeed;
                    else if (cross < -0.1f)
                        angle -= turnSpeed;

                    body->SetLinearVelocity(
                        b2Vec2(cosf(angle) * currentSpeed, sinf(angle) * currentSpeed));
                }
            }
        }
    }
}

bool Bullet::IsDead() const { return time <= 0.0f || explodeFrag; }
