# 🛡️ Đồ án Giới thiệu Trí tuệ Nhân tạo (IT3160) - Nhóm 26

<p align="center">
  <b>ĐẠI HỌC BÁCH KHOA HÀ NỘI</b><br>
  <b>TRƯỜNG CÔNG NGHỆ THÔNG TIN VÀ TRUYỀN THÔNG</b><br>
</p>

## 👥 Thành viên nhóm & Phân công nhiệm vụ

* **Giảng viên hướng dẫn:** PGS.TS Lê Thanh Hương
* **Đề tài:** Huấn luyện AI xe tăng đối kháng bằng Reinforcement Learning (PPO) ứng dụng trong game AZ Tank 2D

| STT | Họ và Tên | Mã sinh viên | Vai trò & Phân công nhiệm vụ | Đóng góp |
| :---: | :--- | :---: | :--- | :---: |
| 1 | **Cao Thanh Hùng** | `202416502` | - Cài đặt môi trường AZ Tank Game bằng Box2D.<br>- Xây dựng hệ thống Bot đối thủ đa cấp độ (Rule-based).<br>- Viết báo cáo phần kiến trúc hệ thống và thuật toán. | `50%` |
| 2 | **Phạm Đỗ Đức Dương** | `202416466` | - Tích hợp môi trường Gymnasium và thiết kế hàm phần thưởng.<br>- Cài đặt và huấn luyện mô hình PPO (Curriculum Learning).<br>- Chạy thực nghiệm, đánh giá kết quả và thiết kế slide. | `50%` |

> 💡 **Thông điệp của nhóm:** *"Chúng mình hy vọng qua dự án này không chỉ nắm vững các khái niệm cơ bản của Trí tuệ Nhân tạo, mà còn ứng dụng thành công các thuật toán vào việc giải quyết bài toán thực tế. Cảm ơn cô và các bạn đã dành thời gian theo dõi dự án của nhóm!"*

---

# 🛡️ AZ Tank Game - Advanced AI Edition

AZ Tank là một dự án game bắn xe tăng 2D hiệu suất cao, được tối ưu hóa cho cả **người chơi (Local Multiplayer)** và **huấn luyện trí tuệ nhân tạo (Reinforcement Learning)**. Dự án sử dụng bộ đôi sức mạnh: **Raylib** cho đồ họa/UI mượt mà và **Box2D** cho mô phỏng vật lý chính xác đến từng pixel.

---

## 🚀 Tính năng nổi bật của dự án

### 🎮 Gameplay & Physics
- **Local Multiplayer**: Hỗ trợ 1–4 người chơi đấu với nhau hoặc đấu với Bot trên cùng một máy.
- **Vật lý Box2D**: Đạn nảy dội tường, va chạm thực tế, hiệu ứng đẩy xe khi bắn.
- **Vũ khí đa dạng**: Gatling Gun, Frag Mine (đạn nổ chùm), Homing Missile (tên lửa đuổi), và Death Ray (tia laser).
- **Cổng dịch chuyển (Portal)**: Hệ thống dịch chuyển tức thời ngẫu nhiên trên bản đồ.
- **Mê cung ngẫu nhiên**: Sinh bản đồ tự động bằng thuật toán *Recursive Backtracker* với các đường tắt (shortcuts) thông minh.

### 🤖 Hệ thống Bot AI Đa dạng & Tiên tiến (NEW!)
Dự án tích hợp 3 loại bot riêng biệt, đại diện cho các phương pháp phát triển AI khác nhau:

1. **Bot Heuristic Đa Luồng (Multi-threaded Rule-based Bot)**:
   - **Đa luồng tối ưu**: Sử dụng **3 luồng phần cứng** (Main Thread thu thập dữ liệu, Movement Thread lái xe, Shooting Thread ngắm bắn) giúp tối ưu CPU.
   - **Định hướng thông minh**: Di chuyển bằng thuật toán **Pure Pursuit** bám sát đường đi ngắn nhất của **A\*** (đã tối ưu hóa **Tiebreaker** và **Nở rộng vật cản**), kết hợp tự động căn giữa lối đi (**Center-Seeking**) và né tránh chướng ngại vật (**Whisker Repulsion**).
   - **Cơ chế chiến đấu**: Tìm đường bắn nảy tường tối ưu qua cảm biến quét quạt tia laser 360 độ (72 tia, nảy từ 1-4 lần tùy cấp độ Level 1-7). Có hệ thống né đạn sườn (**Dodge System**) phản xạ vuông góc với quỹ đạo đạn địch.
   
