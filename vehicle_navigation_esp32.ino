/*
  MEDIBOT - Robotic Vehicle Navigation Module
  ---------------------------------------------
  Controller: ESP32
  Function: Line-following navigation using IR sensors, obstacle detection
            using ultrasonic sensor, destination identification using RFID,
            and Bluetooth-based manual/autonomous control.

  Based on Robotic Vehicle Algorithm (Navigation and Delivery) documented
  in the MEDIBOT project report, Section 3.2.1.
*/

#include <BluetoothSerial.h>
#include <SPI.h>
#include <MFRC522.h>   // RFID module
#include <ArduinoJson.h>

BluetoothSerial SerialBT;

// ---------- Pin Definitions ----------
// IR sensors (line following)
#define IR_LEFT_PIN   34
#define IR_RIGHT_PIN  35

// Ultrasonic sensor (obstacle detection)
#define TRIG_PIN      5
#define ECHO_PIN      18

// L298N Motor Driver
#define LEFT_MOTOR_FWD   25
#define LEFT_MOTOR_BWD   26
#define RIGHT_MOTOR_FWD  27
#define RIGHT_MOTOR_BWD  14
#define ENA              32   // Left motor enable (PWM)
#define ENB              33   // Right motor enable (PWM)

// RFID (SPI)
#define RFID_SS_PIN   21
#define RFID_RST_PIN  22
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

// Status LED
#define STATUS_LED    2

// ---------- Parameters ----------
const int OBSTACLE_THRESHOLD_CM = 15;   // Stop if obstacle closer than this
const int BASE_SPEED = 150;             // PWM speed (0-255)

// ---------- State ----------
bool autoMode = false;
bool bluetoothConnected = false;
String targetDestination = "";          // e.g. "BED1"
String detectedTag = "";

void setup() {
  Serial.begin(115200);
  SerialBT.begin("MEDIBOT");

  pinMode(IR_LEFT_PIN, INPUT);
  pinMode(IR_RIGHT_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(LEFT_MOTOR_FWD, OUTPUT);
  pinMode(LEFT_MOTOR_BWD, OUTPUT);
  pinMode(RIGHT_MOTOR_FWD, OUTPUT);
  pinMode(RIGHT_MOTOR_BWD, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(STATUS_LED, OUTPUT);

  SPI.begin();
  rfid.PCD_Init();

  Serial.println("MEDIBOT Vehicle Module Initialized");
}

void loop() {
  checkBluetoothStatus();
  handleIncomingCommands();

  if (autoMode) {
    runAutonomousNavigation();
  }

  delay(20);
}

// ---------- Bluetooth Status ----------
void checkBluetoothStatus() {
  bluetoothConnected = SerialBT.hasClient();
  digitalWrite(STATUS_LED, bluetoothConnected ? !digitalRead(STATUS_LED) : HIGH);
}

// ---------- Command Handling ----------
// Commands: M (toggle mode), A (start/stop auto), G:<dest> (set destination),
// F/B/L/R/S (manual movement)
void handleIncomingCommands() {
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();

    if (cmd == "M") {
      autoMode = !autoMode;
      sendStatusJSON("mode_toggle");
    }
    else if (cmd == "A") {
      autoMode = !autoMode;
      sendStatusJSON("auto_toggle");
    }
    else if (cmd.startsWith("G:")) {
      targetDestination = cmd.substring(2);
      sendStatusJSON("destination_set");
    }
    else if (!autoMode) {
      // Manual mode movement commands
      if (cmd == "F") moveForward();
      else if (cmd == "B") moveBackward();
      else if (cmd == "L") turnLeft();
      else if (cmd == "R") turnRight();
      else if (cmd == "S") stopMotors();

      // Manual mode still checks obstacles
      if (getDistanceCM() < OBSTACLE_THRESHOLD_CM) {
        stopMotors();
        sendStatusJSON("obstacle_stop");
      }
    }
  }
}

// ---------- Autonomous Navigation (line following + RFID) ----------
void runAutonomousNavigation() {
  // Obstacle check first — highest priority
  int distance = getDistanceCM();
  if (distance < OBSTACLE_THRESHOLD_CM) {
    stopMotors();
    sendStatusJSON("obstacle_detected");
    return;
  }

  // Read IR sensors for line following
  int leftIR = digitalRead(IR_LEFT_PIN);
  int rightIR = digitalRead(IR_RIGHT_PIN);

  if (leftIR == LOW && rightIR == LOW) {
    moveForward();
  } else if (leftIR == LOW && rightIR == HIGH) {
    turnLeft();
  } else if (leftIR == HIGH && rightIR == LOW) {
    turnRight();
  }
  // both HIGH -> off track, could add recovery logic here

  // Check RFID for destination match
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    detectedTag = getTagID();
    if (detectedTag == targetDestination) {
      stopMotors();
      sendStatusJSON("destination_reached");
      triggerRoboticArm();
    }
    rfid.PICC_HaltA();
  }
}

