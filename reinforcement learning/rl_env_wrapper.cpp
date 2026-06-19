#include "game.h"
#include "renderer.h"
#include <cmath>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <raylib.h>
#include <tuple>
#include <vector>
#include <random>
#include "bot.h"

namespace py = pybind11;

/**
 * @class RLEnv
 * @brief Lớp bao bọc (Wrapper) môi trường trò chơi để tương thích với các thư
 * viện Reinforcement Learning (như OpenAI Gym). Lớp này quản lý việc khởi tạo
 * trò chơi, thực hiện các bước đi (step) và thu thập trạng thái (state).
 */
class RLEnv {
private:
  Game *game;
  Renderer *renderer = nullptr;
  bool isRendering = false;
  int maxSteps;
  int currentStep;
  int lastScores[4];
  float lastDistanceToTarget;

  int trainingMode;
  float shapingFactor;
  std::vector<int> lastAction0 = {0, 0, 0};
  std::vector<int> lastAction1 = {0, 0, 0};

  // Reward shaping state
  float minDistanceReached;
  b2Vec2 posHistory[60];
  int historyCount = 0;
  int historyIndex = 0;

  // Persistent Bot instances (giữ state giữa các frame: cachedPath, evasionTimer, ...)
  Bot* bots[4] = {nullptr, nullptr, nullptr, nullptr};


  float getRawDistanceToEnemy(int playerIdx) {
    Tank *myTank = nullptr;
    Tank *enemyTank = nullptr;
    for (auto t : game->tanks) {
      if (t->playerIndex == playerIdx) {
        myTank = t;
      } else if (!t->isDestroyed) {
        enemyTank = t;
      }
    }
    if (myTank && enemyTank) {
      b2Vec2 diff =
          myTank->body->GetPosition() - enemyTank->body->GetPosition();
      return diff.Length() * SCALE; // Khoảng cách tính bằng pixel
    }
    return 1000.0f; // Trả về khoảng cách an toàn rất lớn nếu địch đã chết
  }

public:
  // Hàm khởi tạo môi trường
  RLEnv(int num_players = 2, bool map_enabled = false,
        bool items_enabled = false, int training_mode = 0, float shaping_factor = 1.0f) {
    // Tự động gieo seed ngẫu nhiên dựa trên std::random_device để tránh trùng seed giữa các tiến trình con
    std::random_device rd;
    srand(rd());

    game = new Game();                    // Tạo đối tượng Game mới
    game->numPlayers = num_players;       // Số lượng người chơi
    game->mapEnabled = map_enabled;       // Có sử dụng bản đồ (vật cản) không
    game->itemsEnabled = items_enabled;   // Có xuất hiện vật phẩm không
    game->portalsEnabled = items_enabled; // Cổng dịch chuyển
    trainingMode = training_mode;
    shapingFactor = shaping_factor;
    maxSteps = (trainingMode == 2) ? 1000 : 8000; // Tăng maxSteps lên 8000 (khoảng 133 giây) để agent có đủ thời gian tìm địch
    currentStep = 0;                      // Bước hiện tại
    for (int i = 0; i < 4; i++)
      lastScores[i] = 0; // Lưu trữ điểm số trước đó để tính phần thưởng
  }

  ~RLEnv() {
    for (int i = 0; i < 4; i++) { delete bots[i]; bots[i] = nullptr; }
    if (isRendering) {
      CloseWindow();
      delete renderer;
    }
    delete game;
  }

  bool render() {
    if (!isRendering) {
      InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZGame RL Training Watcher");
      SetTargetFPS(60); // Cap frame rate để dễ nhìn trong lúc train
      renderer = new Renderer();
      isRendering = true;
    }

    if (WindowShouldClose()) {
      CloseWindow();
      isRendering = false;
      delete renderer;
      renderer = nullptr;
      return false;
    }

    // renderer chỉ draw logic cũ, game logic đã được update trong step()
    renderer->Update(*game, 1.0f / 60.0f);

    BeginDrawing();
    ClearBackground({20, 20, 25, 255}); // Nền tối
    if (renderer)
      renderer->DrawWorld(*game);



    EndDrawing();

    return true;
  }

