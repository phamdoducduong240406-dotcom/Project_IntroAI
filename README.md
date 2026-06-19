Đây là dự án môn **Nhập môn Trí tuệ Nhân tạo**.

## 👥 Đội ngũ phát triển (Team Members)

Dưới đây là danh sách 3 thành viên của nhóm:

| STT | Họ và Tên | Mã sinh viên |
| :---: | :--- | :---: |
| 1 | **Nguyễn Quý Trung** | `202416756` |
| 2 | **Phạm Văn Sâm** | `202400114` |
| 3 | **Nguyễn Công Vinh** | `202400083` |



> 💡 **Thông điệp của nhóm:** *"Chúng mình hy vọng qua dự án này không chỉ nắm vững các khái niệm cơ bản của Trí tuệ Nhân tạo, mà còn ứng dụng thành công các thuật toán vào việc giải quyết bài toán thực tế. Cảm ơn thầy cô và các bạn đã dành thời gian theo dõi dự án của nhóm!"*
# 🛡️ AZ Tank Game - Advanced AI Edition

AZ Tank là một dự án game bắn xe tăng 2D hiệu suất cao, được tối ưu hóa cho cả **người chơi (Local Multiplayer)** và **huấn luyện trí tuệ nhân tạo (Reinforcement Learning)**. 

Dự án sử dụng bộ đôi sức mạnh: **Raylib** cho đồ họa/UI mượt mà và **Box2D** cho mô phỏng vật lý chính xác đến từng pixel.

---

## 🚀 Tính năng nổi bật

### 🎮 Gameplay & Physics
- **Local Multiplayer**: Hỗ trợ 1–4 người chơi đấu với nhau hoặc đấu với Bot trên cùng một máy.
- **Vật lý Box2D**: Đạn nảy dội tường, va chạm thực tế, hiệu ứng đẩy xe khi bắn.
- **Vũ khí đa dạng**: Gatling Gun, Frag Mine (đạn nổ chùm), Homing Missile (tên lửa đuổi), và Death Ray (tia laser).
- **Cổng dịch chuyển (Portal)**: Hệ thống dịch chuyển tức thời ngẫu nhiên trên bản đồ.
- **Mê cung ngẫu nhiên**: Sinh bản đồ tự động bằng thuật toán *Recursive Backtracker* với các đường tắt (shortcuts) thông minh.

### 🤖 Hệ thống Bot AI Siêu cấp (NEW!)
Hệ thống Bot AI được xây dựng theo 5 cấp độ khó, sử dụng các thuật toán tiên tiến nhất:
- **A* Pathfinding (Grid-based)**: Tìm đường đi ngắn nhất qua mê cung phức tạp.
- **Fat Raycast (Multi-beam)**: Sử dụng hệ thống 5 tia laser để quét chướng ngại vật, đảm bảo xe tăng không bao giờ bị kẹt góc hoặc va chạm khi rẽ.
- **String Pulling (Path Smoothing)**: Làm mượt đường đi dích dắc của A* thành các đường thẳng tắp, dứt khoát.
- **Path Commitment**: Cơ chế ghi nhớ đường đi thông minh, chỉ tính toán lại A* khi mục tiêu thay đổi ô Caro, giúp tối ưu CPU và loại bỏ hiện tượng AI bị "co giật".
- **Predictive Aiming (Xạ thủ)**: Bot cấp độ 5 có khả năng tính toán vận tốc địch để bắn đón đầu (Aiming at future position).

---

## 🛠️ Kiến trúc Mã nguồn

Game được thiết kế theo mô hình **Logic - Renderer Decoupling**, cho phép logic game chạy độc lập hoàn toàn với đồ họa (thích hợp cho huấn luyện AI Headless).