String getTagID() {
  String tag = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    tag += String(rfid.uid.uidByte[i], HEX);
  }
  return tag;
}

// ---------- Ultrasonic Distance ----------
int getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 999; // no echo received
  return duration * 0.0343 / 2;
}

// ---------- Motor Control ----------
void moveForward() {
  analogWrite(ENA, BASE_SPEED);
  analogWrite(ENB, BASE_SPEED);
  digitalWrite(LEFT_MOTOR_FWD, HIGH);
  digitalWrite(LEFT_MOTOR_BWD, LOW);
  digitalWrite(RIGHT_MOTOR_FWD, HIGH);
  digitalWrite(RIGHT_MOTOR_BWD, LOW);
}

void moveBackward() {
  analogWrite(ENA, BASE_SPEED);
  analogWrite(ENB, BASE_SPEED);
  digitalWrite(LEFT_MOTOR_FWD, LOW);
  digitalWrite(LEFT_MOTOR_BWD, HIGH);
  digitalWrite(RIGHT_MOTOR_FWD, LOW);
  digitalWrite(RIGHT_MOTOR_BWD, HIGH);
}

void turnLeft() {
  analogWrite(ENA, BASE_SPEED);
  analogWrite(ENB, BASE_SPEED);
  digitalWrite(LEFT_MOTOR_FWD, LOW);
  digitalWrite(LEFT_MOTOR_BWD, HIGH);
  digitalWrite(RIGHT_MOTOR_FWD, HIGH);
  digitalWrite(RIGHT_MOTOR_BWD, LOW);
}

void turnRight() {
  analogWrite(ENA, BASE_SPEED);
  analogWrite(ENB, BASE_SPEED);
  digitalWrite(LEFT_MOTOR_FWD, HIGH);
  digitalWrite(LEFT_MOTOR_BWD, LOW);
  digitalWrite(RIGHT_MOTOR_FWD, LOW);
  digitalWrite(RIGHT_MOTOR_BWD, HIGH);
}

void stopMotors() {
  digitalWrite(LEFT_MOTOR_FWD, LOW);
  digitalWrite(LEFT_MOTOR_BWD, LOW);
  digitalWrite(RIGHT_MOTOR_FWD, LOW);
  digitalWrite(RIGHT_MOTOR_BWD, LOW);
}

// ---------- Communication ----------
void triggerRoboticArm() {
  // Sends a signal over Serial2 (UART) to the second ESP32 controlling the arm
  Serial2.println("DISPENSE");
}

void sendStatusJSON(String eventType) {
  StaticJsonDocument<200> doc;
  doc["event"] = eventType;
  doc["mode"] = autoMode ? "auto" : "manual";
  doc["destination"] = targetDestination;
  doc["bluetooth"] = bluetoothConnected;

  String output;
  serializeJson(doc, output);
  SerialBT.println(output);
}
