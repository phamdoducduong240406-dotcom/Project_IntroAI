#pragma once
#include "game.h"
#include <vector>
#include <string>

/**
 * @class AIBot
 * @brief Bot AI dùng Neural Network (PPO đã train) để ra quyết định.
 * 
 * Thay thế Bot C++ rule-based bằng AI đã học qua Reinforcement Learning.
 * Load trọng số từ file binary (.bin), chạy forward pass thuần C++ — 
 * KHÔNG cần Python, KHÔNG cần ONNX Runtime, KHÔNG cần thêm DLL.
 * 
 * Kiến trúc mạng neural (MlpPolicy PPO):
 *   Input (52 floats) → Linear(52,64) → Tanh → Linear(64,64) → Tanh → Linear(64,8) → logits
 *   logits[0:3] → Move (idle/forward/backward)
 *   logits[3:6] → Turn (idle/left/right)  
 *   logits[6:8] → Shoot (no/yes)
 */

/// Một layer fully-connected: y = W*x + b
struct DenseLayer {
    int rows, cols;                 ///< rows = output dim, cols = input dim
    std::vector<float> weights;     ///< Row-major: weights[r * cols + c]
    std::vector<float> biases;      ///< biases[r]
};

class AIBot {
public:
    int playerIndex;

    /// @param playerIndex Index của người chơi (0-3) mà AI điều khiển
    /// @param modelPath Đường dẫn file trọng số (.bin) export từ Python
    AIBot(int playerIndex, const char* modelPath = "models/ai_model.bin");
    ~AIBot() = default;

    /// Ra quyết định hành động cho 1 frame, dựa trên trạng thái game hiện tại
    TankActions GetAction(Game* game);

    /// Kiểm tra model đã load thành công chưa
    bool IsLoaded() const { return loaded; }

private:
    bool loaded = false;
    std::vector<DenseLayer> layers;

    /// Hành động frame trước (dùng cho Previous Action One-Hot observation)
    int lastMove = 0;   // 0=idle, 1=forward, 2=backward
    int lastTurn = 0;   // 0=idle, 1=left, 2=right
    int lastShoot = 0;  // 0=no, 1=yes

    /// Load trọng số từ file binary
    bool LoadWeights(const char* path);

    /// Thu thập 52 observations từ trạng thái game (port 1:1 từ rl_env_wrapper.cpp::getState)
    std::vector<float> CollectObservations(Game* game);

    /// Chạy forward pass neural network → ra 8 logits
    std::vector<float> Forward(const std::vector<float>& input);

    /// Chuyển 8 logits thành 3 actions (argmax mỗi nhóm)
    void LogitsToActions(const std::vector<float>& logits, int& move, int& turn, int& shoot);
};
