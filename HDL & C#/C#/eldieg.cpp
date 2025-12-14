// ======================================================================
// ?? E-NOSE FINAL CODE (FSM + SERIAL INTERFACE)
// Arduino UNO R4 WiFi
// ======================================================================

// --- PIN SENSOR ANALOG MQ ---
const int SENSOR_PIN_MQ4 = A0; // Sensor 1
const int SENSOR_PIN_MQ135 = A1; // Sensor 2
const int SENSOR_PIN_MQ6 = A2; // Sensor 3
const int SENSOR_PIN_MQ7 = A3; // Sensor 4

// ==================== MOTOR PINS ====================
// Pin Motor Driver (Intake & Exhaust)
const int PWM_A = 10;  // Kecepatan Motor A (Intake)
const int DIR_A1 = 12;
const int DIR_A2 = 13;
const int PWM_B = 11;  // Kecepatan Motor B (Exhaust)
const int DIR_B1 = 8;
const int DIR_B2 = 9;

// ==================== FSM & TIMING ====================
enum State { IDLE, PRE_COND, RAMP_UP, HOLD, PURGE, RECOVERY, DONE };
State currentState = IDLE;
unsigned long stateTime = 0; // Waktu state dimulai
int currentLevel = 0;        // Level 0 sampai 4 (Total 5 Level)
bool isRunning = false;      // Status FSM berjalan

// Durasi FSM (dalam milidetik)
const unsigned long T_PRECOND = 15000; // 15 detik
const unsigned long T_RAMP = 2000;  // 2 detik
const unsigned long T_HOLD = 8000;  // 8 detik
const unsigned long T_PURGE = 15000; // 15 detik
const unsigned long T_RECOVERY = 5000;  // 5 detik

// Kecepatan Motor A saat HOLD (20% - 100% dari 255)
const int SPEEDS[5] = { 51, 102, 153, 204, 255 };
int rampSpeed = 0;

// --- KONTROL PENGIRIMAN DATA ---
unsigned long lastDataSend = 0;
const int SEND_DELAY = 100; // Kirim data setiap 100ms (10Hz)

// ====================================================================
// ?? FUNGSI MOTOR DRIVER
// ====================================================================
void motorA(int speed, bool reverse = false) {
    if (speed > 255) speed = 255;
    if (speed < 0) speed = 0;

    if (!reverse) {
        digitalWrite(DIR_A1, HIGH); digitalWrite(DIR_A2, LOW);
    }
    else {
        digitalWrite(DIR_A1, LOW); digitalWrite(DIR_A2, HIGH);
    }
    analogWrite(PWM_A, speed);
}

void motorB(int speed, bool reverse = false) {
    if (speed > 255) speed = 255;
    if (speed < 0) speed = 0;

    if (!reverse) {
        digitalWrite(DIR_B1, HIGH); digitalWrite(DIR_B2, LOW);
    }
    else {
        digitalWrite(DIR_B1, LOW); digitalWrite(DIR_B2, HIGH);
    }
    analogWrite(PWM_B, speed);
}

void stopMotors() {
    analogWrite(PWM_A, 0);
    analogWrite(PWM_B, 0);
}

// ====================================================================
// ?? FUNGSI FSM (LOGIKA ALUR)
// ====================================================================
void changeState(State newState) {
    currentState = newState;
    stateStartTime = millis();
}

void startProcess() {
    isRunning = true;
    currentLevel = 0;
    changeState(PRE_COND);
    Serial.println("MSG: Memulai Siklus Sampling...");
}

void stopProcess() {
    isRunning = false;
    changeState(IDLE);
    stopMotors();
    Serial.println("MSG: Stop Paksa. Kembali ke IDLE.");
}

void rampTo(int target) {
    // Naikkan kecepatan perlahan
    if (rampSpeed < target) {
        rampSpeed += 5; // Naikkan 5 step per loop (sesuaikan agar halus)
        if (rampSpeed > target) rampSpeed = target;
    }
    else {
        rampSpeed = target;
    }
    motorA(rampSpeed);
}

