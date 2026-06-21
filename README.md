# AI-Powered Autonomous Medicine Delivery Robot (MEDIBOT)

An autonomous hospital robot that navigates to a patient's bed, verifies the correct medicine using AI-based computer vision, and dispenses it using a robotic arm — designed to reduce nurse workload for routine medicine delivery.

Final-year B.Tech project, Department of ECE, Government College of Engineering Kannur (GCEK).

## 🎯 Problem Statement
Hospital staff spend significant time on repetitive medicine delivery rounds. MEDIBOT automates navigation, medicine verification, and dispensing — reducing manual workload while adding an AI-based safety check to ensure the correct medicine reaches the correct patient.

## 🛠️ Tech Stack
| Layer | Technology |
|---|---|
| Navigation Controller | ESP32 |
| AI / Vision | Raspberry Pi 4B, Pi Camera, YOLO (Python) |
| Cloud / Database | Firebase Realtime Database |
| Mobile App | Android (Bluetooth control) |
| Sensors | IR sensors (line-following), HC-SR04 Ultrasonic (obstacle detection), RFID (destination ID) |
| Actuation | L298N Motor Driver, PCA9685 Servo Driver, ULN driver (suction pump) |

## ⚙️ System Architecture

[Mobile App] <--Bluetooth--> [ESP32: Vehicle Navigation]
                                     |
                          Line-following (IR) + Obstacle
                          detection (Ultrasonic) + RFID
                          destination matching
                                     |
                              (Serial/UART link)
                                     |
                         [ESP32: Robotic Arm + Pump]
                                     |
                    [Raspberry Pi: YOLO Vision Module]
                                     |
                          [Firebase Realtime DB]
                       (prescription data, verification
                              results, system status)

## 📦 Modules

### 1. Vehicle Navigation (`vehicle_navigation_esp32.ino`)
- Line-following navigation using dual IR sensors
- Real-time obstacle detection via ultrasonic sensor — halts movement if an obstacle is closer than the safety threshold
- RFID-based destination detection (matches scanned tag against the target bed ID)
- Manual override mode via Bluetooth (F/B/L/R/S commands) alongside fully autonomous mode
- Sends live status updates (mode, destination, obstacle events) as JSON over Bluetooth

### 2. AI Vision — Medicine Verification (`ai_vision_module.py`)
- Captures an image via Pi Camera upon reaching the destination
- Runs a YOLO object detection model to identify the medicine in frame
- Cross-checks the detected medicine against the prescribed medicine fetched from Firebase
- Pushes the verification result (match/mismatch) back to Firebase for the mobile app to display

### 3. Robotic Arm Control (`robotic_arm_esp32.ino`)
- Drives a 3-DOF arm (base, middle joint, top joint) via a PCA9685 servo driver
- Controls a suction pump (via ULN driver) to pick up and release medicine
- Supports manual joint control for calibration, plus a programmable sequence storage/playback system for repeatable dispensing motions
- Triggered automatically by the vehicle module upon reaching the correct destination

## 📐 Algorithm Summary
1. Robot initializes all modules (ESP32, Bluetooth, IR, RFID, ultrasonic, motor driver)
2. Navigates autonomously using line-following, continuously checking for obstacles
3. On RFID tag match with target destination, vehicle stops and signals the arm module
4. Raspberry Pi captures an image and runs YOLO to verify the medicine
5. If verified, the robotic arm executes the pick-and-place sequence to dispense the medicine
6. Status updates are sent throughout via Bluetooth/JSON, with prescription and verification data synced through Firebase

## 📊 Results
- Built and demonstrated a working physical prototype integrating navigation, vision, and robotic arm subsystems
- Successfully identified multiple medicine labels using the YOLO-based vision module
- Functional end-to-end pipeline: navigation → destination detection → AI verification → dispensing

## ⚠️ Note on Code
This repository contains **representative implementations** of each module, written from the documented system design, algorithms, and circuit diagrams in the original project report. The original on-device source files (used during physical testing) are being recovered from project hardware; this code reflects the same logic, architecture, and component interfacing as the working build.

## 🔭 Future Scope
- Add GPS/indoor positioning for multi-room hospital navigation
- Live delivery tracking via the mobile/web app
- Smart battery management and auto-charging docking station
- Enhanced gripper with sensor feedback for delicate medicine handling

## 👥 Team
Built as a team project at GCEK, Department of Electronics and Communication Engineering.
