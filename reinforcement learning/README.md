# 🤖 Thiết kế & Hướng dẫn Huấn luyện AI (Reinforcement Learning)

Thư mục này chứa toàn bộ mã nguồn, cấu hình và tài liệu thiết kế để huấn luyện AI cho **AZ Tank Game** sử dụng phương pháp **Học tăng cường (Reinforcement Learning - RL)**. Bằng cách bọc game engine viết bằng C++ qua thư viện **Pybind11**, Agent Python có thể tương tác trực tiếp với môi trường vật lý với hiệu suất cực cao (>1000 FPS ở chế độ headless).

---

## 📐 Kiến trúc & Thiết kế Hệ thống RL

### 1. Thuật toán cốt lõi & Thư viện sử dụng
- **Thuật toán**: **PPO (Proximal Policy Optimization)** được lấy từ thư viện **Stable-Baselines3 (SB3)**. Đây là một thuật toán thuộc nhóm Policy Gradient ổn định, hiệu quả và phổ biến bậc nhất hiện nay.
- **Mạng nơ-ron (Policy)**: Sử dụng **MLP (Multi-Layer Perceptron) Policy** với kiến trúc mạng nơ-ron truyền thẳng. MLP Policy nhỏ gọn giúp giảm thiểu overhead truyền dữ liệu giữa RAM và VRAM, giúp việc huấn luyện trên CPU đạt tốc độ cực nhanh.
- **Thư viện bọc (Wrapper)**: 
  - [rl_env_wrapper.cpp](file:///home/vansam/hust/TSV_nmAI/reinforcement%20learning/rl_env_wrapper.cpp): Bọc Game Engine C++ thành module Python `azgame_env` bằng Pybind11.
  - [gymnasium_wrapper.py](file:///home/vansam/hust/TSV_nmAI/reinforcement%20learning/gymnasium_wrapper.py): Kế thừa chuẩn `gymnasium.Env` để tương thích hoàn toàn với các thuật toán của Stable-Baselines3.

---

### 2. Không gian Quan sát (Observation Space - 52 chiều)
Trạng thái (State) trả về cho AI là một vector gồm **52 số thực** (`np.float32`), được chuẩn hóa về khoảng `[-1.0, 1.0]` hoặc `[0.0, 1.0]`:

| Tên nhóm đặc trưng | Số chiều | Chỉ số trong Vector | Chi tiết mô tả |
| :--- | :---: | :---: | :--- |
| **Self State** | 5 | `[0 -> 4]` | Hướng xoay xe tăng (Cos/Sin), Vận tốc tuyến tính cục bộ (Vx, Vy) chia cho max speed (3.0), và Vận tốc góc. |
| **Enemy Info** | 10 | `[5 -> 14]` | Vị trí tương đối của kẻ địch (Local X, Local Y), Khoảng cách chuẩn hóa, Trạng thái tầm nhìn (Line of Sight - 1.0 nếu nhìn thấy, 0.0 nếu bị tường cản), Vận tốc cục bộ của kẻ địch (Vx, Vy), Góc hướng tương đối (Cos/Sin), Tốc độ tiếp cận (Approach Speed), và trạng thái đối thủ có nhìn thấy mình không (Am I Visible). |
| **Bullet Radar** | 8 | `[15 -> 22]` | Thông tin về 2 viên đạn nguy hiểm nhất đang bay về phía xe tăng (ưu tiên theo thời gian va chạm TTC nhỏ nhất). Mỗi viên đạn gồm: Vị trí tương đối (Local X, Local Y), TTC (Time to Collision), Khoảng cách trượt gần nhất (Miss Distance). |
| **Wall Radar** | 8 | `[23 -> 30]` | Khoảng cách quét từ xe tăng đến các bức tường gần nhất ở 8 hướng xung quanh (`-135°`, `-90°`, `-45°`, `0°`, `45°`, `90°`, `135°`, `180°`). |
| **A\* Navigation** | 3 | `[31 -> 33]` | Tọa độ tương đối của Waypoint tiếp theo theo đường đi tối ưu A* (Local X, Local Y) và Khoảng cách đường đi (Path Distance). |
| **Status Info** | 5 | `[34 -> 38]` | Lượng đạn hiện tại, Thời gian hồi chiêu bắn, Lượng đạn của đối thủ, Trạng thái khiên (Active), Thời gian hồi chiêu khiên. |
| **Weapon Type** | 5 | `[39 -> 43]` | Mã hóa One-Hot của loại vũ khí hiện tại đang sử dụng (Normal, Gatling, Frag, Missile, Death Ray). |
| **Prev Actions** | 8 | `[44 -> 51]` | Mã hóa One-Hot của hành động ở bước trước đó của cả hai xe tăng (Di chuyển: 3 chiều, Xoay: 3 chiều, Bắn: 2 chiều) để AI học cách duy trì quán tính hành động. |

---

### 3. Không gian Hành động (Action Space)
Sử dụng không gian hành động rời rạc nhiều nhánh **`MultiDiscrete([3, 3, 2])`**, cho phép AI nhấn đồng thời nhiều phím:
- **Nhánh di chuyển (Move)**: `0` = Đứng yên (Idle), `1` = Tiến (Forward), `2` = Lùi (Backward).
- **Nhánh xoay (Turn)**: `0` = Đứng yên, `1` = Quay trái (Turn Left), `2` = Quay phải (Turn Right).
- **Nhánh bắn (Shoot)**: `0` = Không bắn, `1` = Bắn.

---

### 4. Thiết kế Phần thưởng (Reward Shaping)
Để định hướng AI học nhanh và đúng mục tiêu, hệ thống phần thưởng được thiết kế cực kỳ chi tiết:
- **Thời gian (Time Penalty)**: `-0.015` điểm mỗi bước để thúc ép AI kết thúc trận đấu nhanh, tránh hành vi đi vòng tròn hoặc đứng im.
- **Tiếp cận mục tiêu (Progress Reward)**: Thưởng/Phạt dựa trên sự thay đổi khoảng cách đường đi (theo A* hoặc đường thẳng) đến kẻ địch: `(Khoảng cách cũ - Khoảng cách mới) * 0.1`.
- **Đâm tường & Kẹt góc (Wall & Stuck Penalty)**:
  - Phạt đâm tường: `-0.02` điểm.
  - Phạt kẹt góc (Stuck Penalty): `-0.03` điểm nếu nhấn phím di chuyển sát tường nhưng vận tốc thực tế lại xấp xỉ bằng 0.
- **Ngắm bắn & Spam đạn (Shooting Reward/Penalty)**:
  - Thưởng bắn chuẩn: Từ `+0.05` đến `+0.1` điểm nếu nổ súng khi kẻ địch đang nằm trong tầm ngắm (Line of Sight).
  - Phạt spam đạn: `-0.1` điểm nếu nổ súng bừa bãi khi không thấy kẻ địch.
- **Phân định Thắng/Thua (Combat Reward)**:
  - Tiêu diệt kẻ địch: `+100.0` điểm.
  - Bị kẻ địch tiêu diệt: `-100.0` điểm.
  - Tự sát (bị đạn nảy của chính mình bắn trúng): `-250.0` điểm (Phạt cực nặng để AI học cách né đạn nảy).
  - Đâm/ôm kẻ địch (Ramming Penalty): `-0.02` điểm (tránh AI chọn giải pháp lao vào tự sát cùng địch).
- **Phần thưởng Điều hướng (A\* Navigation & Facing)**:
  - Thưởng di chuyển bám đường: `+0.005` điểm nếu hướng mặt và di chuyển đúng theo Waypoint của thuật toán A*.
  - Thưởng hướng mặt về địch (Facing Reward): `+0.004` điểm nếu luôn hướng nòng súng về phía đối thủ.
- **Vùng chiến đấu tối ưu (Combat Zone Shaping)** (Khi chơi trên bản đồ trống):
  - Phạt đứng quá xa (>350 pixel): `-0.02` điểm.
  - Phạt đứng quá gần (<80 pixel): `-0.05` điểm.

---

### 5. Chiến lược Huấn luyện Tăng tiến (Curriculum Learning) & Đấu tập (Self-Play)
Huấn luyện AI từ con số 0 trong môi trường phức tạp (mê cung, đạn nảy, nhặt đồ) rất dễ bị phân kỳ. Dự án giải quyết vấn đề này bằng hai kỹ thuật chính:

#### A. Lộ trình học 11 Giai đoạn (Curriculum Learning)
Độ khó của game và sức mạnh của đối thủ (Bot viết bằng luật mã hóa - Rule-based) được nâng dần qua từng giai đoạn:

1. **Phase 1: Nắm quyền kiểm soát (500k steps)**: Huấn luyện trên bản đồ trống, đối thủ là Bot Level 1 đứng im. AI chỉ học cách lái xe và ngắm bắn cơ bản.
2. **Phase 2: Rượt đuổi (500k steps)**: Bản đồ trống, Bot Level 2 biết di chuyển tự do. AI học cách Tracking (bám đuổi) mục tiêu di động.
3. **Phase 3: Né đạn cơ bản (800k steps)**: Bản đồ trống, Bot Level 3 bắn liên tục. AI bắt đầu học cách lách ngang (Strafe) để né đạn bay trực diện.
4. **Phase 4: Tác chiến cơ bản (1.0M steps)**: Bản đồ trống, đối thủ là Bot Level 4 (bắn thẳng chủ động).
5. **Phase 5: Nhập môn mê cung (1.5M steps)**: Bật bản đồ mê cung có tường chắn. Đối thủ là Bot Level 4. AI học cách di chuyển theo A* và né đâm tường.
6. **Phase 6: Tác chiến mê cung (2.0M steps)**: Có mê cung, đối thủ là Bot Level 5 (nảy 1 lần). AI học cách kết hợp giữa việc né tường và lướt lách tránh đạn nảy.
7. **Phase 7: Tác chiến nâng cao (3.0M steps)**: Có mê cung, đối thủ là Bot Level 6 (nảy 2 lần). AI học cách đối đầu với bot cấp độ cao hơn.
8. **Phase 8: Tối ưu hoá chiến thuật (3.0M steps)**: Có mê cung, đối thủ là Bot Level 6, nhưng giảm hệ số reward shaping (SF=0.3) để AI tự tối ưu hóa hành vi Kill/Death thực tế.
9. **Phase 9: Đấu toàn diện (3.0M steps)**: Có mê cung, bật tính năng nhặt vật phẩm hỗ trợ (Tăng đạn nảy, Giáp khiên bảo vệ), đối thủ trộn ngẫu nhiên Bot Level 5 và 6.
10. **Phase 10: Marathon trộn bot (10.0M steps)**: Bản đồ đầy đủ, bật items, đối thủ là Bot Level 4, 5, 6 trộn ngẫu nhiên để chống overfitting (học tủ).
11. **Phase 11: Boss Rush (5.0M steps)**: Bản đồ đầy đủ, bật items, đối thủ là Bot Level 7 (nảy 4 lần) với tốc độ tinh chỉnh cực cao.

#### B. Đấu tập nâng cao (Self-Play với Opponent Pool)
Để tránh tình trạng AI bị "học tủ" (overfitting) với một đối thủ cố định hoặc rơi vào vòng lặp kéo-búa-bao chiến thuật (Rock-Paper-Scissors cycle):
- Hệ thống duy trì một **Opponent Pool** chứa mô hình hiện tại, các checkpoint cũ đã lưu trong suốt quá trình train.
- Khi bắt đầu mỗi Episode mới, môi trường sẽ bốc ngẫu nhiên một đối thủ từ Pool hoặc Rule-based Bot để đối đầu với Agent. Điều này liên tục đa dạng hóa thử thách và buộc AI phải học các kỹ năng mang tính tổng quát hóa cao.

---

## 🛠️ Hướng dẫn Thực hành & Huấn luyện

### 1. Yêu cầu cài đặt (Prerequisites)
1. Cần chắc chắn bạn đã biên dịch thành công phần lõi C++ (file `.pyd` trên Windows hoặc `.so` trên Linux) qua thư mục `build` ở ngoài thư mục gốc, và file thư viện đã có thể nhận diện bởi Python.
2. Cài đặt các thư viện Python cần thiết:
   ```bash
   pip install stable-baselines3 gymnasium numpy torch tensorboard
   ```

---

### 2. Cách chạy huấn luyện tự động (Pipeline)
Mặc định huấn luyện Headless (không đồ họa) giúp tối đa hóa tốc độ FPS.
```bash
python train_ai.py --pipeline
```

---

### 3. Tiếp tục huấn luyện (Resume Training)
Dự án hiện tại hỗ trợ huấn luyện nối tiếp các phase (lưu tại thư mục `models/`). 

Để tiếp tục huấn luyện từ Phase X (ví dụ Phase 9) và tự động học tiếp lên các Phase tiếp theo, chạy lệnh sau:
```bash
python train_ai.py --pipeline --phase 9 --resume
```

**Giải thích lệnh:**
*   `--pipeline`: Chạy hệ thống chuyển cấp tự động sau khi hoàn thành số step mục tiêu.
*   `--phase 9`: Bắt đầu tiến trình từ Phase 9.
*   `--resume`: Tải lại file model `models/ppo_tank_phase9.zip` đã có sẵn để học tiếp thay vì tạo mới từ đầu.

Nếu bạn chỉ muốn train **duy nhất** một Phase cụ thể và dừng lại (ví dụ Phase 10):
```bash
python train_ai.py --phase 10
```

---

### 4. Theo dõi số liệu huấn luyện (TensorBoard)
Trong quá trình AI đang học, bạn có thể xem biểu đồ thời gian thực (điểm thưởng Reward, độ dài trận đấu, Loss) bằng TensorBoard.

1. Mở một cửa sổ Terminal mới.
2. Khởi động TensorBoard trỏ về thư mục `logs/curriculum/` (hoặc `logs/` tổng):
   ```bash
   tensorboard --logdir ./logs/ --host 127.0.0.1
   ```
   *(Lưu ý: Nếu bị lỗi command not found, hãy dùng `python -m tensorboard.main --logdir ./logs/ --host 127.0.0.1`)*
3. Mở trình duyệt web và truy cập: 👉 **http://127.0.0.1:6006**

---

### 5. Xem AI chiến đấu (Test Model)
Bất cứ lúc nào bạn cũng có thể mở giao diện đồ họa lên để xem AI hiện tại đang "múa" thế nào trước các Bot C++:
```bash
python train_ai.py --test 9
```
*(Thay số 9 bằng Phase bạn muốn kiểm tra. Lưu ý: Chế độ test không làm thay đổi trọng số của model).*

---

### 6. Huấn luyện bằng CPU hay GPU?
Mô hình trong dự án này được thiết kế để tối ưu hóa cực tốt khi chạy trên **CPU**. Bạn **KHÔNG CẦN** và không nên cấu hình train bằng GPU vì các lý do sau:
1. **Mạng Nơ-ron rất nhỏ**: PPO sử dụng mạng MlpPolicy với đầu vào chỉ là một vector 52 chiều (các góc bắn, khoảng cách). Tầng ẩn rất nhỏ nên CPU tính toán chỉ mất vài micro giây.
2. **Nghẽn cổ chai truyền dữ liệu**: Lõi vật lý Box2D chạy trên CPU. Nếu train bằng GPU, mỗi bước đi (step) của game sẽ bắt hệ thống phải copy dữ liệu qua lại giữa RAM (CPU) VRAM (GPU) liên tục. Việc truyền dữ liệu này tốn nhiều thời gian hơn cả việc tính toán, làm tốc độ train chậm đi đáng kể.
3. **Song song hóa**: Môi trường đã được cấu hình chạy song song 4 trận đấu (`n_envs=4`), sử dụng đa luồng C++ ngầm rất hiệu quả trên CPU.

---

### 7. Cấu trúc thư mục Models
- `models/`: Nơi lưu các model chính thức (ví dụ: `ppo_tank_phase9.zip`). Đây là những file nhỏ, gọn và đã được theo dõi (track) trên GitHub.
- `models/checkpoints/`: Nơi tự động lưu trữ các bản nháp sau mỗi 50.000 bước. Rất hữu ích để cứu nguy nếu máy bạn bị sập nguồn giữa chừng. Thư mục này thường rất nặng nên đã được đưa vào `.gitignore` để tránh đầy dung lượng kho lưu trữ.