### Core Logic (Header-only compatible with RL)
| Thành phần | Vai trò |
|---|---|
| `bot.h/.cpp` | **Bộ não AI**: A*, String Pulling, Predictive Aiming, Dodging logic. |
| `map.h/.cpp` | Quản lý lưới (Grid), sinh mê cung và thuật toán tìm đường A*. |
| `game.h/.cpp` | Engine chính điều phối toàn bộ thế giới vật lý và luật chơi. |
| `tank.h/.cpp` | Logic vật lý xe tăng, quản lý HP, Khiên và Vũ khí. |
| `bullet.h/.cpp` | Mô phỏng các loại đạn và hiệu ứng đặc biệt. |

### Graphics & UI
| Thành phần | Vai trò |
|---|---|
| `renderer.h/.cpp` | Hệ thống vẽ 2D cao cấp, bao gồm cả **Debug Grid** để quan sát tư duy của AI. |
| `ui.h/.cpp` | Giao diện cài đặt (Settings) và bảng điểm (HUD). |
| `main.cpp` | Điểm khởi đầu của ứng dụng, kết nối Input với Game Engine. |

---

## ⚙️ Hướng dẫn Cài đặt & Biên dịch

### Yêu cầu hệ thống
* **CMake**: Phiên bản `≥ 3.15`
* **Trình biên dịch**: Hỗ trợ chuẩn C++17 (GCC/MinGW, Clang, hoặc MSVC).
* **Thư viện**: Chỉ cần cài đặt **Raylib** trên hệ thống (Box2D và pybind11 sẽ được CMake tự động tải về từ GitHub khi cấu hình dự án):
  * **Trên Linux (Ubuntu/Debian):**
    ```bash
    sudo apt install libraylib-dev
    ```
  * **Trên Windows (MSYS2 / MinGW-w64):**
    ```bash
    pacman -S mingw-w64-x86_64-raylib
    ```
    *(Hoặc bạn có thể tải bản precompiled từ [Raylib Releases](https://github.com/raysan5/raylib/releases) và khai báo đường dẫn).*

### Biên dịch trên Linux (Ubuntu/Debian)
```bash
# 1. Cài đặt Raylib
sudo apt install libraylib-dev

# 2. Tạo thư mục build
mkdir build && cd build

# 3. Cấu hình và biên dịch (Release mode để game chạy mượt nhất)
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# 4. Chạy game chính
./AZgame

# 5. Chạy A* Bridge
./AZgameBridge
```

### Biên dịch trên Windows (MSYS2 / MinGW-w64)
```bash
# Tạo thư mục build
mkdir build && cd build

# Cấu hình và biên dịch
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
mingw32-make -j4

# Chạy game
./AZgame.exe
```

---

## 🕹️ Điều khiển (Mặc định)

| Hành động | Player 1 | Player 2 | Player 3 | Player 4 |
|---|:---:|:---:|:---:|:---:|
| **Di chuyển** | `W/A/S/D` | `↑/←/↓/→` | `I/J/K/L` | `Numpad 8/4/5/6` |
| **Bắn** | `Q` | `/` | `U` | `Numpad 7` |
| **Khiên** | `E` | `.` | `O` | `Numpad 9` |

> 💡 *Bạn có thể thay đổi phím điều khiển bất cứ lúc nào trong menu Settings (biểu tượng bánh răng ⚙️).*

---

## 📈 Hệ thống Học tăng cường (Reinforcement Learning - RL)

Dự án này tích hợp một môi trường huấn luyện xe tăng tự động chất lượng cao dựa trên phương pháp **Học tăng cường**. Bằng cách bọc game engine viết bằng C++ qua thư viện **Pybind11**, Agent Python có thể tương tác trực tiếp với môi trường vật lý với hiệu suất cực cao (>1000 FPS ở chế độ headless).

Chi tiết thiết kế thuật toán cốt lõi, không gian quan sát (52 chiều), không gian hành động, thiết kế phần thưởng, chiến lược huấn luyện tăng tiến (Curriculum Learning) và hướng dẫn chạy huấn luyện vui lòng tham khảo tại:
👉 **[Hướng dẫn & Thiết kế Huấn luyện AI (Reinforcement Learning)](file:///home/vansam/hust/TSV_nmAI/reinforcement%20learning/README.md)**
