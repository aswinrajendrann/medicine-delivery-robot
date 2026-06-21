/*
  MEDIBOT - Robotic Arm Module (Medicine Handling)
  ---------------------------------------------------
  Controller: ESP32
  Function: Controls a multi-DOF robotic arm using a PCA9685 servo driver,
            operates a suction pump (via ULN driver) for picking/releasing
            medicine, and supports both manual control and stored/replayed
            movement sequences.

  Based on Robotic Arm Algorithm (Medicine Handling) documented in the
  MEDIBOT project report, Section 3.2.3.

  Library required: Adafruit_PWMServoDriver
*/

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <EEPROM.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// ---------- Servo Channel Assignments (PCA9685) ----------
#define BASE_SERVO_CH    0
#define MIDDLE_SERVO_CH  1
#define TOP_SERVO_CH     2

// ---------- Pump Control (via ULN driver) ----------
#define PUMP_PIN  16

// ---------- Servo Pulse Range ----------
#define SERVO_MIN  150   // pulse length for 0 degrees
#define SERVO_MAX  600   // pulse length for 180 degrees

// ---------- Sequence Storage ----------
#define MAX_STEPS 20
struct ArmStep {
  int baseAngle;
  int middleAngle;
  int topAngle;
  bool pumpOn;
};
ArmStep sequence[MAX_STEPS];
int stepCount = 0;

// ---------- Current Position ----------
int currentBase = 90;
int currentMiddle = 90;
int currentTop = 90;

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200);  // Communication with vehicle ESP32

  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(60);

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  // Move to initial/home position
  setServoAngle(BASE_SERVO_CH, currentBase);
  setServoAngle(MIDDLE_SERVO_CH, currentMiddle);
  setServoAngle(TOP_SERVO_CH, currentTop);

  Serial.println("MEDIBOT Robotic Arm Module Initialized");
}

void loop() {
  // Listen for trigger from vehicle module (reached destination)
  if (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    msg.trim();
    if (msg == "DISPENSE") {
      dispenseMedicine();
    }
  }

  // Manual control via Serial (for testing/calibration)
  if (Serial.available()) {
    char cmd = Serial.read();
    handleManualCommand(cmd);
  }
}

// ---------- Manual Control ----------
// B = base movement, M = middle joint, T = top joint, P/p = pump on/off
void handleManualCommand(char cmd) {
  switch (cmd) {
    case 'B':
      currentBase = constrain(currentBase + 10, 0, 180);
      setServoAngle(BASE_SERVO_CH, currentBase);
      break;
    case 'M':
      currentMiddle = constrain(currentMiddle + 10, 0, 180);
      setServoAngle(MIDDLE_SERVO_CH, currentMiddle);
      break;
    case 'T':
      currentTop = constrain(currentTop + 10, 0, 180);
      setServoAngle(TOP_SERVO_CH, currentTop);
      break;
    case 'P':
      digitalWrite(PUMP_PIN, HIGH);
      break;
    case 'p':
      digitalWrite(PUMP_PIN, LOW);
      break;
  }
}

// ---------- Sequence Storage & Execution ----------
void storeCurrentStep(bool pumpState) {
  if (stepCount < MAX_STEPS) {
    sequence[stepCount].baseAngle = currentBase;
    sequence[stepCount].middleAngle = currentMiddle;
    sequence[stepCount].topAngle = currentTop;
    sequence[stepCount].pumpOn = pumpState;
    stepCount++;
  }
}

void executeSequence() {
  for (int i = 0; i < stepCount; i++) {
    setServoAngle(BASE_SERVO_CH, sequence[i].baseAngle);
    setServoAngle(MIDDLE_SERVO_CH, sequence[i].middleAngle);
    setServoAngle(TOP_SERVO_CH, sequence[i].topAngle);
    digitalWrite(PUMP_PIN, sequence[i].pumpOn ? HIGH : LOW);
    delay(500); // fixed delay between steps
  }
}

// ---------- Medicine Dispensing Routine ----------
void dispenseMedicine() {
  Serial.println("[ARM] Dispensing sequence started");

  // Position arm over medicine
  setServoAngle(BASE_SERVO_CH, 45);
  setServoAngle(MIDDLE_SERVO_CH, 60);
  setServoAngle(TOP_SERVO_CH, 90);
  delay(500);

  // Activate pump to pick up medicine
  digitalWrite(PUMP_PIN, HIGH);
  delay(500);

  // Move to delivery position
  setServoAngle(BASE_SERVO_CH, 135);
  setServoAngle(MIDDLE_SERVO_CH, 90);
  setServoAngle(TOP_SERVO_CH, 60);
  delay(500);

  // Release medicine
  digitalWrite(PUMP_PIN, LOW);
  delay(300);

  // Return to home position
  setServoAngle(BASE_SERVO_CH, 90);
  setServoAngle(MIDDLE_SERVO_CH, 90);
  setServoAngle(TOP_SERVO_CH, 90);

  Serial.println("[ARM] Dispensing complete");
  Serial2.println("ARM_DONE");
}

// ---------- Servo Helper ----------
void setServoAngle(uint8_t channel, int angle) {
  int pulse = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
  pwm.setPWM(channel, 0, pulse);
}