  /**
   * @brief Khởi tạo lại ván chơi (Reset)
   * Thường gọi khi bắt đầu ván mới hoặc khi AI bị chết/hết thời gian.
   * @return Trạng thái ban đầu của ván chơi.
   */
  std::vector<float> reset() {
    game->ResetMatch(); // Gọi hàm reset trong engine game
    currentStep = 0;
    // Cập nhật lại điểm số ban đầu
    for (int i = 0; i < 4; i++)
      lastScores[i] = game->playerScores[i];
    lastDistanceToTarget = 0.0f;
    
    // Reset reward shaping state
    minDistanceReached = 9999.0f;
    historyCount = 0;
    historyIndex = 0;

    // Reset Bot state (map mới → path cũ vô nghĩa)
    for (int i = 0; i < 4; i++) { delete bots[i]; bots[i] = nullptr; }

    return getState(0); // Trả về trạng thái của người chơi 0
  }

  void seed(long long seedVal) {
    srand(static_cast<unsigned int>(seedVal));
  }

  /**
   * @brief Thực hiện một hành động (Step) trong môi trường.
   * @param action Mã hành động (0-5) từ phía AI Python gửi sang.
   * @return Một Python Tuple chứa (Trạng thái mới, Phần thưởng, Ván chơi kết
   * thúc chưa).
   */
  py::tuple step(std::vector<int> action0,
                 std::vector<int> action1 = std::vector<int>()) {
    
    Tank *myTank = nullptr;
    Tank *enemyTank = nullptr;
    for (auto t : game->tanks) {
      if (t->playerIndex == 0) myTank = t;
      else if (!t->isDestroyed) enemyTank = t;
    }

    bool isEnemyInSight = false;
    float dotProd = 0.0f;
    if (myTank && enemyTank && !enemyTank->isDestroyed) {
      b2Vec2 forwardDir(-sinf(myTank->body->GetAngle()), cosf(myTank->body->GetAngle()));
      b2Vec2 toEnemy = enemyTank->body->GetPosition() - myTank->body->GetPosition();
      toEnemy.Normalize();
      dotProd = forwardDir.x * toEnemy.x + forwardDir.y * toEnemy.y;

      float rayLength = std::sqrt(SCREEN_WIDTH * SCREEN_WIDTH + SCREEN_HEIGHT * SCREEN_HEIGHT) / SCALE;
      b2Vec2 p2 = myTank->body->GetPosition() + rayLength * forwardDir;
      EnemyForwardRayCastCallback cbTarget(enemyTank->body);
      game->world.RayCast(&cbTarget, myTank->body->GetPosition(), p2);
      isEnemyInSight = cbTarget.hitEnemy;
    }

    float shootReward = 0.0f;
    TankActions tankActions0;
    // MultiDiscrete([3, 3, 2]) Action Space
    if (action0.size() == 3) {
      if (action0[0] == 1) tankActions0.forward = true;
      else if (action0[0] == 2) tankActions0.backward = true;

      if (action0[1] == 1) tankActions0.turnLeft = true;
      else if (action0[1] == 2) tankActions0.turnRight = true;

      if (action0[2] == 1 && trainingMode != 2) {
          // Chỉ tính reward/penalty khi bắn THỰC SỰ được (hết cooldown)
          bool canActuallyShoot = (myTank && myTank->shootCooldownTimer <= 0.0f);
          if (isEnemyInSight) {
              tankActions0.shoot = true;
              if (canActuallyShoot)
                  shootReward += (0.02f + std::max(0.0f, dotProd * 0.02f)) * shapingFactor; // Thưởng ngắm chuẩn
          } else {
              tankActions0.shoot = false; // Tịch thu lệnh bắn
              if (canActuallyShoot)
                  shootReward -= 0.03f * shapingFactor; // Phạt spam bắn mù
          }
      }
      
      lastAction0 = action0;
    }

    std::vector<TankActions> all_actions(game->numPlayers);
    all_actions[0] = tankActions0; // Người chơi 0 là AI

    // Người chơi 1 là đối thủ (nếu có action1 truyền tới)
    if (game->numPlayers > 1 && action1.size() == 3) {
      TankActions tankActions1;
      if (action1[0] == 1) tankActions1.forward = true;
      else if (action1[0] == 2) tankActions1.backward = true;

      if (action1[1] == 1) tankActions1.turnLeft = true;
      else if (action1[1] == 2) tankActions1.turnRight = true;

      if (action1[2] == 1) tankActions1.shoot = true;
      
      lastAction1 = action1;
      all_actions[1] = tankActions1;
    }



    // Cập nhật logic game với thời gian cố định 60 FPS (khoảng 0.016s mỗi bước)
    game->Update(all_actions, 1.0f / 60.0f);
    currentStep++;

    // --- LOGIC TÍNH PHẦN THƯỞNG (Reward) ---
    // Time Penalty nhân shapingFactor (sàn 0.1) để phase cuối không bị lấn át Kill/Death
    // Phase 1 (SF=1.0): -0.002 × 8000 = -16 | Phase 10 (SF=0.05→sàn 0.1): -0.0002 × 8000 = -1.6
    float reward = -0.002f * std::max(0.1f, shapingFactor);

    myTank = nullptr;
    enemyTank = nullptr;
    for (auto t : game->tanks) {
      if (t->playerIndex == 0)
        myTank = t;
      else if (!t->isDestroyed)
        enemyTank = t;
    }
    // 2. Kiểm tra trạng thái AI (index 0)
    bool p0Alive = (myTank != nullptr && !myTank->isDestroyed);

    // 0. Thưởng Tịnh Tiến (Approaching Reward)
    //    Đơn vị: LUÔN dùng Box2D units (1 grid cell = 90px / 30 SCALE = 3.0 units)
    float currentDistToTarget = 0.0f;
    if (game->mapEnabled && p0Alive && enemyTank && !enemyTank->isDestroyed) {
        int pathDist = 0;
        game->map.GetNextWaypoint(game->world, myTank->body->GetPosition(), enemyTank->body->GetPosition(), pathDist);
        currentDistToTarget = pathDist * 3.0f; // Chuyển grid cells → Box2D units (90px / SCALE)
    } else if (p0Alive && enemyTank && !enemyTank->isDestroyed) {
        currentDistToTarget = (myTank->body->GetPosition() - enemyTank->body->GetPosition()).Length();
    }

    if (p0Alive && enemyTank && !enemyTank->isDestroyed) {
        // Thưởng LIÊN TỤC khi khoảng cách giảm (so với step trước)
        if (lastDistanceToTarget > 0.1f && currentDistToTarget < lastDistanceToTarget) {
            reward += 0.005f * shapingFactor;
        }
        // Thưởng BONUS khi phá kỷ lục khoảng cách gần nhất
        if (currentDistToTarget < minDistanceReached) {
            if (minDistanceReached < 9000.0f) {
                reward += (minDistanceReached - currentDistToTarget) * 0.02f * shapingFactor;
            }
            minDistanceReached = currentDistToTarget;
        }
    }
    lastDistanceToTarget = currentDistToTarget;

    // 0b. Trừng phạt Cắm Trại (Camping Penalty): Phạt nếu đứng im TRONG KHI ĐỊCH XA
    // Không phạt nếu địch đang lại gần (phục kích hợp lý)
    if (myTank) {
        b2Vec2 currentPos = myTank->body->GetPosition();
        posHistory[historyIndex] = currentPos;
        historyIndex = (historyIndex + 1) % 60;
        if (historyCount < 60) historyCount++;

        if (historyCount == 60) {
            b2Vec2 oldPos = posHistory[historyIndex];
            float displacement = (currentPos - oldPos).Length() * SCALE;
            if (displacement < 30.0f) { // Giảm threshold: chỉ phạt khi THỰC SỰ đứng im (trước: 50px)
                float distToEnemy = getRawDistanceToEnemy(0);
                if (distToEnemy > 240.0f) {
                    // Enhanced: nếu ĐỊCH CŨNG đứng yên → phạt CỰC NẶNG
                    float enemySpd = 0.f;
                    if (enemyTank && !enemyTank->isDestroyed)
                        enemySpd = enemyTank->body->GetLinearVelocity().Length();
                    if (enemySpd < 0.15f)
                        reward -= 0.02f * shapingFactor;  // Phạt gãy cổ: cả 2 camping = bế tắc
                    else
                        reward -= 0.01f * shapingFactor;  // Phạt nặng: chỉ mình AI camping
                }
            }
        }
    }

    // Phạt đâm tường (tăng mạnh để AI sợ tường)
    if (myTank) {
      for (b2ContactEdge *edge = myTank->body->GetContactList(); edge;
           edge = edge->next) {
        if (edge->contact->IsTouching() &&
            edge->other->GetType() == b2_staticBody) {
          reward -= 0.005f * shapingFactor;

          // Stuck Penalty: Nếu nhấn tiến/lùi mà không di chuyển được (vận tốc
          // thấp) khi chạm tường
          float speed = myTank->body->GetLinearVelocity().Length();
          if (speed < 0.2f && action0.size() == 3 &&
              (action0[0] == 1 || action0[0] == 2)) {
            reward -= 0.01f * shapingFactor; // Phạt nặng để AI học cách lùi ra hoặc xoay đi
                             // hướng khác
          }
          break;
        }
      }
    }

    // Phạt thay đổi hướng liên tục (Jerky Movement Penalty)
    if (action0.size() == 3 && lastAction0.size() == 3) {
        if (action0[1] != lastAction0[1] && action0[1] != 0 && lastAction0[1] != 0) {
            reward -= 0.001f * shapingFactor; // Phạt giật trái phải liên tục
        }
        if (action0[0] != lastAction0[0] && action0[0] != 0 && lastAction0[0] != 0) {
            reward -= 0.001f * shapingFactor; // Phạt giật tiến lùi liên tục
        }
    }

    // Phạt / Thưởng bắn (đã tính trước khi Update để có Action Masking)
    reward += shootReward;

    // === BULLET PROXIMITY PENALTY ===
    // Phạt gradient khi đạn ĐỊCH bay gần Agent → dạy né đạn sớm
    if (p0Alive && myTank) {
        for (auto b : game->bullets) {
            if (!b || b->time <= 0 || b->ownerPlayerIndex == 0) continue;
            b2Vec2 bPos = b->body->GetPosition();
            b2Vec2 bVel = b->body->GetLinearVelocity();
            b2Vec2 toMe = myTank->body->GetPosition() - bPos;
            float dist = toMe.Length();
            if (dist > 8.0f || dist < 0.2f) continue;
            float bSpd = bVel.Length();
            if (bSpd < 0.5f) continue;
            b2Vec2 bDir(bVel.x / bSpd, bVel.y / bSpd);
            float dot = (bDir.x * toMe.x + bDir.y * toMe.y) / dist;
            if (dot < 0.3f) continue;
            float cross = bDir.x * toMe.y - bDir.y * toMe.x;
            float perpDist = fabsf(cross);
            if (perpDist < 2.5f) {
                float proximity = 1.0f - std::min(1.0f, perpDist / 2.5f);
                float closeness = 1.0f - std::min(1.0f, dist / 8.0f);
                reward -= 0.005f * proximity * closeness * shapingFactor;
            }
        }
    }

    // === RUSH REWARD ===
    // Thưởng áp sát khi kẻ địch ĐỨNG YÊN (đang ngắm sniper)
    if (p0Alive && enemyTank && !enemyTank->isDestroyed && action0.size() == 3 && action0[0] == 1) {
        float enemySpeed = enemyTank->body->GetLinearVelocity().Length();
        if (enemySpeed < 0.15f) {
            b2Vec2 myFwd(-sinf(myTank->body->GetAngle()), cosf(myTank->body->GetAngle()));
            b2Vec2 toEnemy = enemyTank->body->GetPosition() - myTank->body->GetPosition();
            float dist = toEnemy.Length();
            if (dist > 0.1f) {
                toEnemy.x /= dist; toEnemy.y /= dist;
                float facingEnemy = myFwd.x * toEnemy.x + myFwd.y * toEnemy.y;
                if (facingEnemy > 0.5f)
                    reward += 0.01f * shapingFactor;
            }
        }
    }

    // [ĐÃ XÓA] Active Movement Bonus — thừa vì:
    //   1. Bullet Proximity Penalty đã incentivize né đạn (gradient mượt)
    //   2. Reward này thưởng di chuyển BẤT KỲ hướng nào khi có đạn, kể cả chạy VÀO đạn
    //   3. Tạo noise: AI học rằng "cứ chạy là tốt" thay vì "né đúng hướng"

    // 1. Thưởng khi GIẾT (score tăng) = +5 (giảm từ +100 để cân bằng scale)
    int scoreDiff = game->playerScores[0] - lastScores[0];
    if (scoreDiff > 0) {
      reward += 5.0f * scoreDiff;
      lastScores[0] = game->playerScores[0];
    }

    if (!p0Alive && game->needsRestart) {
        bool suicided = false;
        for (const auto& death : game->recentDeaths) {
            if (death.playerIndex == 0 && death.killerIndex == 0) {
                suicided = true;
                break;
            }
        }
        if (suicided) {
            reward -= 10.0f; // Phạt tự sát nặng (giảm từ -250)
        } else {
            reward -= 5.0f; // Bị địch giết (giảm từ -100)
        }
    }

    // Phạt đâm/ôm sát kẻ địch (Ramming Penalty)
    if (p0Alive && enemyTank && !enemyTank->isDestroyed) {
      for (b2ContactEdge *edge = myTank->body->GetContactList(); edge; edge = edge->next) {
        if (edge->contact->IsTouching() && edge->other == enemyTank->body) {
          reward -= 0.005f * shapingFactor; // Phạt ôm địch
          break;
        }
      }
    }

    // 3. Shaping Reward: Vùng chiến đấu tối ưu
    //    CHỈ DÙNG khi KHÔNG có bản đồ (A* sẽ thay thế distance shaping)
    if (!game->mapEnabled) {
      float currentDist = getRawDistanceToEnemy(0);
      if (p0Alive && currentDist < 1000.0f) {
        if (currentDist > 350.0f) {
          reward -= 0.005f * shapingFactor;
        } else if (currentDist < 80.0f) {
          reward -= 0.015f * shapingFactor;
        }
      }

    }

    // 3b. A* Following Reward: Thưởng khi di chuyển theo hướng waypoint
    //     Chỉ dùng khi CÓ bản đồ — thay thế distance shaping
    if (game->mapEnabled && p0Alive && enemyTank && !enemyTank->isDestroyed) {
      int pathDist = 0;
      b2Vec2 waypoint =
          game->map.GetNextWaypoint(game->world, myTank->body->GetPosition(),
                                    enemyTank->body->GetPosition(), pathDist);
      b2Vec2 toWP = waypoint - myTank->body->GetPosition();
      float wpAbsAngle = atan2f(-toWP.x, toWP.y);
      float wpRelAngle = wpAbsAngle - myTank->body->GetAngle();
      float wpFacing = cosf(wpRelAngle);

      // Thưởng khi hướng về waypoint VÀ đang tiến tới
      if (wpFacing > 0.7f && action0.size() == 3 && action0[0] == 1) {
        reward += 0.008f * shapingFactor; // Thưởng đi theo waypoint A*
      }
    }

    // [ĐÃ XÓA] Facing Reward — thừa vì:
    //   1. Shoot reward (+0.02~0.04) đã thưởng khi ngắm đúng VÀ bắn → tín hiệu mạnh hơn
    //   2. +0.002 × SF quá nhỏ (nhỏ hơn time penalty), gần như không ảnh hưởng gradient
    //   3. Rush Reward đã thưởng facing + forward khi địch đứng yên



    // Kiểm tra điều kiện kết thúc
    bool isTimeout = (currentStep >= maxSteps);
    bool done = game->needsRestart || isTimeout || (!p0Alive);

    if (isTimeout && p0Alive) {
      if (trainingMode == 2) {
        reward += 5.0f; // CHẾ ĐỘ NÉ TRÁNH: Sống sót = THẮNG!
      } else {
        // Scale penalty theo khoảng cách: gần địch = đang cố gắng, xa địch = đang trốn
        // Giảm bớt để tránh AI học rằng "chết sớm tốt hơn timeout" (kết hợp time penalty tích lũy)
        float dist = getRawDistanceToEnemy(0);
        float penalty = -1.0f - 2.0f * std::min(1.0f, dist / 500.0f); // Range: -1.0 (gần) → -3.0 (xa)
        reward += penalty;
      }
    }

    auto state = getState(0); // Lấy trạng thái mới sau khi thực hiện hành động
    return py::make_tuple(state, reward, done, isTimeout);
  }

