import numpy as np

class MockSpace:
    def __init__(self, shape):
        self.shape = shape

class RuleBasedBot:
    def __init__(self, level=1):
        self.level_list = level if isinstance(level, list) else [level]
        self.level = self.level_list[0]
        self.action_space_shape = (3,)
        self.observation_space = MockSpace((45,))

    def sample_level(self):
        import random
        self.level = random.choice(self.level_list)
        return self.level

    def get_action_from_env(self, env_instance):
        """
        Gọi trực tiếp backend C++ để lấy hành động cực nhanh và chính xác.
        env_instance là đối tượng RLEnv từ azgame_env.
        Người chơi AI luôn là playerIndex 1 (Player 2).
        """
        action = env_instance.get_bot_action(self.level, 1)
        return action

    def predict(self, obs, deterministic=True):
        # Fallback in case it's used somewhere else without env_instance
        action = np.array([0, 0, 0], dtype=np.int32)
        is_batch = len(obs.shape) > 1
        if is_batch: return np.array([action]), None
        return action, None
