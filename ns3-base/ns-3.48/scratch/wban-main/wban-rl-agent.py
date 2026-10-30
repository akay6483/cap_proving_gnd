import os
import csv
import argparse
import gymnasium as gym
import numpy as np
from multiprocessing import shared_memory, resource_tracker
import struct
import time
from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import DummyVecEnv, VecNormalize
from stable_baselines3.common.logger import configure
from stable_baselines3.common.callbacks import BaseCallback

# ============================================================================
# DIRECTORY ROUTING
# ============================================================================
BASE_DIR = os.path.expanduser("~/Documents/1_Projects/capstone/cap_proving_gnd/data-analysis")
CSV_DIR = os.path.join(BASE_DIR, "csv")
RESULTS_DIR = os.path.join(BASE_DIR, "results")
TB_LOG_DIR = os.path.join(RESULTS_DIR, "wban_ppo_logs")
MODEL_PATH = os.path.join(RESULTS_DIR, "wban_ppo_trained_model")
STATS_PATH = os.path.join(RESULTS_DIR, "vec_normalize.pkl")

os.makedirs(CSV_DIR, exist_ok=True)
os.makedirs(RESULTS_DIR, exist_ok=True)
os.makedirs(TB_LOG_DIR, exist_ok=True)

# ============================================================================
# CUSTOM CALLBACK LOGGER
# ============================================================================
class WbanRlLoggerCallback(BaseCallback):
    def __init__(self, filename=os.path.join(CSV_DIR, "wban_sim_rl_actions.csv"), verbose=0):
        super().__init__(verbose)
        self.filename = filename
        self.file = None
        self.writer = None
        self.hypercycle_ms = 10.0 # CHANGED: 10ms sync

    def _on_training_start(self) -> None:
        self.file = open(self.filename, mode='w', newline='')
        self.writer = csv.writer(self.file)
        self.writer.writerow([
            "Step", "SimTime_s", 
            "Action_CP_Ratio", "Action_RP_Ratio", "Action_OP_Ratio",
            "Gate_CP_ms", "Gate_RP_ms", "Gate_OP_ms",
            "Obs_CP_Size", "Obs_RP_Size", "Obs_OP_Size",
            "Obs_CP_Delay", "Obs_RP_Delay", "Obs_OP_Delay",
            "Reward"
        ])

    def _on_step(self) -> bool:
        actions = self.locals["actions"][0]
        rewards = self.locals["rewards"][0]
        # Pull original unnormalized obs for logging
        obs = self.training_env.get_original_obs()[0] 

        exp_a = np.exp(actions - np.max(actions))
        ratios = exp_a / np.sum(exp_a)
        gate_durations = ratios * self.hypercycle_ms
        sim_time = 1.0 + (self.n_calls * (self.hypercycle_ms / 1000.0))

        self.writer.writerow([
            self.n_calls, f"{sim_time:.4f}",
            f"{ratios[0]:.4f}", f"{ratios[1]:.4f}", f"{ratios[2]:.4f}",
            f"{gate_durations[0]:.2f}", f"{gate_durations[1]:.2f}", f"{gate_durations[2]:.2f}",
            obs[0], obs[1], obs[2],
            obs[6], obs[7], obs[8],
            f"{rewards:.4f}"
        ])
        
        if self.n_calls % 50 == 0:
            self.file.flush()

        return True

    def _on_training_end(self) -> None:
        if self.file:
            self.file.flush()
            self.file.close()