  /**
   * @brief Trích xuất các đặc trưng trạng thái (Observations) để AI ra quyết
   * định. Các giá trị thường được chuẩn hóa về khoảng [0, 1] hoặc [-1, 1] để
   * mạng neural hoạt động tốt.
   */
  // Lớp hỗ trợ tia laser đo khoảng cách cho Radar
  class RadarRayCastCallback : public b2RayCastCallback {
  public:
    float closestFraction = 1.0f;
    float ReportFixture(b2Fixture *fixture, const b2Vec2 &point,
                        const b2Vec2 &normal, float fraction) override {
      // Chỉ tính vật cản tĩnh (Tường)
      if (fixture->GetBody()->GetType() == b2_staticBody) {
        if (fraction < closestFraction) {
          closestFraction = fraction;
        }
        return fraction; // Trả về fraction giúp tối ưu kết quả tìm kiếm của
                         // Box2D
      }
      return -1.0f; // Bỏ qua vật thể khác (đạn, xe tăng khác)
    }
  };

  class EnemyForwardRayCastCallback : public b2RayCastCallback {
  public:
    b2Body *enemyBody;
    bool hitEnemy = false;

    EnemyForwardRayCastCallback(b2Body *enemy) : enemyBody(enemy) {}

    float ReportFixture(b2Fixture *fixture, const b2Vec2 &point,
                        const b2Vec2 &normal, float fraction) override {
      b2Body *hitBody = fixture->GetBody();
      if (hitBody->GetType() == b2_staticBody) {
        hitEnemy = false;
        return fraction; // Chạm tường -> cắt tia
      }
      if (hitBody == enemyBody) {
        hitEnemy = true;
        return fraction; // Chạm địch -> cắt tia
      }
      return -1.0f; // Bỏ qua vật thể khác (đạn, xe của chính mình, v.v.)
    }
  };

