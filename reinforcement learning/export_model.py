"""
Export trọng số PPO Model sang file binary để game C++ có thể inference.

Thay vì dùng ONNX Runtime (50MB DLL, phức tạp), ta export trọng số neural network
sang file binary nhỏ (~32KB) và implement forward pass thuần C++.

File binary format:
  - "AZAI" (4 bytes magic)
  - version (uint32)
  - num_layers (uint32)
  - For each layer:
      - rows (uint32), cols (uint32)
      - weight data (rows × cols × float32)
      - bias data (rows × float32)

Usage:
  python export_model.py --phase 11
  python export_model.py --phase 11 --output ../models/ai_model.bin
"""
import os
import sys
import struct
import numpy as np

# Đảm bảo in tiếng Việt ra terminal Windows không bị lỗi
if sys.stdout and hasattr(sys.stdout, 'encoding') and sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout.reconfigure(encoding='utf-8')

from stable_baselines3 import PPO


def export_weights(model_path, output_path):
    """Export policy network weights to binary format for C++ game."""
    print(f"Đang tải model từ {model_path}.zip ...")
    model = PPO.load(model_path)
    
    policy = model.policy
    
    # ================================================================
    # Trích xuất trọng số từ SB3 MlpPolicy
    # Kiến trúc:
    #   mlp_extractor.policy_net: Sequential(Linear, Tanh, Linear, Tanh)
    #   action_net: Linear(64, 8)  # cho MultiDiscrete [3, 3, 2]
    # ================================================================
    layers = []
    
    # Hidden layers (policy net)
    print("\n  [Kiến trúc Policy Network]")
    for i, layer in enumerate(policy.mlp_extractor.policy_net):
        if hasattr(layer, 'weight'):
            w = layer.weight.detach().cpu().numpy()
            b = layer.bias.detach().cpu().numpy()
            layers.append((w, b))
            print(f"    Hidden layer {len(layers)}: weight {w.shape}, bias {b.shape}, activation=Tanh")
    
    # Output layer (action net) — không có activation
    w = policy.action_net.weight.detach().cpu().numpy()
    b = policy.action_net.bias.detach().cpu().numpy()
    layers.append((w, b))
    print(f"    Output layer:   weight {w.shape}, bias {b.shape}, activation=None")
    
    # ================================================================
    # Ghi file binary
    # ================================================================
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)
    
    with open(output_path, 'wb') as f:
        f.write(b'AZAI')                             # Magic number
        f.write(struct.pack('I', 1))                  # Version
        f.write(struct.pack('I', len(layers)))        # Số lượng layer
        
        for w, b in layers:
            rows, cols = w.shape
            f.write(struct.pack('II', rows, cols))
            f.write(w.astype(np.float32).tobytes())   # Weight matrix (row-major)
            f.write(b.astype(np.float32).tobytes())   # Bias vector
    
    file_size = os.path.getsize(output_path)
    total_params = sum(w.size + b.size for w, b in layers)
    print(f"\n  ✅ Đã export {total_params:,} tham số ({file_size:,} bytes) → {output_path}")
    
    # Verify file
    _verify_file(output_path, layers)
    
    return layers


def _verify_file(path, original_layers):
    """Đọc lại file binary và so sánh với weights gốc."""
    with open(path, 'rb') as f:
        magic = f.read(4)
        assert magic == b'AZAI', f"Magic sai: {magic}"
        version = struct.unpack('I', f.read(4))[0]
        assert version == 1
        num_layers = struct.unpack('I', f.read(4))[0]
        assert num_layers == len(original_layers), f"Số layer sai: {num_layers} vs {len(original_layers)}"
        
        for i, (orig_w, orig_b) in enumerate(original_layers):
            rows, cols = struct.unpack('II', f.read(8))
            assert (rows, cols) == orig_w.shape, f"Layer {i} shape sai"
            w = np.frombuffer(f.read(rows * cols * 4), dtype=np.float32).reshape(rows, cols)
            b = np.frombuffer(f.read(rows * 4), dtype=np.float32)
            assert np.allclose(w, orig_w, atol=1e-7), f"Layer {i} weight sai!"
            assert np.allclose(b, orig_b, atol=1e-7), f"Layer {i} bias sai!"
    
    print("  ✅ Verification PASSED — file binary chính xác!")


