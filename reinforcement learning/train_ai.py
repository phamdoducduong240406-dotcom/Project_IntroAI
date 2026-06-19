"""
Script huấn luyện AI xe tăng (Curriculum Learning 10 Giai đoạn)
Hỗ trợ Self-Play (Lấy AI cũ làm đối thủ cho AI mới tập né).
"""

import os
import argparse
import sys

# Đảm bảo in tiếng Việt ra terminal Windows không bị lỗi
if sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout.reconfigure(encoding='utf-8')

from stable_baselines3 import PPO
from stable_baselines3.common.env_util import make_vec_env
from stable_baselines3.common.callbacks import CheckpointCallback, BaseCallback
from gymnasium_wrapper import AZTankEnv

class ProgressCallback(BaseCallback):
    def __init__(self, print_freq=10_000, verbose=0):
        super().__init__(verbose)
        self.print_freq = print_freq

    def _on_step(self) -> bool:
        if self.n_calls % self.print_freq == 0:
            print(f"  [Bước {self.num_timesteps:>10,}] "
                  f"Cập nhật quá trình học...")
        return True

# LỘ TRÌNH 10 GIAI ĐOẠN HUẤN LUYỆN (ANTI-BOUNCE SNIPER v3 — Curriculum Optimized)
# Level 1: Đứng yên | Level 2: Chỉ chạy | Level 3: Bắn thụ động | Level 4: Bắn thẳng chủ động
# Level 5: Nảy 1 lần | Level 6: Nảy 2 lần
#
# Hyperparameters per-phase:
#   lr          = Learning Rate — tốc độ cập nhật trọng số mạng neural.
#                 Giảm dần qua các phase để tránh "quên kiến thức cũ" (catastrophic forgetting).
#                 Phase đầu (3e-4): bước nhảy lớn, học nhanh từ zero.
#                 Phase cuối (5e-5): bước nhảy nhỏ, tinh chỉnh (fine-tune) kỹ năng đã có.
#
#   ent_coef    = Entropy Coefficient — hệ số khám phá ngẫu nhiên.
#                 Giá trị cao (0.05) = AI thử nhiều hành động khác nhau → tốt cho giai đoạn đầu.
#                 Giá trị thấp (0.005) = AI ổn định chính sách, ít ngẫu nhiên → tốt cho giai đoạn cuối.
#                 Nếu giảm quá đột ngột → AI "đóng băng" chính sách sớm, không thích ứng được.
#
#   shaping_factor (SF) = Hệ số khuếch đại Reward Shaping (các phần thưởng dẫn dắt nhỏ).
#                 SF=1.0: Reward shaping mạnh → dạy kỹ năng cơ bản (tìm đường, né đạn...).
#                 SF=0.05: Reward shaping gần như tắt → AI chỉ tối ưu Kill/Death thực sự.
#
#   bot_level   = Danh sách level Bot đối thủ. Khi có nhiều level (vd [4,5,6]),
#                 mỗi ván sẽ ngẫu nhiên chọn 1 level → AI phải giỏi đánh MỌI loại đối thủ,
#                 tránh overfit (chỉ biết đánh 1 kiểu bot cụ thể).
PHASES = {
    # CHƯƠNG 1: NỀN TẢNG (Bãi trống, khám phá nhiều, LR cao)
    1:  {"map": False, "items": False, "mode": 0, "bot_level": [1], "steps": 500_000,   "shaping_factor": 1.0,  "lr": 3e-4, "ent_coef": 0.05},   # L1: Bia tập bắn (tăng từ 300K → 500K để hội tụ kỹ năng cơ bản)
    2:  {"map": False, "items": False, "mode": 0, "bot_level": [2], "steps": 500_000,   "shaping_factor": 1.0,  "lr": 3e-4, "ent_coef": 0.05},   # L2: Đuổi mục tiêu di động
    3:  {"map": False, "items": False, "mode": 0, "bot_level": [3], "steps": 800_000,   "shaping_factor": 1.0,  "lr": 3e-4, "ent_coef": 0.05},   # L3: Né bắn thụ động
    4:  {"map": False, "items": False, "mode": 0, "bot_level": [4], "steps": 1_000_000, "shaping_factor": 1.0,  "lr": 3e-4, "ent_coef": 0.05},   # L4: Combat thẳng chủ động

    # CHƯƠNG 2: MÊ CUNG + BOUNCE (LR giảm dần, ent_coef chuyển tiếp mượt)
    5:  {"map": True,  "items": False, "mode": 0, "bot_level": [4],    "steps": 1_500_000, "shaping_factor": 0.9,  "lr": 2e-4, "ent_coef": 0.03},   # L4 + mê cung (SF giảm nhẹ vì đã biết cơ bản)
    6:  {"map": True,  "items": False, "mode": 0, "bot_level": [5],    "steps": 2_000_000, "shaping_factor": 0.7,  "lr": 2e-4, "ent_coef": 0.03},   # L5: Nảy 1 lần
    7:  {"map": True,  "items": False, "mode": 0, "bot_level": [6],    "steps": 3_000_000, "shaping_factor": 0.5,  "lr": 1e-4, "ent_coef": 0.01},   # L6: Nảy 2 lần
    8:  {"map": True,  "items": False, "mode": 0, "bot_level": [6],    "steps": 3_000_000, "shaping_factor": 0.3,  "lr": 1e-4, "ent_coef": 0.01},   # L6: Củng cố với SF thấp hơn

    # CHƯƠNG 3: NÂNG CAO (LR thấp = fine-tune, bot trộn để chống overfit)
    9:  {"map": True,  "items": True,  "mode": 0, "bot_level": [5, 6],    "steps": 3_000_000,  "shaping_factor": 0.15, "lr": 5e-5, "ent_coef": 0.01},   # Full combat + items, trộn 2 loại bot
    10: {"map": True,  "items": True,  "mode": 0, "bot_level": [4, 5, 6], "steps": 10_000_000, "shaping_factor": 0.05, "lr": 5e-5, "ent_coef": 0.005},  # Marathon: trộn 3 loại bot chống overfit

    # CHƯƠNG 4: BOSS RUSH (Chỉ đánh Bot mạnh nhất — tinh chỉnh tối đa)
    11: {"map": True,  "items": True,  "mode": 0, "bot_level": [7],       "steps": 5_000_000,  "shaping_factor": 0.03, "lr": 3e-5, "ent_coef": 0.003},  # Boss Rush: 1v1 Bot L7 bounce 4 lần, LR cực thấp
}

