#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// Chân nút bấm
#define BTN_1 2
#define BTN_2 3
#define BTN_3 4
#define BTN_ADMIN_VIEW 5   // Nút R (Xem/Quay lại)
#define BTN_ADMIN_RESET 6  // Nút L (Reset/Xác nhận)

#define SS_PIN 10
#define RST_PIN 9
#define BUZZER 8

#define ADDR_VOTER_COUNT 5    
#define ADDR_VOTER_LIST 10    

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

byte adminUID[] = {0x56, 0x40, 0x11, 0x05}; 

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  lcd.init();
  lcd.backlight();
  
  pinMode(BUZZER, OUTPUT);
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
  pinMode(BTN_ADMIN_VIEW, INPUT_PULLUP);
  pinMode(BTN_ADMIN_RESET, INPUT_PULLUP);

  lcd.print("HE THONG EVM");
  delay(2000);
  lcd.clear();
  lcd.print("MOI QUET THE");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW);

  if (isAdmin(mfrc522.uid.uidByte)) {
    handleAdminMenu();
  } else {
    if (checkAlreadyVoted(mfrc522.uid.uidByte)) {
      lcd.clear(); lcd.print("THE NAY DA BAU");
      lcd.setCursor(0, 1); lcd.print("KHONG BAU LAI");
      delay(3000);
    } else {
      handleVoter();
    }
  }
  lcd.clear();
  lcd.print("MOI QUET THE");
  mfrc522.PICC_HaltA();
}

void handleAdminMenu() {
  while (true) { 
    lcd.clear();
    lcd.print("ADMIN MODE");
    lcd.setCursor(0, 1);
    lcd.print("L:RESET R:RESULT");

    unsigned long startTime = millis();
    bool backToMenu = false;

    while (millis() - startTime < 6000) {
      // 1. NHẤN XEM KẾT QUẢ
      if (digitalRead(BTN_ADMIN_VIEW) == LOW) {
        delay(300); 
        if (showResults()) { 
           backToMenu = true; 
           break; 
        } else {
           return; 
        }
      }
      
      // 2. NHẤN RESET (Đã sửa logic ở đây)
      if (digitalRead(BTN_ADMIN_RESET) == LOW) {
        delay(300);
        if (confirmReset()) {
           return; // Nếu đã Reset thành công thì thoát hẳn ra màn hình quẹt thẻ
        } else {
           backToMenu = true; // Nếu nhấn Hủy (Cancel) thì quay lại Menu Admin
           break;
        }
      }
    }
    
    if (!backToMenu) return; 
  }
}

// Sửa kiểu dữ liệu trả về thành bool để báo trạng thái Reset cho Menu
bool confirmReset() {
  lcd.clear();
  lcd.print("CONFIRM RESET");
  lcd.setCursor(0, 1);
  lcd.print("L:OK R:CANCEL");
  delay(500);

  unsigned long resetStart = millis();
  while (millis() - resetStart < 6000) {
    if (digitalRead(BTN_ADMIN_RESET) == LOW) { // Nhấn OK
      for(int i=0; i<3; i++) EEPROM.write(i, 0);
      EEPROM.write(ADDR_VOTER_COUNT, 0);
      lcd.clear(); lcd.print("DA RESET ALL");
      digitalWrite(BUZZER, HIGH); delay(1000); digitalWrite(BUZZER, LOW);
      delay(2000);
      return true; // Trả về true báo hiệu đã Reset xong
    }
    if (digitalRead(BTN_ADMIN_VIEW) == LOW) { // Nhấn CANCEL
      lcd.clear(); lcd.print("DA HUY RESET");
      delay(2000);
      return false; // Trả về false báo hiệu đã Hủy
    }
  }
  return false; // Hết thời gian cũng coi như Hủy
}

