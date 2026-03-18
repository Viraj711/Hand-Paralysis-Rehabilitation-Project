import pandas as pd
import numpy as np
from sklearn.preprocessing import PolynomialFeatures
from sklearn.linear_model import LinearRegression
from datetime import timedelta
import matplotlib.pyplot as plt

# -----------------------------
# CONFIGURATION
# -----------------------------
CSV_PATH = "D:/Work/RFL/Hand Paralysis Project/testing_only_physio_data.csv"
NUM_SESSIONS_TO_USE = 20
MIN_SESSIONS_FOR_PREDICTION = 10
USE_LATEST_SESSIONS = False
MAX_ROM = 170  # maximum physiologically possible range of motion

# -----------------------------
# STEP 1: LOAD DATA
# -----------------------------
df = pd.read_csv(CSV_PATH)
df['timestamp'] = pd.to_datetime(df['timestamp'])

# -----------------------------
# STEP 2: DETERMINE WHICH SESSIONS TO USE
# -----------------------------
all_sessions = sorted(df['session_id'].unique())
if len(all_sessions) < NUM_SESSIONS_TO_USE:
    print(f"⚠️ Dataset only has {len(all_sessions)} sessions — using all available data.")
    selected_sessions = all_sessions
else:
    selected_sessions = all_sessions[-NUM_SESSIONS_TO_USE:] if USE_LATEST_SESSIONS else all_sessions[:NUM_SESSIONS_TO_USE]

df_selected = df[df['session_id'].isin(selected_sessions)]

# -----------------------------
# STEP 3: CALCULATE RANGE OF MOTION (ROM) PER SESSION
# -----------------------------
# Clip angle_value to physiologically possible range
df_selected['angle_value'] = df_selected['angle_value'].clip(-70, 70)

rom_per_session = (
    df_selected.groupby('session_id')['angle_value']
    .agg(lambda x: x.max() - x.min())
    .reset_index()
    .rename(columns={'angle_value': 'ROM'})
)

num_sessions = len(rom_per_session)
X = rom_per_session['session_id'].values.reshape(-1, 1)
y = rom_per_session['ROM'].values

# -----------------------------
# STEP 4: CHECK DATA SUFFICIENCY
# -----------------------------
if num_sessions < MIN_SESSIONS_FOR_PREDICTION:
    print("❌ Not enough data available to predict recovery progress.")
else:
    # -----------------------------
    # STEP 5: POLYNOMIAL REGRESSION MODEL (2nd degree)
    # -----------------------------
    poly = PolynomialFeatures(degree=2)
    X_poly = poly.fit_transform(X)
    model = LinearRegression()
    model.fit(X_poly, y)

    # -----------------------------
    # STEP 6: PREDICT FUTURE SESSIONS
    # -----------------------------
    # Predict until ROM reaches 95% of MAX_ROM
    predicted_session = None
    current_session = int(X[-1][0])
    max_predict_sessions = 1000  # safety to prevent infinite loop

    for future_session in range(current_session, current_session + max_predict_sessions):
        y_pred = model.predict(poly.transform(np.array([[future_session]])))[0]
        y_pred = min(y_pred, MAX_ROM)  # cap only ROM, not sessions
        if y_pred >= 0.95 * MAX_ROM:
            predicted_session = future_session
            break

    if predicted_session is None:
        predicted_session = current_session + max_predict_sessions

    remaining_sessions = max(0, predicted_session - current_session)
    current_rom = y[-1]

    # -----------------------------
    # STEP 7: DISPLAY RESULTS
    # -----------------------------
    print("📈 Recovery Progress Prediction:")
    print(f"  • Sessions analyzed: {num_sessions}")
    print(f"  • Current ROM: {current_rom:.2f}")
    print(f"  • Predicted total sessions for near full recovery: {predicted_session}")
    print(f"  • Estimated sessions remaining: {remaining_sessions}")

    last_timestamp = df_selected['timestamp'].max()
    estimated_completion = last_timestamp + timedelta(days=int(remaining_sessions))
    print(f"  • Estimated full recovery date: {estimated_completion.strftime('%Y-%m-%d')}")

    # -----------------------------
    # STEP 8: VISUALIZATION
    # -----------------------------
    # Plot past sessions
    plt.scatter(X, y, color='blue', label="Actual ROM per session")

    # Plot predicted curve for next sessions
    X_future = np.arange(int(X.min()), predicted_session + 1).reshape(-1,1)
    y_future = model.predict(poly.transform(X_future))
    y_future = np.minimum(y_future, MAX_ROM)  # cap only ROM

    plt.plot(X_future, y_future, color='orange', label="Polynomial trend (fitted curve)")
    plt.axvline(predicted_session, color='red', linestyle='--', label='Predicted full recovery')
    plt.ylim(0, MAX_ROM + 10)
    plt.xlabel("Session ID")
    plt.ylabel("Range of Motion (ROM)")
    plt.title(f"Recovery Prediction (using {num_sessions} sessions)")
    plt.legend()
    plt.show()