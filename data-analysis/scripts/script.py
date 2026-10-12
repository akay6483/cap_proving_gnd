import re
import numpy as np
from collections import defaultdict, Counter

def analyze_wban_logs(filename='output.txt'):
    # Regex pattern matching the ns-3 log format:
    # [T=3599.27s] Node 10 | Sample Generated -> Class: DP (Delay Packet) | Size: 245B | Buffer Total: 498B
    pattern = re.compile(
        r'\[T=([\d.]+)s\]\s+Node\s+(\d+)\s+\|\s+Sample Generated\s+->\s+Class:\s+([A-Za-z0-9\s()]+?)\s+\|\s+Size:\s+(\d+)B'
    )
    
    node_packets = defaultdict(list)
    global_packet_types = Counter()
    node_timestamps = defaultdict(list)
    node_sizes = defaultdict(list)
    
    try:
        with open(filename, 'r') as f:
            for line in f:
                match = pattern.search(line)
                if match:
                    t = float(match.group(1))
                    node_id = int(match.group(2))
                    pkt_class = match.group(3).strip()
                    size = int(match.group(4))
                    
                    node_packets[node_id].append(pkt_class)
                    global_packet_types[pkt_class] += 1
                    node_timestamps[node_id].append(t)
                    node_sizes[node_id].append(size)
    except FileNotFoundError:
        print(f"Error: {filename} not found in the current directory.")
        return

    total_global_packets = sum(global_packet_types.values())
    if total_global_packets == 0:
        print("No valid packet generation logs found matching the pattern.")
        return

    print("="*60)
    print(" 1. GLOBAL PACKET TYPE DISTRIBUTION & RATIOS")
    print("="*60)
    for pkt, count in global_packet_types.items():
        ratio = (count / total_global_packets) * 100
        print(f"  - {pkt}: {count} packets ({ratio:.2f}%)")
    print(f"  Total Packets Generated Across All Nodes: {total_global_packets}\n")
    
    print("="*60)
    print(" 2. PER-NODE PACKET TYPE RATIOS")
    print("="*60)
    for node_id in sorted(node_packets.keys()):
        pkts = node_packets[node_id]
        total_node_pkts = len(pkts)
        counts = Counter(pkts)
        print(f"Node {node_id} (Total Packets: {total_node_pkts}):")
        for pkt, count in counts.items():
            ratio = (count / total_node_pkts) * 100
            print(f"    -> {pkt}: {count} ({ratio:.2f}%)")
    print()
            
    print("="*60)
    print(" 3. PACKET SIZE STATISTICS (Mean & Standard Deviation)")
    print("="*60)
    for node_id in sorted(node_sizes.keys()):
        sizes = node_sizes[node_id]
        if sizes:
            mean_size = np.mean(sizes)
            std_size = np.std(sizes, ddof=1) if len(sizes) > 1 else 0.0
            min_size, max_size = np.min(sizes), np.max(sizes)
            print(f"Node {node_id:2d} | Mean: {mean_size:6.2f}B | Std Dev: {std_size:5.2f}B | Min-Max: [{min_size}, {max_size}]B (Count: {len(sizes)})")
    print()
            
    print("="*60)
    print(" 4. PACKET GENERATION TIME INTERVAL STATISTICS (Mean & Standard Deviation)")
    print("="*60)
    for node_id in sorted(node_timestamps.keys()):
        ts = sorted(node_timestamps[node_id])
        if len(ts) > 1:
            intervals = np.diff(ts)
            mean_int = np.mean(intervals)
            std_int = np.std(intervals, ddof=1)
            min_int, max_int = np.min(intervals), np.max(intervals)
            print(f"Node {node_id:2d} | Mean Interval: {mean_int:7.4f}s | Std Dev: {std_int:7.4f}s | Min-Max: [{min_int:.4f}, {max_int:.4f}]s")
        else:
            print(f"Node {node_id:2d} | Insufficient timestamps for interval calculation.")
    print("="*60)

if __name__ == '__main__':
    analyze_wban_logs()