# ============================================================================
# GYM ENVIRONMENT (PHASE 3)
# ============================================================================
class WbanGclEnv(gym.Env):
    def __init__(self):
        super().__init__()
        self.action_space = gym.spaces.Box(low=0.0, high=1.0, shape=(3,), dtype=np.float32)
        self.observation_space = gym.spaces.Box(low=0.0, high=np.inf, shape=(12,), dtype=np.float32)

        self.MAX_QUEUE_PACKETS = 50.0  # CHANGED: Realistic Buffer ceiling
        self.TARGET_DELAY_CP = 0.015   # 15ms target
        self.TARGET_DELAY_RP = 0.050   # 50ms target

        self.shm = shared_memory.SharedMemory(name="wban_rl_shm")
        resource_tracker.unregister(self.shm._name, 'shared_memory')
        self.struct_fmt = '=i i 15d'
        self.struct_size = struct.calcsize(self.struct_fmt)

    def step(self, action):
        exp_a = np.exp(action - np.max(action))
        gate_ratios = exp_a / np.sum(exp_a)

        data = list(struct.unpack(self.struct_fmt, self.shm.buf[:self.struct_size]))
        data[14:17] = gate_ratios
        data[0] = 0
        self.shm.buf[:self.struct_size] = struct.pack(self.struct_fmt, *data)

        while True:
            flag = struct.unpack('=i', self.shm.buf[:4])[0]
            if flag == 1:
                break
            elif flag == -1:
                raise InterruptedError("Simulation naturally completed by ns-3.")
            time.sleep(0.0001)

        new_data = struct.unpack(self.struct_fmt, self.shm.buf[:self.struct_size])
        obs = np.array(new_data[2:14], dtype=np.float32)
        
        # CHANGED: Advanced Multi-Objective Reward Calculation
        cp_utilization = min(obs[0] / self.MAX_QUEUE_PACKETS, 1.0)
        rp_utilization = min(obs[1] / self.MAX_QUEUE_PACKETS, 1.0)
        
        reward = 0.0
        
        # 1. Deadline Penalties
        if obs[6] > self.TARGET_DELAY_CP: 
            reward -= (obs[6] / self.TARGET_DELAY_CP) * 5.0
        if obs[7] > self.TARGET_DELAY_RP: 
            reward -= (obs[7] / self.TARGET_DELAY_RP) * 2.0
            
        # 2. Queue Capacity Penalty (punish before drops occur)
        reward -= (cp_utilization**2 * 10.0)
        reward -= (rp_utilization**2 * 5.0)
        
        # 3. Best-Effort Starvation Check
        if gate_ratios[2] < 0.05: 
            reward -= 2.0
            
        # 4. Gradient Exploder Safeguard
        clipped_reward = max(reward, -20.0)
        
        return obs, float(clipped_reward), False, False, {}

    def reset(self, seed=None, options=None):
        super().reset(seed=seed)
        return np.zeros(12, dtype=np.float32), {}

    def close(self):
        if hasattr(self, 'shm'):
            self.shm.close()

# ============================================================================
# EXECUTION ORCHESTRATOR
# ============================================================================
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", type=str, choices=["train", "eval"], required=True, help="Execution mode: train or eval")
    args = parser.parse_args()

    print("Waiting for ns-3 to initialize shared memory...")
    while True:
        try:
            base_env = WbanGclEnv()
            break
        except FileNotFoundError:
            time.sleep(1)

    try:
        # Wrap environment in Normalizer
        env = DummyVecEnv([lambda: base_env])
        
        if args.mode == "train":
            print("--- INITIATING RL TRAINING ---")
            env = VecNormalize(env, norm_obs=True, norm_reward=True, clip_obs=10.0)
            new_logger = configure(TB_LOG_DIR, ["stdout", "csv", "tensorboard"])
            
            # Deep Network [128,128] with Fast Rollouts (512)
            model = PPO("MlpPolicy", env, n_steps=512, policy_kwargs=dict(net_arch=[128, 128]), verbose=1, device="cpu")
            model.set_logger(new_logger)
            action_logger = WbanRlLoggerCallback()

            model.learn(total_timesteps=int(1e9), callback=action_logger)
            
        elif args.mode == "eval":
            print("--- INITIATING RL EVALUATION ---")
            env = VecNormalize.load(STATS_PATH, env)
            env.training = False       # Freeze normalization weights
            env.norm_reward = False    # Predict uses raw rewards
            
            model = PPO.load(MODEL_PATH, env=env)
            obs = env.reset()
            
            while True:
                # Deterministic True forces the optimal policy
                action, _states = model.predict(obs, deterministic=True)
                obs, rewards, dones, infos = env.step(action)

    except InterruptedError as e:
        print(f"\nSimulation ended safely: {e}")
        if args.mode == "train":
            model.save(MODEL_PATH)
            env.save(STATS_PATH)
            print("Agent and normalization weights saved.")
    finally:
        base_env.close()