  std::vector<float> getState(int playerIdx) {
    std::vector<float> state;

    Tank *myTank = nullptr;
    Tank *enemyTank = nullptr;
    for (auto t : game->tanks) {
      if (t->playerIndex == playerIdx) {
        myTank = t;
      } else if (!t->isDestroyed) {
        enemyTank = t;
      }
    }

    if (myTank && !myTank->isDestroyed) {
      b2Vec2 myPos = myTank->body->GetPosition();
      float myAngle = myTank->body->GetAngle();
      
      b2Vec2 forwardDir(-sinf(myAngle), cosf(myAngle));
      b2Vec2 rightDir(cosf(myAngle), sinf(myAngle));
      
      float rayLength = std::sqrt(SCREEN_WIDTH * SCREEN_WIDTH + SCREEN_HEIGHT * SCREEN_HEIGHT) / SCALE;

      // Nhóm 1: Self State (5 Tham số)
      state.push_back(cosf(myAngle)); // [0] Heading Cos
      state.push_back(sinf(myAngle)); // [1] Heading Sin
      
      b2Vec2 myVel = myTank->body->GetLinearVelocity();
      state.push_back((myVel.x * rightDir.x + myVel.y * rightDir.y) / 3.0f);   // [2] Local Vx (chuẩn hóa = max speed 3.0)
      state.push_back((myVel.x * forwardDir.x + myVel.y * forwardDir.y) / 3.0f); // [3] Local Vy
      state.push_back(myTank->body->GetAngularVelocity() / 3.0f);              // [4] Angular Velocity (chuẩn hóa)

      // Nhóm 2: Enemy Info (8 Tham số)
      if (enemyTank) {
        b2Vec2 toEnemy = enemyTank->body->GetPosition() - myPos;
        // Project toEnemy onto Local axes
        float localX = toEnemy.x * rightDir.x + toEnemy.y * rightDir.y;
        float localY = toEnemy.x * forwardDir.x + toEnemy.y * forwardDir.y;
        state.push_back(localX / rayLength); // [5] Enemy Local X
        state.push_back(localY / rayLength); // [6] Enemy Local Y
        state.push_back(std::min(1.0f, toEnemy.Length() / rayLength)); // [7] Enemy Distance
        
        // Line of Sight
        EnemyForwardRayCastCallback cbTarget(enemyTank->body);
        game->world.RayCast(&cbTarget, myPos, enemyTank->body->GetPosition());
        state.push_back(cbTarget.hitEnemy ? 1.0f : 0.0f); // [8] Line of Sight
        
        b2Vec2 enemyVel = enemyTank->body->GetLinearVelocity();
        state.push_back((enemyVel.x * rightDir.x + enemyVel.y * rightDir.y) / 3.0f);   // [9] Enemy Local Vx
        state.push_back((enemyVel.x * forwardDir.x + enemyVel.y * forwardDir.y) / 3.0f); // [10] Enemy Local Vy
        
        float enemyAngle = enemyTank->body->GetAngle();
        state.push_back(cosf(enemyAngle - myAngle)); // [11] Enemy Heading Cos
        state.push_back(sinf(enemyAngle - myAngle)); // [12] Enemy Heading Sin

        // [MỚI] Tốc độ địch tiến về phía mình (approach speed)
        b2Vec2 toMe = myPos - enemyTank->body->GetPosition();
        float toMeDist = toMe.Length();
        float approachSpeed = 0.0f;
        if (toMeDist > 0.1f) {
            approachSpeed = (toMe.x * enemyVel.x + toMe.y * enemyVel.y) / toMeDist; // Dương = địch đang lại gần
        }
        state.push_back(std::max(-1.0f, std::min(1.0f, approachSpeed / 3.0f))); // [13] Enemy Approach Speed

        // [MỚI] Địch có thấy mình không? (Reverse Line of Sight)
        EnemyForwardRayCastCallback cbReverse(myTank->body);
        game->world.RayCast(&cbReverse, enemyTank->body->GetPosition(), myPos);
        state.push_back(cbReverse.hitEnemy ? 1.0f : 0.0f); // [14] Am I Visible to Enemy
      } else {
        for(int i=0; i<10; i++) state.push_back(0.0f); // [5-14]
      }

      // Nhóm 3: Bullet Radar (8 Tham số) - 2 viên đạn nguy hiểm nhất (bay về phía mình)
      struct BulletData {
        b2Vec2 pos;
        b2Vec2 vel;
        float ttc;
        float missDist;
        float priority; // TTC càng nhỏ thì ưu tiên càng cao
      };
      std::vector<BulletData> enemyBullets;
      for (auto b : game->bullets) {
        if (b->time > 0.0f) {
          b2Vec2 bPos = b->body->GetPosition();
          b2Vec2 bVel = b->body->GetLinearVelocity();
          b2Vec2 relPos = myPos - bPos;
          b2Vec2 relVel = bVel - myVel;
          
          float speedSqr = relVel.LengthSquared();
          float ttc = 1.0f; // Max
          float missDist = 1.0f;
          float priority = 999.0f;
          
          if (speedSqr > 0.001f) {
            float t = relPos.x * relVel.x + relPos.y * relVel.y;
            t /= speedSqr;
            if (t > 0) { // Bullet is moving towards tank
              ttc = std::min(1.0f, t / 5.0f); // Normalize max 5 seconds
              b2Vec2 closestPoint = bPos + t * bVel;
              b2Vec2 myExpectedPos = myPos + t * myVel;
              missDist = std::min(1.0f, (closestPoint - myExpectedPos).Length() / (100.0f/SCALE));
              priority = ttc;
            }
          }
          enemyBullets.push_back({bPos, bVel, ttc, missDist, priority});
        }
      }
      
      // Sort by priority (ascending)
      std::sort(enemyBullets.begin(), enemyBullets.end(), [](const BulletData& a, const BulletData& b) {
        return a.priority < b.priority;
      });
      
      for (int i = 0; i < 2; i++) {
        if (i < enemyBullets.size()) {
          b2Vec2 toBullet = enemyBullets[i].pos - myPos;
          float localX = toBullet.x * rightDir.x + toBullet.y * rightDir.y;
          float localY = toBullet.x * forwardDir.x + toBullet.y * forwardDir.y;
          state.push_back(localX / rayLength); // [15, 19] Bullet Local X
          state.push_back(localY / rayLength); // [16, 20] Bullet Local Y
          state.push_back(enemyBullets[i].ttc); // [17, 21] Time To Contact
          state.push_back(enemyBullets[i].missDist); // [18, 22] Miss Distance
        } else {
          state.push_back(0.0f);
          state.push_back(0.0f);
          state.push_back(1.0f); // Max TTC
          state.push_back(1.0f); // Max Miss Dist
        }
      }

      // Nhóm 4: Wall Radar (8 Tham số)
      float scanAngles[] = {-135.0f, -90.0f, -45.0f, 0.0f, 45.0f, 90.0f, 135.0f, 180.0f};
      for (int i = 0; i < 8; i++) {
        float rad = myAngle + scanAngles[i] * PI / 180.0f;
        b2Vec2 p2 = myPos + rayLength * b2Vec2(-sinf(rad), cosf(rad));
        RadarRayCastCallback cb;
        game->world.RayCast(&cb, myPos, p2);
        state.push_back(cb.closestFraction); // [23-30]
      }

      // Nhóm 5: A* Navigation (3 Tham số)
      if (game->mapEnabled && enemyTank) {
        int pathDist = 0;
        b2Vec2 waypoint = game->map.GetNextWaypoint(game->world, myPos, enemyTank->body->GetPosition(), pathDist);
        b2Vec2 toWP = waypoint - myPos;
        float localX = toWP.x * rightDir.x + toWP.y * rightDir.y;
        float localY = toWP.x * forwardDir.x + toWP.y * forwardDir.y;
        
        state.push_back(localX / rayLength); // [31] Waypoint Local X
        state.push_back(localY / rayLength); // [32] Waypoint Local Y
        state.push_back(std::min(1.0f, pathDist / 48.0f)); // [33] Waypoint Dist
      } else {
        state.push_back(0.0f); // [31]
        state.push_back(0.0f); // [32]
        state.push_back(1.0f); // [33]
      }

      // Nhóm 6: Status (5 Tham số)
      state.push_back(myTank->currentWeapon != ItemType::NORMAL ? std::min(1.0f, myTank->ammo / 5.0f) : 0.0f); // [34] My Ammo
      state.push_back(std::max(0.0f, 1.0f - myTank->shootCooldownTimer / 0.5f)); // [35] My Shoot Cooldown
      state.push_back(enemyTank && enemyTank->currentWeapon != ItemType::NORMAL ? std::min(1.0f, enemyTank->ammo / 5.0f) : 0.0f); // [36] Enemy Ammo
      state.push_back(myTank->hasShield ? 1.0f : 0.0f); // [37] Shield Active
      state.push_back(std::max(0.0f, 1.0f - myTank->shieldCooldownTimer / 15.0f)); // [38] Shield Cooldown

      // Nhóm 6b: Weapon Type One-Hot (5 Tham số: NORMAL, GATLING, FRAG, MISSILE, DEATH_RAY)
      state.push_back(myTank->currentWeapon == ItemType::NORMAL    ? 1.0f : 0.0f); // [39]
      state.push_back(myTank->currentWeapon == ItemType::GATLING   ? 1.0f : 0.0f); // [40]
      state.push_back(myTank->currentWeapon == ItemType::FRAG      ? 1.0f : 0.0f); // [41]
      state.push_back(myTank->currentWeapon == ItemType::MISSILE   ? 1.0f : 0.0f); // [42]
      state.push_back(myTank->currentWeapon == ItemType::DEATH_RAY ? 1.0f : 0.0f); // [43]

      // Nhóm 7: Previous Action One-Hot (8 Tham số)
      std::vector<int> lastAct = (playerIdx == 0) ? lastAction0 : lastAction1;
      int mMove = lastAct[0];
      int mTurn = lastAct[1];
      int mShoot = lastAct[2];
      
      // Move (0, 1, 2)
      state.push_back(mMove == 0 ? 1.0f : 0.0f); // [44]
      state.push_back(mMove == 1 ? 1.0f : 0.0f); // [45]
      state.push_back(mMove == 2 ? 1.0f : 0.0f); // [46]
      
      // Turn (0, 1, 2)
      state.push_back(mTurn == 0 ? 1.0f : 0.0f); // [47]
      state.push_back(mTurn == 1 ? 1.0f : 0.0f); // [48]
      state.push_back(mTurn == 2 ? 1.0f : 0.0f); // [49]
      
      // Shoot (0, 1)
      state.push_back(mShoot == 0 ? 1.0f : 0.0f); // [50]
      state.push_back(mShoot == 1 ? 1.0f : 0.0f); // [51]

    } else {
      // Tank dead -> fill 52 zeros
      state.insert(state.end(), 52, 0.0f);
    }

    return state;
  }