bool showResults() {
  int v1 = EEPROM.read(0);
  int v2 = EEPROM.read(1);
  int v3 = EEPROM.read(2);
  int total = EEPROM.read(ADDR_VOTER_COUNT);

  lcd.clear();
  lcd.print("A:"); lcd.print(v1);
  lcd.print(" B:"); lcd.print(v2);
  lcd.print(" C:"); lcd.print(v3);
  lcd.setCursor(0, 1);
  lcd.print("BACK --> VIEW");

  unsigned long viewStart = millis();
  while (millis() - viewStart < 6000) {
    if (digitalRead(BTN_ADMIN_VIEW) == LOW) {
      digitalWrite(BUZZER, HIGH); delay(50); digitalWrite(BUZZER, LOW);
      delay(300);
      return true; 
    }
  }
  return false; 
}

// --- CÁC HÀM CÒN LẠI GIỮ NGUYÊN ---
bool isAdmin(byte *uid) {
  for (byte i = 0; i < 4; i++) { if (uid[i] != adminUID[i]) return false; }
  return true;
}

bool checkAlreadyVoted(byte *uid) {
  int totalVoters = EEPROM.read(ADDR_VOTER_COUNT);
  for (int i = 0; i < totalVoters; i++) {
    bool match = true;
    for (int j = 0; j < 4; j++) {
      if (EEPROM.read(ADDR_VOTER_LIST + (i * 4) + j) != uid[j]) { match = false; break; }
    }
    if (match) return true;
  }
  return false;
}

void saveVoterUID(byte *uid) {
  int totalVoters = EEPROM.read(ADDR_VOTER_COUNT);
  for (int j = 0; j < 4; j++) EEPROM.write(ADDR_VOTER_LIST + (totalVoters * 4) + j, uid[j]);
  EEPROM.write(ADDR_VOTER_COUNT, totalVoters + 1);
}

void handleVoter() {
  bool finalConfirmed = false;

  while (!finalConfirmed) {
    lcd.clear();
    lcd.print("MOI CHON (A/B/C)");
    
    int firstChoice = -1;
    String candidateName = "";

    // Bước 1: Đợi cử tri chọn lần đầu (Không giới hạn thời gian)
    while (firstChoice == -1) {
      if (digitalRead(BTN_1) == LOW) { firstChoice = BTN_1; candidateName = "A"; }
      else if (digitalRead(BTN_2) == LOW) { firstChoice = BTN_2; candidateName = "B"; }
      else if (digitalRead(BTN_3) == LOW) { firstChoice = BTN_3; candidateName = "C"; }
    }
    
    digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW);
    delay(500); // Chống dội phím (Debounce)

    // Bước 2: Hiển thị màn hình xác nhận
    lcd.clear();
    lcd.print("XN BAU CHO "); lcd.print(candidateName); lcd.print("?");
    lcd.setCursor(0, 1);
    lcd.print(candidateName); lcd.print(":OK - KHAC:HUY");

    // Bước 3: Đợi xác nhận hoặc hủy
    bool actionTaken = false;
    while (!actionTaken) {
      // Nếu nhấn lại đúng nút vừa chọn -> XÁC NHẬN
      if (digitalRead(firstChoice) == LOW) {
        int addr = (firstChoice == BTN_1) ? 0 : (firstChoice == BTN_2 ? 1 : 2);
        recordVote(addr, "PHUONG AN " + candidateName);
        saveVoterUID(mfrc522.uid.uidByte);
        finalConfirmed = true;
        actionTaken = true;
      } 
      // Nếu nhấn bất kỳ nút nào khác (trong số các nút bầu cử) -> HỦY
      else if ((digitalRead(BTN_1) == LOW || digitalRead(BTN_2) == LOW || digitalRead(BTN_3) == LOW || 
                digitalRead(BTN_ADMIN_VIEW) == LOW || digitalRead(BTN_ADMIN_RESET) == LOW)) {
        lcd.clear();
        lcd.print("DA HUY CHON!");
        digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW);
        delay(1500);
        actionTaken = true; // Thoát vòng lặp chờ xác nhận, quay lại màn hình chọn A/B/C
      }
    }
  }
}

void recordVote(int addr, String name) {
  int count = EEPROM.read(addr);
  EEPROM.write(addr, count + 1);
  digitalWrite(BUZZER, HIGH); delay(500); digitalWrite(BUZZER, LOW);
  lcd.clear(); lcd.print("DA BAU: "); lcd.setCursor(0, 1); lcd.print(name); delay(3000);
}