import random
import glob

class OpponentPool:
    """
    Opponent Pool cho Self-Play nâng cao.
    Thay vì chỉ đánh với 1 model cố định (frozen), pool lưu nhiều phiên bản cũ
    và chọn ngẫu nhiên đối thủ → tránh bẫy Rock-Paper-Scissors cycling.
    """
    def __init__(self, phase_config, current_phase=None):
        self.models = []
        self.phase_config = phase_config
        self.current_phase = current_phase
        self.refresh()

    def refresh(self):
        """Quét ổ đĩa để tải các model đối thủ mới nhất"""
        op_phase = self.phase_config.get("op_phase")
        if op_phase is None:
            return

        # Logic tải đối thủ:
        # Phase 9 (Asymmetric): Đấu với Phase 8 (frozen)
        # Phase 10 (Symmetric): Đấu với chính mình (Phase 10 checkpoints)
        phases_to_load = [op_phase]

        new_models = []
        for p in phases_to_load:
            # 1. Thêm model phase chính thức
            op_path = f"models/ppo_tank_phase{p}.zip"
            if os.path.exists(op_path):
                try:
                    new_models.append(PPO.load(op_path))
                    print(f"  [Pool] Đã nạp model chính Phase {p}")
                except Exception: pass

            # 2. Quét thêm các checkpoint (chỉ lấy 3 cái mới nhất mỗi phase để tránh tràn RAM)
            checkpoint_pattern = f"models/checkpoints/ppo_phase{p}_*.zip"
            checkpoints = sorted(glob.glob(checkpoint_pattern))
            for cp in checkpoints[-3:]:
                try:
                    new_models.append(PPO.load(cp))
                    print(f"  [Pool] Đã nạp checkpoint: {os.path.basename(cp)}")
                except Exception: pass
        
        if new_models:
            self.models = new_models
            print(f"  [Pool] Hiện có tổng cộng {len(self.models)} đối thủ.")
        elif not self.models:
            print(f"  [Cảnh báo] Pool trống rỗng! Đối thủ sẽ đứng im.")

    def sample(self):
        """Chọn ngẫu nhiên 1 đối thủ từ pool"""
        if not self.models:
            return None
        return random.choice(self.models)

    def __len__(self):
        return len(self.models)