2. **Bot Demo A* (A* Pathfinding Agent)**:
   - **Logic tối giản**: Chạy đơn luồng (Single-thread), phục vụ cho mục đích debug và làm ví dụ demo trực quan cho thuật toán A*.
   - **Hành vi**: Chỉ tập trung tìm đường ngắn nhất qua A* để tiếp cận địch, ngắm bắn thẳng khi có tầm nhìn hoặc bắn nảy đơn giản 1 lần qua tường (1-Bounce). Không tích hợp hệ thống né đạn nâng cao.

3. **Bot AI Học Tăng Cường (PPO Deep RL Agent)**:
   - **Học sâu (Deep RL)**: Sử dụng mạng nơ-ron truyền thẳng (MLP Policy) với kiến trúc đơn giản để ra quyết định dựa trên thuật toán **PPO (Proximal Policy Optimization)**.
   - **Suy luận siêu tốc thuần C++ (Zero-Dependency)**: Mô hình được nạp từ file binary `models/ai_model.bin` (~32 KB, ~8k tham số). Forward Pass được tính toán thủ công bằng toán học cơ bản, suy luận tức thời trong **1-3 microseconds** trên CPU, không cần PyTorch hay ONNX Runtime.
   - **Lối chơi giống con người (Human-like)**: Sở hữu các hành vi thông minh tự phát triển như di chuyển lướt né đạn (Sidestep), chủ động nhặt hòm vật phẩm, núp góc phục kích khi ít máu, và áp sát tấn công khi sở hữu vũ khí mạnh.

---

## 🛠️ Kiến trúc Mã nguồn

Game được thiết kế theo mô hình **Logic - Renderer Decoupling**, cho phép logic game chạy độc lập hoàn toàn với đồ họa (thích hợp cho huấn luyện AI Headless).