def test_inference(model_path, output_path):
    """So sánh kết quả inference Python vs manual forward pass (giống C++ sẽ làm)."""
    import torch
    
    model = PPO.load(model_path)
    
    # Tạo observation test ngẫu nhiên
    np.random.seed(42)
    test_obs = np.random.randn(52).astype(np.float32)
    
    # ---- Inference bằng SB3 (Python) ----
    obs_tensor = torch.FloatTensor(test_obs).unsqueeze(0)
    with torch.no_grad():
        features = model.policy.extract_features(obs_tensor, model.policy.features_extractor)
        latent_pi, _ = model.policy.mlp_extractor(features)
        logits = model.policy.action_net(latent_pi)
    logits_python = logits.numpy().flatten()
    
    # ---- Manual forward pass (giống C++) ----
    with open(output_path, 'rb') as f:
        f.read(4 + 4)  # magic + version
        num_layers = struct.unpack('I', f.read(4))[0]
        layers = []
        for _ in range(num_layers):
            rows, cols = struct.unpack('II', f.read(8))
            w = np.frombuffer(f.read(rows * cols * 4), dtype=np.float32).reshape(rows, cols)
            b = np.frombuffer(f.read(rows * 4), dtype=np.float32)
            layers.append((w, b))
    
    x = test_obs.copy()
    for i, (w, b) in enumerate(layers):
        x = w @ x + b
        if i < len(layers) - 1:  # Tanh cho hidden layers, KHÔNG cho output layer
            x = np.tanh(x)
    logits_manual = x
    
    # So sánh
    print(f"\n  [Test Inference]")
    print(f"    Python logits:  {logits_python}")
    print(f"    Manual logits:  {logits_manual}")
    max_diff = np.max(np.abs(logits_python - logits_manual))
    print(f"    Max difference: {max_diff:.2e}")
    
    if np.allclose(logits_python, logits_manual, atol=1e-5):
        print("  ✅ Inference test PASSED — C++ sẽ cho kết quả giống hệt!")
    else:
        print("  ❌ Inference test FAILED — Cần kiểm tra lại!")
        return False
    
    # Hiển thị actions mẫu
    actions = [
        int(np.argmax(x[0:3])),  # Move: 0=idle, 1=forward, 2=backward
        int(np.argmax(x[3:6])),  # Turn: 0=idle, 1=left, 2=right
        int(np.argmax(x[6:8])),  # Shoot: 0=no, 1=yes
    ]
    move_names = ["Đứng im", "Tiến", "Lùi"]
    turn_names = ["Không xoay", "Xoay trái", "Xoay phải"]
    shoot_names = ["Không bắn", "Bắn"]
    print(f"    Actions: {move_names[actions[0]]}, {turn_names[actions[1]]}, {shoot_names[actions[2]]}")
    
    return True


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Export trọng số PPO Model cho game C++")
    parser.add_argument("--phase", type=int, default=11,
                        help="Phase number của model cần export (mặc định: 11 = mạnh nhất)")
    parser.add_argument("--output", type=str, default=None,
                        help="Đường dẫn file output (mặc định: ../models/ai_model.bin)")
    args = parser.parse_args()
    
    model_path = f"models/ppo_tank_phase{args.phase}"
    if not os.path.exists(model_path + ".zip"):
        print(f"  [Lỗi] Không tìm thấy model tại {model_path}.zip!")
        print(f"  Các model có sẵn:")
        import glob
        for f in sorted(glob.glob("models/ppo_tank_phase*.zip")):
            print(f"    - {f}")
        sys.exit(1)
    
    output_path = args.output or os.path.join("..", "models", "ai_model.bin")
    
    print("=" * 60)
    print(f"  EXPORT PPO MODEL PHASE {args.phase} → BINARY C++")
    print("=" * 60)
    
    export_weights(model_path, output_path)
    test_inference(model_path, output_path)
    
    print("\n" + "=" * 60)
    print("  HOÀN THÀNH! File sẵn sàng cho game C++.")
    print(f"  Copy file '{output_path}' vào cùng thư mục với AZgame.exe")
    print("=" * 60)
