import sys
import os
import numpy as np
import gymnasium as gym
from gymnasium import spaces

# -------------------------------------------------------
# Thêm thư mục build vào đường dẫn để Python tìm thấy file .pyd
# Lưu ý: file này trong /reinforcement learning/ nên phải lùi ra 1 cấp ('..') để thấy /build/
build_dir = os.path.join(os.path.dirname(__file__), '..', 'build')
if os.path.exists(os.path.join(build_dir, 'Debug')):
    sys.path.append(os.path.join(build_dir, 'Debug'))
elif os.path.exists(os.path.join(build_dir, 'Release')):
    sys.path.append(os.path.join(build_dir, 'Release'))
else:
    sys.path.append(build_dir)

# Thêm thư mục MSYS2 chứa các DLL (libstdc++, libgcc) vào đường dẫn để có thể load file .pyd
msys2_bin = "C:/msys64/ucrt64/bin"
if hasattr(os, "add_dll_directory") and os.path.exists(msys2_bin):
    os.add_dll_directory(msys2_bin)

import azgame_env


class AZTankEnv(gym.Env):
    """
    Bọc (wrap) RLEnv của C++ theo chuẩn Gymnasium để
    tương thích với Stable Baselines3.

    State (52 floats):
        [0 -> 4]: Self State (Heading Cos/Sin, Local Vx/Vy, Angular Vel)
        [5 -> 14]: Enemy Info (Local X/Y, Distance, LOS, Local Vx/Vy, Heading Cos/Sin, Approach Speed, Am I Visible)
        [15 -> 22]: Bullet Radar (2 most dangerous bullets: Local X/Y, TTC, Miss Dist)
        [23 -> 30]: Wall Radar (8 directions scan)
        [31 -> 33]: A* Navigation (Waypoint Local X/Y, Path Distance)
        [34 -> 38]: Status (Ammo, Shoot Cooldown, Enemy Ammo, Shield Active, Shield Cooldown)
        [39 -> 43]: Weapon Type One-Hot (Normal, Gatling, Frag, Missile, Death Ray)
        [44 -> 51]: Previous Action One-Hot (Move 3, Turn 3, Shoot 2)

    Action (MultiBinary - 5 phím có thể nhấn cùng lúc):
        [0]: Tiến (1=có, 0=không)
        [1]: Lùi (1=có, 0=không)
        [2]: Quay trái (1=có, 0=không)
        [3]: Quay phải (1=có, 0=không)
        [4]: Bắn (1=có, 0=không)
    """

    metadata = {"render_modes": ["human"]}

    def __init__(self, num_players=2, map_enabled=False, items_enabled=False, training_mode=0, opponent_model=None, opponent_pool=None, render_mode=None, shaping_factor=1.0):
        super().__init__()

        self.opponent_model = opponent_model
        self.opponent_pool = opponent_pool  # Pool đối thủ để swap mỗi episode
        self.render_mode = render_mode

        # Khởi tạo môi trường game C++
        self._env = azgame_env.RLEnv(
            num_players=num_players,
            map_enabled=map_enabled,
            items_enabled=items_enabled,
            training_mode=training_mode,
            shaping_factor=shaping_factor
        )

        # Không gian hành động: MultiDiscrete(3, 3, 2)
        # 0: Move (0=idle, 1=fwd, 2=back)
        # 1: Turn (0=idle, 1=left, 2=right)
        # 2: Shoot (0=idle, 1=shoot)
        self.action_space = spaces.MultiDiscrete([3, 3, 2])

        # Định nghĩa không gian quan sát: 52 con số thực
        self.observation_space = spaces.Box(
            low=-1.0,
            high=1.0,
            shape=(52,),
            dtype=np.float32
        )

    def reset(self, seed=None, options=None):
        """Bắt đầu ván chơi mới, trả về trạng thái ban đầu."""
        super().reset(seed=seed)
        if seed is not None:
            self._env.seed(seed)
            
        # Swap đối thủ mỗi ván mới → tăng diversity
        if self.opponent_pool is not None:
            new_opponent = self.opponent_pool.sample()
            if new_opponent is not None:
                self.opponent_model = new_opponent
        
        # Nếu là RuleBasedBot, chọn ngẫu nhiên Level từ danh sách được cấp
        if self.opponent_model is not None and hasattr(self.opponent_model, 'sample_level'):
            self.opponent_model.sample_level()

        state = self._env.reset()
        expected_shape = self.observation_space.shape[0]
        obs = np.array(state[:expected_shape], dtype=np.float32)
        info = {}
        return obs, info

    def step(self, action):
        """
        Thực hiện 1 hành động, trả về kết quả theo chuẩn Gymnasium.
        Returns: (observation, reward, terminated, truncated, info)
        """
        action_p1 = []
        if self.opponent_model is not None:
             # Thu thập State của Enemy (Player 1)
             state_p1 = self._env.get_state(1)

             # FIX LỖI 1: Cắt độ dài mảng State sao cho vừa đúng kích thước model cũ cần (vd 20 hoặc 21)
             expected_shape = self.opponent_model.observation_space.shape[0]
             obs_p1 = np.array(state_p1[:expected_shape], dtype=np.float32)

             # Cho model PPO cũ dự đoán đòn đánh
             if hasattr(self.opponent_model, 'get_action_from_env'):
                 action_p1 = self.opponent_model.get_action_from_env(self._env)
             else:
                 opp_action, _ = self.opponent_model.predict(obs_p1, deterministic=True)
                 
                 # FIX LỖI 2: Xử lý output của model đối thủ
                 if isinstance(opp_action, np.ndarray):
                     action_p1 = opp_action.flatten().tolist()
                 elif isinstance(opp_action, list):
                     action_p1 = opp_action
                 else:
                     action_p1 = [0, 0, 0]
                     
                 # Tịch thu lệnh bắn nếu không thấy địch (mô phỏng lại logic lúc train)
                 if len(action_p1) == 3 and action_p1[2] == 1 and state_p1[8] < 0.5:
                     action_p1[2] = 0

        # Đảm bảo chuyển Numpy Array của SB3 sang Python List dạng INT trước khi đẩy cho C++
        action_list = [int(a) for a in action]
        
        # Tương tự cho đối thủ
        action_p1_list = [int(a) for a in action_p1] if len(action_p1) > 0 else []

        state, reward, done, is_timeout = self._env.step(action_list, action_p1_list)
        expected_shape = self.observation_space.shape[0]
        obs = np.array(state[:expected_shape], dtype=np.float32)

        info = {}
        if self.render_mode == "human":
             is_open = self._env.render()
             info["window_open"] = is_open

        # Gymnasium phân biệt 2 loại kết thúc:
        # terminated = kết thúc tự nhiên (bị chết, bắn hạ hết địch)
        # truncated  = hết thời gian (maxSteps)
        terminated = done and not is_timeout
        truncated = is_timeout

        return obs, float(reward), terminated, truncated, info

    def render(self):
        if self.render_mode == "human":
             return self._env.render()
        return True

    def close(self):
        pass
