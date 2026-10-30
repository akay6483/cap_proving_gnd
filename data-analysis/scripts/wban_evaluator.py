import os
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path

# =============================================================================
# GLOBAL CONFIGURATION & AESTHETICS
# =============================================================================
# Enforce large fonts and distinct color mapping across all plots
sns.set_theme(style="whitegrid")
plt.rcParams.update({
    'figure.figsize': (14, 8),
    'axes.titlesize': 18,
    'axes.labelsize': 14,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'legend.fontsize': 12,
    'lines.linewidth': 2.5
})

COLOR_CRITICAL = '#D62728'  # Distinct Red
COLOR_RELIABLE = '#1F77B4'  # Distinct Blue
COLOR_ORDINARY = '#2CA02C'  # Distinct Green

HOME_DIR = Path.home()
BASE_DIR = HOME_DIR / "Documents/1_Projects/capstone/cap_proving_gnd/data-analysis"
CSV_DIR = BASE_DIR / "csv"
RESULTS_DIR = BASE_DIR / "results"
TB_LOG_DIR = RESULTS_DIR / "wban_ppo_logs"

RESULTS_DIR.mkdir(parents=True, exist_ok=True)

# =============================================================================
# RL AGENT CONVERGENCE
# =============================================================================
def plot_rl_training_convergence():
    progress_file = TB_LOG_DIR / "progress.csv"
    if not progress_file.exists():
        return

    dataframe = pd.read_csv(progress_file)
    figure, axis_left = plt.subplots()
    
    # Primary Axis: Policy Loss
    axis_left.set_xlabel('Training Updates')
    axis_left.set_ylabel('Policy Gradient Loss', color=COLOR_CRITICAL)
    axis_left.plot(dataframe['time/iterations'], dataframe['train/policy_gradient_loss'], 
                   color=COLOR_CRITICAL, label="Policy Loss")
    axis_left.tick_params(axis='y', labelcolor=COLOR_CRITICAL)

    # Secondary Axis: Value Loss
    axis_right = axis_left.twinx()
    axis_right.set_ylabel('Value Loss (Critic)', color=COLOR_RELIABLE)
    axis_right.plot(dataframe['time/iterations'], dataframe['train/value_loss'], 
                    color=COLOR_RELIABLE, linestyle="--", label="Value Loss")
    axis_right.tick_params(axis='y', labelcolor=COLOR_RELIABLE)

    plt.title("PPO Agent Convergence Dynamics")
    figure.tight_layout()
    plt.savefig(RESULTS_DIR / "rl_convergence_fixed.png", dpi=300)
    plt.close()

# =============================================================================
# RL AGENT GATE ACTIONS
# =============================================================================
def plot_rl_gate_actions():
    actions_file = CSV_DIR / "wban_sim_rl_actions.csv"
    if not actions_file.exists():
        return

    dataframe = pd.read_csv(actions_file)
    plt.figure()
    
    time_series = dataframe['SimTime_s']
    gate_critical = dataframe['Gate_CP_ms']
    gate_reliable = dataframe['Gate_RP_ms']
    gate_ordinary = dataframe['Gate_OP_ms']
    
    plt.stackplot(time_series, gate_critical, gate_reliable, gate_ordinary,
                  labels=['Critical (Priority 0)', 'Reliable (Priority 1)', 'Ordinary (Priority 2)'],
                  colors=[COLOR_CRITICAL, COLOR_RELIABLE, COLOR_ORDINARY], 
                  alpha=0.85)
    
    plt.title('Dynamic Gate Allocation Over Time (18ms Hypercycle)')
    plt.xlabel('Simulation Time (Seconds)')
    plt.ylabel('Gate Open Duration (ms)')
    plt.legend(loc='upper right', framealpha=1.0)
    plt.margins(x=0, y=0)
    
    plt.tight_layout()
    plt.savefig(RESULTS_DIR / "rl_gate_allocations_fixed.png", dpi=300)
    plt.close()

# =============================================================================
# QUEUE OCCUPANCY (FIXED SCALE VIA SUBPLOTS)
# =============================================================================
def plot_queue_occupancy_subplots():
    queues_file = CSV_DIR / "wban_sim_queues.csv"
    if not queues_file.exists():
        return

    dataframe = pd.read_csv(queues_file)
    time_series = dataframe['Time_s']
    
    # Create 3 isolated subplots to prevent line overlap
    figure, (axis_cp, axis_rp, axis_op) = plt.subplots(3, 1, sharex=True, figsize=(14, 10))
    
    axis_cp.plot(time_series, dataframe['CP_Size'], color=COLOR_CRITICAL)
    axis_cp.set_title('Critical Queue Occupancy')
    axis_cp.set_ylabel('Packets')
    
    axis_rp.plot(time_series, dataframe['RP_Size'], color=COLOR_RELIABLE)
    axis_rp.set_title('Reliable Queue Occupancy')
    axis_rp.set_ylabel('Packets')
    
    axis_op.plot(time_series, dataframe['OP_Size'], color=COLOR_ORDINARY)
    axis_op.set_title('Ordinary Queue Occupancy')
    axis_op.set_ylabel('Packets')
    axis_op.set_xlabel('Simulation Time (Seconds)')
    
    plt.tight_layout()
    plt.savefig(RESULTS_DIR / "telemetry_queues_fixed.png", dpi=300)
    plt.close()

# =============================================================================
# END-TO-END LATENCY CDF
# =============================================================================
def plot_latency_cdf():
    rx_file = CSV_DIR / "wban_sim_lpu_rx.csv"
    if not rx_file.exists():
        return

    dataframe = pd.read_csv(rx_file)
    dataframe['Delay_Microseconds'] = dataframe['E2EDelay_s'] * 1_000_000
    
    priority_mapping = {0: 'Critical (P0)', 1: 'Reliable (P1)', 2: 'Ordinary (P2)'}
    dataframe['Traffic_Class'] = dataframe['Priority'].map(priority_mapping)

    plt.figure()
    
    sns.ecdfplot(data=dataframe, x='Delay_Microseconds', hue='Traffic_Class', 
                 hue_order=['Critical (P0)', 'Reliable (P1)', 'Ordinary (P2)'], 
                 palette=[COLOR_CRITICAL, COLOR_RELIABLE, COLOR_ORDINARY])
    
    plt.axvline(x=15_000, color=COLOR_CRITICAL, linestyle='--', label='15ms Critical Deadline')
    plt.axvline(x=50_000, color=COLOR_RELIABLE, linestyle='--', label='50ms Reliable Deadline')
    
    plt.title('Cumulative Distribution Function (CDF) of End-to-End Latency')
    plt.xlabel('End-to-End Delay (Microseconds)')
    plt.ylabel('Cumulative Probability')
    
    # Force legend to display the manual deadline lines clearly
    handles, labels = plt.gca().get_legend_handles_labels()
    plt.legend(handles=handles, labels=labels, loc='lower right', framealpha=1.0)
    
    plt.tight_layout()
    plt.savefig(RESULTS_DIR / "telemetry_latency_cdf_fixed.png", dpi=300)
    plt.close()

# =============================================================================
# EXECUTION
# =============================================================================
if __name__ == "__main__":
    plot_rl_training_convergence()
    plot_rl_gate_actions()
    plot_queue_occupancy_subplots()
    plot_latency_cdf()
