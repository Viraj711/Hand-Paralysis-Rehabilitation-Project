import firebase_admin
from firebase_admin import credentials, db
import pandas as pd
import os
from datetime import datetime

# ---------------- FIREBASE SETUP ----------------
cred = credentials.Certificate(
    "D:/Work/RFL/Hand Paralysis Project/FirebaseJSON/hand-paralysis-83b96-firebase-adminsdk-fbsvc-eaa00c1a3a.json"
)
firebase_admin.initialize_app(cred, {
    "databaseURL": "https://hand-paralysis-83b96-default-rtdb.asia-southeast1.firebasedatabase.app/"
})

# ---------------- CSV SETUP ----------------
csv_file = "D:/Work/RFL/Hand Paralysis Project/imu_data.csv"
if not os.path.isfile(csv_file):
    df = pd.DataFrame(columns=["session_id", "mode", "rep_no", "angle_value", "timestamp"])
    df.to_csv(csv_file, index=False)

# ---------------- PARAMETERS ----------------
latest = {"sessionID": None, "mode": None, "y": None, "z": None}
rep_no = 1
rep_state = None  # Can be 'neutral', 'neg', 'pos'

# ---------------- HELPER FUNCTION ----------------
def detect_rep(current_value):
    global rep_state, rep_no

    threshold = 1e-3
    if abs(current_value) < threshold:
        if rep_state in ['pos', 'neg']:
            rep_no += 1
        rep_state = 'neutral'
    elif current_value < 0:
        rep_state = 'neg'
    elif current_value > 0:
        rep_state = 'pos'

    return rep_no

# ---------------- LISTENER ----------------
def listener(event):
    global latest, rep_no

    if event.data is None:
        return

    # Update latest values
    if isinstance(event.data, dict):
        latest["sessionID"] = event.data.get("sessionID", latest["sessionID"])
        latest["mode"] = event.data.get("mode", latest["mode"])
        latest["y"] = event.data.get("y", latest["y"])
        latest["z"] = event.data.get("z", latest["z"])
    else:
        key = event.path.strip("/").split("/")[-1]
        if key in latest:
            latest[key] = event.data

    # Ensure sessionID is available
    if latest["sessionID"] is None:
        return  # Skip logging until sessionID exists

    # Determine angle value based on mode
    mode = latest.get("mode")
    if mode is None:
        return

    angle_value = None
    if mode == 1 and latest["y"] is not None:
        angle_value = latest["y"]
    elif mode == 2 and latest["z"] is not None:
        angle_value = latest["z"]
    else:
        return

    # Update rep number
    current_rep = detect_rep(angle_value)

    # Timestamp in milliseconds
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]

    # Append row to CSV
    row = {
        "session_id": latest["sessionID"],
        "mode": mode,
        "rep_no": current_rep,
        "angle_value": angle_value,
        "timestamp": timestamp
    }
    pd.DataFrame([row]).to_csv(csv_file, mode='a', header=False, index=False)
    print("Logged:", row)

# ---------------- START LISTENER ----------------
print("Listening to Firebase IMU node...")
ref = db.reference("IMU")
ref.listen(listener)