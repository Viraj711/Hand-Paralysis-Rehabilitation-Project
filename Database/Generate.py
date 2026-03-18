import pandas as pd
import numpy as np
from datetime import datetime, timedelta

# Simulation parameters
num_sessions = 60
testing_reps = 5
sampling_interval = 0.5   # seconds
rep_duration = 10.0        # seconds per repetition

# Base and target ranges for each mode
flexion_range = (0, 0)      # mode 1 (start)
deviation_range = (0, 0)     # mode 2 (start)
flexion_target = (-70, 70)
deviation_target = (-20, 35)

# Logistic recovery curve (more realistic than linear)
def recovery_curve(session, total_sessions):
    x = session / total_sessions * 10
    return 1 / (1 + np.exp(-(x - 5)))  # 0–1 sigmoid curve

# Data container
data = []
start_time = datetime(2025, 1, 1)

for session in range(1, num_sessions + 1):
    progress = recovery_curve(session, num_sessions)

    # Gradually increase motion range
    flex_min = flexion_range[0] + progress * (flexion_target[0] - flexion_range[0])
    flex_max = flexion_range[1] + progress * (flexion_target[1] - flexion_range[1])
    dev_min = deviation_range[0] + progress * (deviation_target[0] - deviation_range[0])
    dev_max = deviation_range[1] + progress * (deviation_target[1] - deviation_range[1])

    for mode in [1, 2]:
        if mode == 1:
            low, high = flex_min, flex_max
        else:
            low, high = dev_min, dev_max

        amplitude = (high - low) / 2
        mid = (high + low) / 2

        for rep in range(1, testing_reps + 1):
            # Fatigue factor (slightly less amplitude as reps progress)
            fatigue = 1 - (rep / testing_reps) * np.random.uniform(0, 0.1)
            amp = amplitude * fatigue

            # Generate samples every 0.5s for this 3s repetition
            t_points = np.arange(0, rep_duration, sampling_interval)
            for t in t_points:
                # Sine wave for natural motion
                angle = mid + amp * np.sin((t / rep_duration) * 2 * np.pi)
                noise = np.random.normal(0, amp * 0.05)  # natural human noise
                angle_value = round(angle + noise, 2)

                timestamp = start_time + timedelta(
                    minutes=session * 10
                ) + timedelta(seconds=(mode * 100) + (rep - 1) * rep_duration + t)

                data.append([session, mode, rep, angle_value, timestamp])

# Create DataFrame
df = pd.DataFrame(data, columns=["session_id", "mode", "rep_no", "angle_value", "timestamp"])

# Save to CSV
df.to_csv("testing_only_physio_data.csv", index=False)
print("✅ Testing-only physiotherapy data generated: testing_only_physio_data.csv")

# Quick check
print(df.groupby(["session_id", "mode"]).angle_value.agg(["min", "max"]).head(10))