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
const int SPEAKER_PIN = 12;

// ── 설정값 ──
const int FACE_THRESHOLD = 3000;
const int MOTOR_SPEED    = 110;
const int MOTOR_TIME     = 1500;
const int COOL_DOWN      = 2000;

// ── 날씨 상태 (블루투스로 갱신) ──
bool rainExpected = false;
unsigned long lastWeatherUpdate = 0;
const unsigned long WEATHER_TIMEOUT = 6UL * 60UL * 60UL * 1000UL; // 6시간

// 얼굴 감지 → 앱 TTS 트리거 쿨다운
unsigned long lastFaceBT = 0;
const unsigned long FACE_TTS_COOLDOWN = 15000; // 15초

String btBuf = "";

// ── 함수 선언 ──
void runMotor();
void stopMotor();
void playDoReMi();
void handleBluetooth();

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);
  Wire.begin();

  pinMode(IR_PIN, INPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);

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
  // 1) 블루투스 수신
  handleBluetooth();

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
    playDoReMi();
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

// ── 블루투스 수신 처리 ──
void handleBluetooth() {
  while (btSerial.available()) {
    char c = btSerial.read();
    if (c == '\n' || c == '\r') {
      btBuf.trim();
      if (btBuf.length() > 0) {
        Serial.print("[BT 수신] "); Serial.println(btBuf);
        if (btBuf.startsWith("RAIN:")) {
          int v = btBuf.substring(5).toInt();
          rainExpected = (v == 1);
          lastWeatherUpdate = millis();
          Serial.print("   → 비 예보: ");
          Serial.println(rainExpected ? "옴" : "안 옴");
          btSerial.print("ACK RAIN=");
          btSerial.println(rainExpected ? "1" : "0");
        } else if (btBuf == "PING") {
          btSerial.println("PONG");
        }
      }
      btBuf = "";
    } else {
      btBuf += c;
      if (btBuf.length() > 48) btBuf = ""; // 오버플로 방지
    }
  }

  // 너무 오래된 정보는 무효화
  if (rainExpected && lastWeatherUpdate > 0 && (millis() - lastWeatherUpdate) > WEATHER_TIMEOUT) {
    Serial.println("[경고] 6시간 이상 갱신 없음 — 비 예보 상태 초기화");
    rainExpected = false;
    lastWeatherUpdate = 0;
  }
}

// ── 도레미파솔라시도 ──
void playDoReMi() {
  int notes[]    = {262, 294, 330, 349, 392, 440, 494, 523};
  int duration[] = {200, 200, 200, 200, 200, 200, 200, 400};
  for (int i = 0; i < 8; i++) {
    tone(SPEAKER_PIN, notes[i], duration[i]);
    delay((int)(duration[i] * 1.2));
    noTone(SPEAKER_PIN);
  }
  Serial.println("   [스피커] 도레미파솔라시도 완료");
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
