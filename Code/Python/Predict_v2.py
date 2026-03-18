import pandas as pd
import numpy as np
from sklearn.preprocessing import PolynomialFeatures
from sklearn.linear_model import LinearRegression
from datetime import timedelta
import matplotlib.pyplot as plt
import tkinter as tk
from tkinter import messagebox

# -----------------------------
# CONFIGURATION
# -----------------------------
CSV_PATH = "D:/Work/RFL/Hand Paralysis Project/testing_only_physio_data.csv"
MIN_SESSIONS_FOR_PREDICTION = 10
USE_LATEST_SESSIONS = False
MAX_ROM = 170  # max physiologically possible ROM

# -----------------------------
# STEP 1: LOAD DATA
# -----------------------------
df = pd.read_csv(CSV_PATH)
df['timestamp'] = pd.to_datetime(df['timestamp'])

# Clip angle_value to physiologically possible range
df['angle_value'] = df['angle_value'].clip(-70, 70)

# -----------------------------
# FUNCTION TO PLOT & PREDICT
# -----------------------------
def predict_and_plot(num_sessions_to_use):
    # Close any previously opened plots
    plt.close('all')  

    all_sessions = sorted(df['session_id'].unique())
    
    if len(all_sessions) < num_sessions_to_use:
        messagebox.showwarning("Warning", f"Dataset only has {len(all_sessions)} sessions — using all available data.")
        selected_sessions = all_sessions
    else:
        selected_sessions = all_sessions[-num_sessions_to_use:] if USE_LATEST_SESSIONS else all_sessions[:num_sessions_to_use]

    df_selected = df[df['session_id'].isin(selected_sessions)]

    rom_per_session = (
        df_selected.groupby('session_id')['angle_value']
        .agg(lambda x: x.max() - x.min())
        .reset_index()
        .rename(columns={'angle_value': 'ROM'})
    )

    num_sessions = len(rom_per_session)
    if num_sessions < MIN_SESSIONS_FOR_PREDICTION:
        messagebox.showinfo("Info", "Not enough data available to predict recovery progress.")
        return

    X = rom_per_session['session_id'].values.reshape(-1, 1)
    y = rom_per_session['ROM'].values

    # Polynomial regression (2nd degree)
    poly = PolynomialFeatures(degree=2)
    X_poly = poly.fit_transform(X)
    model = LinearRegression()
    model.fit(X_poly, y)

    # Predict future sessions dynamically
    predicted_session = None
    current_session = int(X[-1][0])
    max_predict_sessions = 1000

    for future_session in range(current_session, current_session + max_predict_sessions):
        y_pred = model.predict(poly.transform(np.array([[future_session]])))[0]
        y_pred = min(y_pred, MAX_ROM)
        if y_pred >= 0.95 * MAX_ROM:
            predicted_session = future_session
            break

    if predicted_session is None:
        predicted_session = current_session + max_predict_sessions

    remaining_sessions = max(0, predicted_session - current_session)
    current_rom = y[-1]

    last_timestamp = df_selected['timestamp'].max()
    estimated_completion = last_timestamp + timedelta(days=int(remaining_sessions))

    # Display results
    print("📈 Recovery Progress Prediction:")
    print(f"  • Sessions analyzed: {num_sessions}")
    print(f"  • Current ROM: {current_rom:.2f}")
    print(f"  • Predicted total sessions for near full recovery: {predicted_session}")
    print(f"  • Estimated sessions remaining: {remaining_sessions}")
    print(f"  • Estimated full recovery date: {estimated_completion.strftime('%Y-%m-%d')}")

    # Plot
    plt.figure(figsize=(10,5))
    plt.scatter(X, y, color='blue', label="Actual ROM per session")
    
    X_future = np.arange(int(X.min()), predicted_session + 1).reshape(-1,1)
    y_future = model.predict(poly.transform(X_future))
    y_future = np.minimum(y_future, MAX_ROM)

    plt.plot(X_future, y_future, color='orange', label="Polynomial trend (fitted curve)")
    plt.axvline(predicted_session, color='red', linestyle='--', label='Predicted full recovery')
    plt.ylim(0, MAX_ROM + 10)
    plt.xlabel("Session ID")
    plt.ylabel("Range of Motion (ROM)")
    plt.title(f"Recovery Prediction (using {num_sessions} sessions)")
    plt.legend()
    plt.show()
# -----------------------------
# TKINTER GUI
# -----------------------------
def on_go():
    try:
        n = int(entry_sessions.get())
        if n <= 0:
            messagebox.showerror("Error", "Enter a positive integer")
            return
        predict_and_plot(n)
    except ValueError:
        messagebox.showerror("Error", "Invalid input. Enter an integer.")

root = tk.Tk()
root.title("Recovery Prediction")

tk.Label(root, text="Number of sessions to use:").grid(row=0, column=0, padx=10, pady=10)
entry_sessions = tk.Entry(root)
entry_sessions.grid(row=0, column=1, padx=10, pady=10)
entry_sessions.insert(0, "20")

btn_go = tk.Button(root, text="Go", command=on_go)
btn_go.grid(row=0, column=2, padx=10, pady=10)

root.mainloop()