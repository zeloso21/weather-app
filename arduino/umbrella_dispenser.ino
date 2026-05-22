#include "HUSKYLENS.h"
#include "Wire.h"

// Arduino Mega 2560 기준 핀 매핑
//   I2C  : SDA=20, SCL=21 (HUSKYLENS — Wire가 자동 사용)
//   HC-05: TX1=18 → HC-05 RXD (전압분주 1kΩ+2kΩ로 5V→3.3V 강하 필수)
//          RX1=19 ← HC-05 TXD (직결 OK)
// Uno에서 옮긴 경우 차이점:
//   - SoftwareSerial 제거 → 하드웨어 Serial1 사용 (더 안정적)
//   - HUSKYLENS 배선: A4/A5 → D20/D21

HUSKYLENS huskylens;
#define btSerial Serial1

// ── 핀 설정 ──
const int IR_PIN      = 2;
const int IN1         = 5;
const int IN2         = 6;
const int IN3         = 7;
const int IN4         = 8;
const int ENA         = 4;
const int ENB         = 9;

// ── 설정값 ──
const int FACE_THRESHOLD = 3000;
const int MOTOR_SPEED    = 255;
const int MOTOR_TIME     = 1500;
const int COOL_DOWN      = 2000;

// ── 날씨 상태 (블루투스로 갱신) ──
bool rainExpected = false;
unsigned long lastWeatherUpdate = 0;
const unsigned long WEATHER_TIMEOUT = 6UL * 60UL * 60UL * 1000UL; // 6시간

// 얼굴 감지 → 앱 TTS 트리거 쿨다운
unsigned long lastFaceBT = 0;
const unsigned long FACE_TTS_COOLDOWN = 15000; // 15초

String btBuf = "";   // BT(Serial1) 입력 버퍼
String usbBuf = "";  // USB(Serial) 입력 버퍼

// ── 함수 선언 ──
void runMotor();
void stopMotor();
void handleSerialInput();
void processLine(const String& line, Stream& replyTo, const char* src);

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);
  Wire.begin();

  pinMode(IR_PIN, INPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);

  stopMotor();

  Serial.println("허스키렌즈 연결 중...");
  while (!huskylens.begin(Wire)) {
    Serial.println("연결 실패, 재시도...");
    delay(1000);
  }
  Serial.println("연결 성공!");

  huskylens.writeAlgorithm(ALGORITHM_FACE_RECOGNITION);
  delay(500);
  Serial.println("대기 중... (BT 'RAIN:1' / 'RAIN:0' 수신 대기)");
}

void loop() {
  // 1) BT(HC-05) + USB(Web Serial) 둘 다에서 명령 수신
  handleSerialInput();

  // 2) 센서 입력
  bool irDetected = (digitalRead(IR_PIN) == HIGH);
  bool faceDetected = false;
  int faceArea = 0;

  if (huskylens.request() && huskylens.countBlocks() > 0) {
    HUSKYLENSResult result = huskylens.getBlock(0);
    faceArea = result.width * result.height;
    if (faceArea >= FACE_THRESHOLD) {
      faceDetected = true;
    }
  }

  // 3) 디버그 출력
  Serial.print("[비예보] "); Serial.print(rainExpected ? "O" : "X");
  Serial.print("  [IR] ");   Serial.print(irDetected   ? "감지O" : "감지X");
  Serial.print("  [얼굴] "); Serial.print(faceDetected ? "감지O" : "감지X");
  Serial.print("  [크기] "); Serial.println(faceArea);

  // 4) 얼굴 감지 → 앱이 TTS 멘트 재생하도록 신호 (쿨다운으로 스팸 방지)
  if (faceDetected && (millis() - lastFaceBT) > FACE_TTS_COOLDOWN) {
    btSerial.println("FACE");
    lastFaceBT = millis();
    Serial.println(">> [BT 송신] FACE — 앱이 멘트 재생");
  }

  // 5) 비 + 우산 + 외출(얼굴) 동시 감지 → 우산 내보내기
  if (rainExpected && irDetected && faceDetected) {
    Serial.println(">> ★ 비 + 우산 + 외출 감지! 우산 내보냅니다.");
    btSerial.println("EVENT umbrella_dispensed");
    runMotor();
    delay(MOTOR_TIME);
    stopMotor();
    Serial.println(">> 완료. 대기 중...");
    delay(COOL_DOWN);
  } else {
    stopMotor();
  }

  delay(300);
}

// ── 한 줄 처리 (RAIN:1 / RAIN:0 / PING) ──
void processLine(const String& raw, Stream& replyTo, const char* src) {
  String line = raw;
  line.trim();
  if (line.length() == 0) return;
  Serial.print("["); Serial.print(src); Serial.print(" 수신] "); Serial.println(line);
  if (line.startsWith("RAIN:")) {
    int v = line.substring(5).toInt();
    rainExpected = (v == 1);
    lastWeatherUpdate = millis();
    Serial.print("   → 비 예보: ");
    Serial.println(rainExpected ? "옴" : "안 옴");
    replyTo.print("ACK RAIN=");
    replyTo.println(rainExpected ? "1" : "0");
  } else if (line == "PING") {
    replyTo.println("PONG");
  }
}

// ── 시리얼 수신 처리 (BT + USB 둘 다) ──
void handleSerialInput() {
  // BT (HC-05)
  while (btSerial.available()) {
    char c = btSerial.read();
    if (c == '\n' || c == '\r') {
      processLine(btBuf, btSerial, "BT");
      btBuf = "";
    } else {
      btBuf += c;
      if (btBuf.length() > 48) btBuf = "";
    }
  }
  // USB (Web Serial로 PC 크롬이 직접 전송)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      processLine(usbBuf, Serial, "USB");
      usbBuf = "";
    } else {
      usbBuf += c;
      if (usbBuf.length() > 48) usbBuf = "";
    }
  }

  // 너무 오래된 정보는 무효화
  if (rainExpected && lastWeatherUpdate > 0 && (millis() - lastWeatherUpdate) > WEATHER_TIMEOUT) {
    Serial.println("[경고] 6시간 이상 갱신 없음 — 비 예보 상태 초기화");
    rainExpected = false;
    lastWeatherUpdate = 0;
  }
}

// ── 모터 회전 ──
void runMotor() {
  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}

// ── 모터 정지 ──
void stopMotor() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}
