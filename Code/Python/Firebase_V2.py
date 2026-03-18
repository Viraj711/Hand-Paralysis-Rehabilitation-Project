import firebase_admin
from firebase_admin import credentials, db
import pandas as pd
import os
import time
from datetime import datetime

# ---------------- FIREBASE SETUP ----------------
cred = credentials.Certificate(
    "D:/Work/RFL/Hand Paralysis Project/FirebaseJSON/hand-paralysis-project-firebase-adminsdk-fbsvc-8b270fe4eb.json"
)
firebase_admin.initialize_app(cred, {
    "databaseURL": "https://hand-paralysis-project-default-rtdb.asia-southeast1.firebasedatabase.app/"
})

# ---------------- CSV SETUP ----------------
csv_file = "D:/Work/RFL/Hand Paralysis Project/imu_data.csv"
if not os.path.isfile(csv_file):
    df = pd.DataFrame(columns=["timestamp", "mode", "value"])
    df.to_csv(csv_file, index=False)

# Store latest values
latest = {"mode": None, "y": None, "z": None}

def listener(event):
    global latest

    if event.data is None:
        return

    # Detect whether it's a full object or single update
    if isinstance(event.data, dict):
        latest["mode"] = event.data.get("mode", latest["mode"])
        latest["y"] = event.data.get("y", latest["y"])
        latest["z"] = event.data.get("z", latest["z"])
    else:
        key = event.path.strip("/").split("/")[-1]
        if key in latest:
            latest[key] = event.data

    # Log only when mode and at least one value (y or z) are present
    if latest["mode"] is not None:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]  # trim microseconds to ms
        mode = latest["mode"]

        if mode == 1 and latest["y"] is not None:
            value = latest["y"]
        elif mode == 2 and latest["z"] is not None:
            value = latest["z"]
        else:
            return  # no valid data yet

        # Append to CSV
        row = {"timestamp": timestamp, "mode": mode, "value": value}
        pd.DataFrame([row]).to_csv(csv_file, mode='a', header=False, index=False)
        print("Logged:", row)

        # Reset y/z to avoid duplicate writes
        latest["y"] = None
        latest["z"] = None

# ---------------- START LISTENER ----------------
print("Listening to Firebase IMU node...")
ref = db.reference("IMU")
ref.listen(listener)