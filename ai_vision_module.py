"""
MEDIBOT - AI Vision Module (Medicine Identification)
------------------------------------------------------
Runs on: Raspberry Pi 4B with Pi Camera
Function: Captures an image at the destination, runs YOLO object
          detection to identify the medicine, verifies it against
          the prescribed medicine (fetched from Firebase), and sends
          the verification result back to the mobile application.

Based on AI Vision Algorithm (Medicine Identification) documented in
the MEDIBOT project report, Section 3.2.2.

Setup:
    pip install ultralytics opencv-python firebase-admin

    You will need:
    - A trained YOLO model file (e.g. medicine_yolov8.pt) — train this
      using labeled images of your medicine packets (15+ drug labels)
    - A Firebase service account JSON key for cloud read/write
"""

import cv2
import time
import json
from ultralytics import YOLO
import firebase_admin
from firebase_admin import credentials, db

# ---------- Configuration ----------
MODEL_PATH = "medicine_yolov8.pt"          # Your trained YOLO weights
FIREBASE_CRED_PATH = "firebase_key.json"   # Service account key
FIREBASE_DB_URL = "https://medibot-default-rtdb.firebaseio.com/"
CONFIDENCE_THRESHOLD = 0.6

# ---------- Initialize Firebase ----------
cred = credentials.Certificate(FIREBASE_CRED_PATH)
firebase_admin.initialize_app(cred, {"databaseURL": FIREBASE_DB_URL})

# ---------- Load YOLO Model ----------
model = YOLO(MODEL_PATH)


def get_prescribed_medicine(bed_id):
    """Fetch the medicine that should be delivered to a given bed from Firebase."""
    ref = db.reference(f"prescriptions/{bed_id}")
    data = ref.get()
    if data and "medicine" in data:
        return data["medicine"]
    return None


def capture_image():
    """Capture a single frame from the Pi Camera."""
    cam = cv2.VideoCapture(0)
    time.sleep(1)  # allow camera to warm up
    ret, frame = cam.read()
    cam.release()
    if not ret:
        raise RuntimeError("Failed to capture image from Pi Camera")
    return frame


def detect_medicine(frame):
    """
    Run YOLO detection on the captured frame.
    Returns the detected medicine label with highest confidence, or None.
    """
    results = model.predict(source=frame, conf=CONFIDENCE_THRESHOLD, verbose=False)

    best_label = None
    best_conf = 0

    for result in results:
        for box in result.boxes:
            conf = float(box.conf[0])
            cls_id = int(box.cls[0])
            label = model.names[cls_id]

            if conf > best_conf:
                best_conf = conf
                best_label = label

    return best_label, best_conf


def send_verification_result(bed_id, detected_label, prescribed_label, match):
    """Push the verification result to Firebase so the mobile app can read it."""
    ref = db.reference(f"verification/{bed_id}")
    ref.set({
        "detected_medicine": detected_label,
        "prescribed_medicine": prescribed_label,
        "match": match,
        "timestamp": time.time()
    })


def verify_medicine_for_bed(bed_id):
    """
    Full pipeline: capture image -> detect medicine -> compare with
    prescription -> send result.
    """
    print(f"[INFO] Activating camera for destination: {bed_id}")
    frame = capture_image()

    print("[INFO] Running YOLO detection...")
    detected_label, confidence = detect_medicine(frame)

    prescribed_label = get_prescribed_medicine(bed_id)

    match = (detected_label is not None and
             prescribed_label is not None and
             detected_label.lower() == prescribed_label.lower())

    print(f"[RESULT] Detected: {detected_label} ({confidence:.2f}) | "
          f"Prescribed: {prescribed_label} | Match: {match}")

    send_verification_result(bed_id, detected_label, prescribed_label, match)
    return match


if __name__ == "__main__":
    # Example usage — in the full system this is triggered when the
    # robot reaches a destination (signaled by the ESP32 over serial/Bluetooth)
    destination_bed = "BED1"
    result = verify_medicine_for_bed(destination_bed)

    if result:
        print("[INFO] Medicine verified. Safe to dispense.")
    else:
        print("[WARNING] Medicine mismatch or detection failed. Dispensing blocked.")
