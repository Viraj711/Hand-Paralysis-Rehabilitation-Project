import firebase_admin
from firebase_admin import credentials, db
import pandas as pd
import os
import time

# Firebase setup
cred = credentials.Certificate(
    "D:/Work/RFL/Hand Paralysis Project/FirebaseJSON/hand-paralysis-project-firebase-adminsdk-fbsvc-8b270fe4eb.json"
)
firebase_admin.initialize_app(cred, {
    "databaseURL": "https://hand-paralysis-project-default-rtdb.asia-southeast1.firebasedatabase.app/"
})

# CSV setup
csv_file = "D:\Work\RFL\Hand Paralysis Project\imu_data.csv"
if not os.path.isfile(csv_file):
    df = pd.DataFrame(columns=["timestamp", "pitch", "roll", "yaw"])
    df.to_csv(csv_file, index=False)

# Temporary storage for latest values
latest_values = {"pitch": None, "roll": None, "yaw": None}

def listener(event):
    global latest_values
    if event.data is None:
        return

    # Determine which key was updated
    if isinstance(event.data, dict):
        # Full object sent
        latest_values["pitch"] = event.data.get("pitch", latest_values["pitch"])
        latest_values["roll"]  = event.data.get("roll", latest_values["roll"])
        latest_values["yaw"]   = event.data.get("yaw", latest_values["yaw"])
    else:
        # Single value sent
        key = event.path.strip("/").split("/")[-1]
        if key in latest_values:
            latest_values[key] = event.data

    # Only log when all three values are present
    if all(v is not None for v in latest_values.values()):
        row = {
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "pitch": latest_values["pitch"],
            "roll": latest_values["roll"],
            "yaw": latest_values["yaw"]
        }
        pd.DataFrame([row]).to_csv(csv_file, mode='a', header=False, index=False)
        print("Logged:", row)

        # Reset latest_values for next batch
        latest_values = {"pitch": None, "roll": None, "yaw": None}

# Start listening
print("Starting IMU logger...")
ref = db.reference("IMU")  # listen on the IMU node
ref.listen(listener)  # keeps running