  std::vector<int> getBotAction(int level, int playerIdx) {
      if (playerIdx < 0 || playerIdx >= 4) return {0, 0, 0};

      // Tạo Bot persistent nếu chưa có, hoặc nếu level thay đổi (do RuleBasedBot.sample_level)
      if (!bots[playerIdx] || bots[playerIdx]->level != level) {
          delete bots[playerIdx];
          bots[playerIdx] = new Bot(level, playerIdx);
      }

      TankActions acts = bots[playerIdx]->GetAction(game);
      std::vector<int> result(3, 0);
      
      if (acts.forward) result[0] = 1;
      else if (acts.backward) result[0] = 2;
      
      if (acts.turnLeft) result[1] = 1;
      else if (acts.turnRight) result[1] = 2;
      
      if (acts.shoot) result[2] = 1;
      
      return result;
  }
};

PYBIND11_MODULE(azgame_env, m) {
  m.doc() = "Môi trường học tăng cường Pybind11 cho AZGame xe tăng";
  py::class_<RLEnv>(m, "RLEnv")
      .def(py::init<int, bool, bool, int, float>(), py::arg("num_players") = 2,
           py::arg("map_enabled") = true, py::arg("items_enabled") = true,
           py::arg("training_mode") = 0, py::arg("shaping_factor") = 1.0f)
      .def("reset", &RLEnv::reset)
      .def("seed", &RLEnv::seed, py::arg("seed"))
      .def("step", &RLEnv::step, py::arg("action0"),
           py::arg("action1") = std::vector<int>())
      .def("get_state", &RLEnv::getState, py::arg("playerIdx"))
      .def("render", &RLEnv::render)
      .def("get_bot_action", &RLEnv::getBotAction, py::arg("level"), py::arg("playerIdx"));
}