from bot import RuleBasedBot

def make_env(phase_id, opponent_pool=None, render_mode=None):
    cfg = PHASES[phase_id]
    
    # Ưu tiên dùng Hardcoded Bot nếu cấu hình yêu cầu
    if cfg.get("bot_level") is not None:
        opponent_model = RuleBasedBot(level=cfg["bot_level"])
    else:
        # Nếu không có bot_level, dùng Self-Play từ Opponent Pool
        opponent_model = opponent_pool.sample() if opponent_pool else None

    return AZTankEnv(
        num_players=2, # Cần 2 vì còn có địch để bắn
        map_enabled=cfg["map"],
        items_enabled=cfg["items"],
        training_mode=cfg["mode"],
        opponent_model=opponent_model,
        opponent_pool=opponent_pool,  # Truyền pool để swap đối thủ mỗi episode
        render_mode=render_mode,
        shaping_factor=cfg.get("shaping_factor", 1.0)
    )

def train_phase(phase_id, resume_model_path=None, render=False):
    cfg = PHASES[phase_id]
    os.makedirs("models", exist_ok=True)
    os.makedirs("logs", exist_ok=True)
    
    model_path = f"models/ppo_tank_phase{phase_id}"
    
    print("=" * 60)
    print(f"  BẮT ĐẦU HUẤN LUYỆN - GIAI ĐOẠN {phase_id}")
    print(f"  Map: {cfg['map']} | Items: {cfg['items']} | Mode: {cfg['mode']} | Target Steps: {cfg['steps']:,}")
    print("=" * 60)

    # 1. Tải Pool Đối thủ (nếu có) — Self-Play nâng cao
    opponent_pool = OpponentPool(cfg, current_phase=phase_id)

    # 2. Khởi tạo môi trường
    # Nếu đang bật xem trực tiếp (render) thì phải ép n_envs=1 để khỏi bay nhiều cửa sổ
    num_envs = 1 if render else 4
    render_mode = "human" if render else None
    env = make_vec_env(lambda: make_env(phase_id, opponent_pool, render_mode), n_envs=num_envs)

    # 3. Callback đặc biệt cho Self-Play: Cập nhật pool mỗi khi có checkpoint mới
    class SelfPlayCallback(BaseCallback):
        def __init__(self, pool, refresh_freq=100_000):
            super().__init__()
            self.pool = pool
            self.refresh_freq = refresh_freq
        def _on_step(self) -> bool:
            if self.n_calls % self.refresh_freq == 0:
                print("\n  [Self-Play] Đang làm mới Pool đối thủ từ các checkpoint mới nhất...")
                self.pool.refresh()
            return True
    
    self_play_cb = SelfPlayCallback(opponent_pool)

    # 3. PPO Hyperparameters — đọc từ cấu hình per-phase
    #    Phase 1-4: Khám phá nhiều (ent_coef cao, LR cao, batch lớn)
    #    Phase 5-10: Khai thác kiến thức (ent_coef/LR giảm dần, batch nhỏ hơn)
    is_early_phase = (phase_id <= 4)
    ppo_params = {
        "n_steps": 4096 if is_early_phase else 2048,
        "batch_size": 128 if is_early_phase else 64,
        "ent_coef": cfg.get("ent_coef", 0.05 if is_early_phase else 0.01),
        "learning_rate": cfg.get("lr", 3e-4),
    }
    print(f"  [PPO] n_steps={ppo_params['n_steps']} | batch={ppo_params['batch_size']} | ent={ppo_params['ent_coef']} | lr={ppo_params['learning_rate']}")

    # 4. Khởi tạo Model AI (Tiếp tục từ phase trước, hoặc resume file)
    training_device = "cpu" 

    # Tính toán target_timesteps để khi resume không bị train lố
    base_timesteps = 0
    if phase_id > 1 and os.path.exists(f"models/ppo_tank_phase{phase_id - 1}.zip"):
        try:
            # Lấy num_timesteps của phase trước
            temp_model = PPO.load(f"models/ppo_tank_phase{phase_id - 1}.zip", custom_objects={"env": None}, device="cpu")
            base_timesteps = temp_model.num_timesteps
            del temp_model
        except Exception:
            base_timesteps = sum(PHASES[p]["steps"] for p in range(1, phase_id))
    
    target_timesteps = base_timesteps + cfg["steps"]
    
    if resume_model_path and os.path.exists(resume_model_path + ".zip"):
        print(f"  [Info] Kế thừa trí tuệ từ model: {resume_model_path}.zip")
        model = PPO.load(resume_model_path, env=env, device=training_device,
                         custom_objects={
                             "n_steps": ppo_params["n_steps"],
                             "batch_size": ppo_params["batch_size"],
                             "ent_coef": ppo_params["ent_coef"],
                             "learning_rate": ppo_params["learning_rate"],
                         })
    elif phase_id > 1 and os.path.exists(f"models/ppo_tank_phase{phase_id - 1}.zip"):
        prev_path = f"models/ppo_tank_phase{phase_id - 1}"
        print(f"  [Info] Kế thừa trí tuệ từ Phase {phase_id - 1}")
        model = PPO.load(prev_path, env=env, device=training_device,
                         custom_objects={
                             "n_steps": ppo_params["n_steps"],
                             "batch_size": ppo_params["batch_size"],
                             "ent_coef": ppo_params["ent_coef"],
                             "learning_rate": ppo_params["learning_rate"],
                         })
    else:
        print("  [Info] Khởi tạo Model hoàn toàn mới!")
        model = PPO(
            policy="MlpPolicy",
            env=env,
            learning_rate=ppo_params["learning_rate"],
            n_steps=ppo_params["n_steps"],
            batch_size=ppo_params["batch_size"],
            n_epochs=10,
            gamma=0.99,
            ent_coef=ppo_params["ent_coef"],
            verbose=1,
            device=training_device,
            tensorboard_log="./logs/curriculum/"
        )

    # Nếu model đã train đủ hoặc lố số steps mục tiêu, tự động chuyển phase
    if model.num_timesteps >= target_timesteps:
        print(f"\n  [Hoàn Thành] Giai đoạn {phase_id} ĐÃ HOÀN THÀNH TỪ TRƯỚC (Current steps: {model.num_timesteps:,} >= Target: {target_timesteps:,})")
        if resume_model_path:
            model.save(model_path)
        env.close()
        del model
        del opponent_pool
        del env
        import gc
        gc.collect()
        import torch
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
        return

    remaining_steps = target_timesteps - model.num_timesteps
    print(f"  [Info] Target Phase {phase_id}: {target_timesteps:,} steps. Hiện tại: {model.num_timesteps:,} steps -> Cần train thêm: {remaining_steps:,} steps")

    # Callback
    checkpoint_cb = CheckpointCallback(save_freq=50_000, save_path="./models/checkpoints/", name_prefix=f"ppo_phase{phase_id}")
    progress_cb = ProgressCallback(print_freq=20_000)

    if render:
        print("\n  [Chú ý] Chế độ biểu diễn ĐANG BẬT. Tốc độ train sẽ bị dìm xuống mức thấp nhất (bằng tốc độ mắt nhìn)...")

    try:
        model.learn(total_timesteps=remaining_steps, callback=[checkpoint_cb, progress_cb, self_play_cb], reset_num_timesteps=False)
        model.save(model_path)
        print(f"\n  [Hoàn Thành] Giai đoạn {phase_id} lưu tại: {model_path}.zip\n")
        
        env.close()
        del model
        del opponent_pool
        del env
        import gc
        gc.collect()
        import torch
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
            
    except KeyboardInterrupt:
        model.save(model_path)
        print(f"\n  Người dùng dừng sớm, vẫn lưu model tại: {model_path}.zip\n")
        sys.exit(0)