void runFSM() {
    unsigned long elapsed = millis() - stateStartTime;

    switch (currentState) {
    case PRE_COND:
        // Fase Pemanasan: Motor A 40%, Motor B Mati
        motorA(102); motorB(0);
        if (elapsed >= T_PRECOND) changeState(RAMP_UP);
        break;

    case RAMP_UP:
        // Fase Naik Kecepatan
        rampTo(SPEEDS[currentLevel]);
        if (elapsed >= T_RAMP) changeState(HOLD);
        break;

    case HOLD:
        // Fase Pengambilan Data (Stabil)
        motorA(SPEEDS[currentLevel]); motorB(0);
        if (elapsed >= T_HOLD) changeState(PURGE);
        break;

    case PURGE:
        // Fase Pembersihan: Motor A Reverse 100%, Motor B Forward 100%
        motorA(255, true); motorB(255);
        if (elapsed >= T_PURGE) changeState(RECOVERY);
        break;

    case RECOVERY:
        // Fase Istirahat: Semua Motor Mati
        stopMotors();
        if (elapsed >= T_RECOVERY) {
            currentLevel++;
            if (currentLevel >= 5) {
                stopProcess(); // Selesai semua level
                Serial.println("MSG: 5 Siklus Selesai.");
            }
            else {
                // Lanjut ke level berikutnya
                rampSpeed = SPEEDS[currentLevel - 1];
                changeState(RAMP_UP);
            }
        }
        break;

    case IDLE:
        stopMotors();
        break;

    case DONE:
        stopMotors();
        break;
    }
}

// ====================================================================
// ?? KALIBRASI & PENGIRIMAN DATA
// ====================================================================

// ?? FUNGSI KALIBRASI (PLACEHOLDER)
float get_calibrated_value(int raw_adc) {
    // Nilai ADC mentah dikonversi menjadi rentang visual yang bagus.
    float voltage = (float)raw_adc * (5.0 / 1023.0);
    return voltage * 50.0;
}

void kirimDataPython(float mq4_val, float mq135_val, float mq6_val, float mq7_val) {
    // Format Protokol: DATA,val1,val2,val3,val4
    Serial.print("DATA,");
    Serial.print(mq4_val, 2);
    Serial.print(",");
    Serial.print(mq135_val, 2);
    Serial.print(",");
    Serial.print(mq6_val, 2);
    Serial.print(",");
    Serial.println(mq7_val, 2);
}

// ====================================================================
// ?? SETUP (DIJALANKAN SEKALI)
// ====================================================================
void setup() {
    Serial.begin(115200); // Harus sama dengan Python

    // Setup Pin Motor
    pinMode(DIR_A1, OUTPUT); pinMode(DIR_A2, OUTPUT); pinMode(PWM_A, OUTPUT);
    pinMode(DIR_B1, OUTPUT); pinMode(DIR_B2, OUTPUT); pinMode(PWM_B, OUTPUT);
    stopMotors();

    // Setup Pin Sensor
    pinMode(SENSOR_PIN_MQ4, INPUT);
    pinMode(SENSOR_PIN_MQ135, INPUT);
    pinMode(SENSOR_PIN_MQ6, INPUT);
    pinMode(SENSOR_PIN_MQ7, INPUT);

    delay(1000);
    Serial.println("ARDUINO READY. Menunggu perintah START_SAMPLING...");
}

// ====================================================================
// ?? LOOP (DIJALANKAN BERULANG KALI)
// ====================================================================
void loop() {

    // 1. Cek Perintah dari Python
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "START_SAMPLING") {
            startProcess();
        }
        else if (cmd == "STOP_SAMPLING") {
            stopProcess();
        }
    }

    // 2. Jalankan Mesin FSM (Kontrol Motor)
    if (isRunning) {
        runFSM();
    }

    // 3. Kirim Data Sensor (Hanya saat fase HOLD)
    if (currentState == HOLD) {
        if (millis() - lastDataSend >= SEND_DELAY) {
            lastDataSend = millis();

            // Baca sensor
            float val_mq4 = get_calibrated_value(analogRead(SENSOR_PIN_MQ4));
            float val_mq135 = get_calibrated_value(analogRead(SENSOR_PIN_MQ135));
            float val_mq6 = get_calibrated_value(analogRead(SENSOR_PIN_MQ6));
            float val_mq7 = get_calibrated_value(analogRead(SENSOR_PIN_MQ7));

            // Kirim ke Python
            kirimDataPython(val_mq4, val_mq135, val_mq6, val_mq7);
        }
    }
}