### Core Logic & Bots
| Thành phần | Vai trò |
|---|---|
| [bot.h/.cpp](file:///c:/Users/Admin/Desktop/PRJ_AI_N26/reinforcement%20learning/bot.h) | **Bot Heuristic Đa Luồng**: Điều hướng Pure Pursuit, né tránh đạn và quét 360 độ bắn nảy 4 tường. |
| [astar_bot.h/.cpp](file:///c:/Users/Admin/Desktop/PRJ_AI_N26/Astar/astar_bot.h) | **Bot Demo A\***: Logic tìm đường A* và bắn nảy 1 tường tối giản. |
| [ai_bot.h/.cpp](file:///c:/Users/Admin/Desktop/PRJ_AI_N26/reinforcement%20learning/ai_bot.h) | **Bot AI PPO**: Forward Pass mạng nơ-ron thuần C++ và thu thập 52 đặc trưng trạng thái (Observations). |
| [map.h/.cpp](file:///c:/Users/Admin/Desktop/PRJ_AI_N26/map.cpp) | Sinh mê cung Recursive Backtracker và thuật toán cốt lõi A* (String Pulling, Obstacle Inflation). |
| [game.h/.cpp](file:///c:/Users/Admin/Desktop/PRJ_AI_N26/game.cpp) | Engine chính điều phối thế giới vật lý Box2D và quản lý vòng lặp logic game. |
| [tank.h/.cpp](file:///c:/Users/Admin/Desktop/PRJ_AI_N26/tank.cpp) | Logic vật lý xe tăng, máu, khiên bảo vệ và vũ khí. |

### Graphics & UI
| Thành phần | Vai trò |
|---|---|
| [renderer.h/.cpp](file:///c:/Users/Admin/Desktop/PRJ_AI_N26/renderer.cpp) | Hệ thống vẽ đồ họa 2D (Raylib) và vẽ các đường dẫn debug. |
| [ui.h/.cpp](file:///c:/Users/Admin/Desktop/PRJ_AI_N26/ui.cpp) | Menu cấu hình trận đấu (Settings) và HUD điểm số. |
| [main.cpp](file:///c:/Users/Admin/Desktop/PRJ_AI_N26/main.cpp) | Entry point chính của game, kết nối bàn phím/chuột và chạy game loop. |

---

## ⚙️ Hướng dẫn Cài đặt & Biên dịch

### 📦 Các thư viện & gói phần mềm sử dụng
#### C++ / Đồ họa:
* **CMake** (`≥ 3.15`)
* **Trình biên dịch**: Chuẩn C++17 (MSVC, GCC/MinGW, hoặc Clang).
* **Raylib**: Thư viện đồ họa và UI.
* **Box2D** (`v2.4.1`): Được CMake tự động tải về từ GitHub.
* **pybind11** (`v2.12.0`): Được CMake tự động tải về từ GitHub.

#### Python (Cho huấn luyện/chạy AI):
* **Python** (`≥ 3.8`)
* **stable-baselines3**: Thuật toán RL (PPO).
* **gymnasium**: Môi trường giả lập.
* **numpy**, **torch** (PyTorch), **tensorboard**.

### 🛠️ Cài đặt thư viện phụ thuộc
* **Cài đặt Raylib (C++):**
  * **Trên Windows (MSYS2 / MinGW-w64):**
    ```bash
    pacman -S mingw-w64-x86_64-raylib
    ```
  * **Trên Linux (Ubuntu/Debian):**
    ```bash
    sudo apt install libraylib-dev
    ```
* **Cài đặt thư viện Python:**
  ```bash
  pip install stable-baselines3 gymnasium numpy torch tensorboard
  ```

### 🔨 Biên dịch phần C++
*Để chạy được phần đồ họa hoặc huấn luyện AI bằng Python, trước hết bạn cần biên dịch lõi C++.*

#### Biên dịch trên Windows (MSYS2 / MinGW-w64)
```bash
# 1. Tạo thư mục build
mkdir build && cd build

# 2. Cấu hình và biên dịch
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
mingw32-make -j4

# 3. Chạy game đồ họa
./AZgame.exe
```

#### Biên dịch trên Linux (Ubuntu/Debian)
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# Chạy game đồ họa
./AZgame
```

---

## 🕹️ Điều khiển (Mặc định)

| Hành động | Player 1 | Player 2 | Player 3 | Player 4 |
|---|:---:|:---:|:---:|:---:|
| **Di chuyển** | `W/A/S/D` | `↑/←/↓/→` | `I/J/K/L` | `Numpad 8/4/5/6` |
| **Bắn** | `Q` | `/` | `U` | `Numpad 7` |
| **Khiên** | `E` | `.` | `O` | `Numpad 9` |

> 💡 *Bạn có thể cấu hình lại phím điều khiển bất cứ lúc nào trong menu Settings (biểu tượng bánh răng ⚙️).*

---

## 📈 Hệ thống Học tăng cường (Reinforcement Learning - RL)

Dự án này tích hợp một môi trường huấn luyện xe tăng tự động chất lượng cao dựa trên phương pháp **Học tăng cường**. Bằng cách bọc game engine viết bằng C++ qua thư viện **Pybind11**, Agent Python có thể tương tác trực tiếp với môi trường vật lý với hiệu suất cực cao (>1000 FPS ở chế độ headless).

*Lưu ý: Đảm bảo bạn đã hoàn thành việc biên dịch lõi C++ ở trên. Việc biên dịch thành công sẽ sinh ra module thư viện liên kết động (`azgame_env.pyd` trên Windows hoặc `azgame_env.so` trên Linux) trong thư mục `build`, hỗ trợ Python import trực tiếp.*

### 🏃 Hướng dẫn chạy và huấn luyện AI

1. **Huấn luyện tự động theo lộ trình (Curriculum Pipeline):**
   ```bash
   python train_ai.py --pipeline
   ```
2. **Tiếp tục huấn luyện từ một Phase cụ thể (Resume):**
   ```bash
   python train_ai.py --pipeline --phase 9 --resume
   ```
3. **Chạy thử nghiệm xem AI thi đấu (Có đồ họa):**
   ```bash
   python train_ai.py --test 9
   ```
4. **Theo dõi quá trình huấn luyện bằng TensorBoard:**
   ```bash
   tensorboard --logdir ./logs/ --host 127.0.0.1
   ```
   Sau đó, mở trình duyệt và truy cập: [http://127.0.0.1:6006](http://127.0.0.1:6006)

---

Chi tiết thiết kế thuật toán cốt lõi, không gian quan sát (52 chiều), không gian hành động, thiết kế phần thưởng, chiến lược huấn luyện tăng tiến (Curriculum Learning) và hướng dẫn chạy huấn luyện vui lòng tham khảo tại:
👉 **[Hướng dẫn & Thiết kế Huấn luyện AI (Reinforcement Learning)](file:///c:/Users/Admin/Desktop/PRJ_AI_N26/reinforcement%20learning/README.md)**