def pipeline(start_phase=1, resume_start=False, render=False):
    print(f"\n🚀 Bắt đầu Curriculum Learning từ Phase {start_phase} 🚀\n")
    for phase_id in sorted(PHASES.keys()):
        if phase_id < start_phase:
            continue
            
        expected_path = f"models/ppo_tank_phase{phase_id}.zip"
        
        # Nếu là phase đầu tiên và người dùng muốn resume, ta luôn nạp model cũ
        if phase_id == start_phase and resume_start:
             print(f"  🔄 Đang nạp lại model Phase {phase_id} để học tiếp...")
             train_phase(phase_id, resume_model_path=f"models/ppo_tank_phase{phase_id}", render=render)
        # Nếu đã có model phase này rồi thì bỏ qua để tiết kiệm thời gian
        elif os.path.exists(expected_path):
             print(f"  ⏩ Đã có model Phase {phase_id}, tự bỏ qua...")
             continue
        else:
             train_phase(phase_id, render=render)
    print("\n✅ TẤT CẢ CÁC GIAI ĐOẠN ĐÃ HOÀN THÀNH!")

def test_model(phase_id):
    """Mở cửa sổ đồ họa xem thư giãn model đã train (không học)"""
    model_path = f"models/ppo_tank_phase{phase_id}.zip"
    if not os.path.exists(model_path):
        print(f"  [Lỗi] Không tìm thấy model tại {model_path}!")
        return
        
    print(f"\n🎮 Đang TEST Model Phase {phase_id}...")
    cfg = PHASES[phase_id]
    opponent_pool = OpponentPool(cfg)
    env = make_env(phase_id, opponent_pool, render_mode="human")
    model = PPO.load(model_path)
    
    obs, _ = env.reset()
    try:
        while True:
            action, _ = model.predict(obs, deterministic=True)
            obs, reward, done, trunc, info = env.step(action)
            if not info.get("window_open", True):
                print("\n  [INFO] Đã đóng cửa sổ game.")
                break

            if done or trunc:
                obs, _ = env.reset()
    except KeyboardInterrupt:
        print("\n  [INFO] Dừng test model.")
    finally:
        env.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--pipeline", action="store_true", help="Chạy tự động từ GĐ 1 đến 10")
    parser.add_argument("--phase", type=int, choices=range(1, 12), help="Chỉ định chạy 1 GĐ cụ thể")
    parser.add_argument("--render", action="store_true", help="Mở cửa sổ Raylib xem (TRAIN RẤT CHẬM)")
    parser.add_argument("--test", type=int, choices=range(1, 12), help="Xem AI múa ở Phase X (sau khi train)")
    parser.add_argument("--resume", action="store_true", help="Tiếp tục học từ file save đang dở")
    args = parser.parse_args()

    if args.test:
        test_model(args.test)
    elif args.pipeline:
        # Nếu có truyền --phase X khi đang chạy --pipeline, nó sẽ bắt đầu từ phase X
        start_ph = args.phase if args.phase else 1
        pipeline(start_phase=start_ph, resume_start=args.resume, render=args.render)
    elif args.phase:
        resume_path = f"models/ppo_tank_phase{args.phase}" if args.resume else None
        train_phase(args.phase, resume_model_path=resume_path, render=args.render)
    else:
        print("Vui lòng chọn cách chạy:")
        print("  python train_ai.py --pipeline             (Chạy từ đầu đến cuối)")
        print("  python train_ai.py --pipeline --phase 9   (Bắt đầu từ Phase 9, bỏ qua nếu đã có file)")
        print("  python train_ai.py --pipeline --phase 9 --resume  (Học TIẾP Phase 9 rồi tự động sang các Phase sau)")
        print("  python train_ai.py --phase 1 --resume     (Chỉ học tiếp Phase 1 rồi dừng)")
        print("  python train_ai.py --test 7               (Xem AI diễn hài SAU KHI train xong)")
