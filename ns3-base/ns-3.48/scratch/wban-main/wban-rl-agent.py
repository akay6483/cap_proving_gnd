import gymnasium as gym
import numpy as np
from multiprocessing import shared_memory
import struct
import time
from stable_baselines3 import PPO

class WbanGclEnv(gym.Env):
    def __init__(self):
        super().__init__()
        self.action_space = gym.spaces.Box(low=0.0, high=1.0, shape=(3,), dtype=np.float32)
        self.observation_space = gym.spaces.Box(low=0.0, high=np.inf, shape=(12,), dtype=np.float32)

        # Connect to the POSIX shared memory created by C++
        self.shm = shared_memory.SharedMemory(name="wban_rl_shm")
        
        # Memory Layout: 2x 32-bit ints (flag + padding) + 15x 64-bit doubles
        self.struct_fmt = '=i i 15d'
        self.struct_size = struct.calcsize(self.struct_fmt)

    def step(self, action):
        # Softmax normalization for gate ratios
        exp_a = np.exp(action - np.max(action))
        gate_ratios = exp_a / np.sum(exp_a)

        # Unpack memory block
        data = list(struct.unpack(self.struct_fmt, self.shm.buf[:self.struct_size]))
        
        # Update gateRatios (indices 14, 15, 16 in the unpacked tuple)
        data[14:17] = gate_ratios
        
        # Change flag to 0 to yield control back to C++
        data[0] = 0
        self.shm.buf[:self.struct_size] = struct.pack(self.struct_fmt, *data)

        # Wait for C++ to simulate the hypercycle and flip the flag back to 1
        while True:
            flag = struct.unpack('=i', self.shm.buf[:4])[0]
            if flag == 1:
                break
            time.sleep(0.0001)

        # Read the new state
        new_data = struct.unpack(self.struct_fmt, self.shm.buf[:self.struct_size])
        
        # Extract the 12 state metrics (indices 2 through 13)
        obs = np.array(new_data[2:14], dtype=np.float32)
        
        # Calculate Reward (Penalize CP delay/drops heavily)
        delay_penalty = (obs[6]*50.0) + (obs[7]*10.0) + (obs[8]*1.0)
        drop_penalty = (obs[9]*100.0) + (obs[10]*20.0) + (obs[11]*2.0)
        reward = -(delay_penalty + drop_penalty)
        
        return obs, float(reward), False, False, {}

    def reset(self, seed=None, options=None):
        super().reset(seed=seed)
        return np.zeros(12, dtype=np.float32), {}

    def close(self):
        # Gracefully release the shared memory file descriptor on exit
        if hasattr(self, 'shm'):
            self.shm.close()

if __name__ == "__main__":
    print("Waiting for ns-3 to initialize shared memory...")
    while True:
        try:
            env = WbanGclEnv()
            break
        except FileNotFoundError:
            time.sleep(1)

    try:
        # Force PyTorch to use the CPU to bypass the MX330 architecture mismatch
        model = PPO("MlpPolicy", env, verbose=1, device="cpu")
        model.learn(total_timesteps=10000)
    finally:
        # Ensure shared memory is closed even if the script is interrupted or crashes
        env.close()
        print("Environment closed and shared memory released.")