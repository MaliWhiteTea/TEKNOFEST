#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>

// 1. ve 2. RC522 Okuyucu Pin Tanımlamaları
#define SS_1_PIN 10
#define SS_2_PIN 8
#define RST_PIN  9

MFRC522 rfid1(SS_1_PIN, RST_PIN);
MFRC522 rfid2(SS_2_PIN, RST_PIN);

// Nano 2'ye veri göndermek için (RX: 2 kullanılmıyor, TX: 3)
SoftwareSerial nano2Serial(2, 3); 

// --- BURAYA KENDİ RFID KART ID'LERİNİZİ YAZIN ---
String itfaiyeID  = "F3:E3:47:14"; // Örnek: "f3:e3:47:14"
String polisID    = "83:B0:33:14"; 
String ambulansID = "23:8D:74:13"; 
// ------------------------------------------------

void setup() {
  Serial.begin(9600);
  nano2Serial.begin(9600);
  
  SPI.begin();
  rfid1.PCD_Init();
  rfid2.PCD_Init();
  
  Serial.println("Nano 1 Basladi. Kartlar bekleniyor...");
}

void loop() {
  // 1. Okuyucu Kontrolü
  if (rfid1.PICC_IsNewCardPresent() && rfid1.PICC_ReadCardSerial()) {
    String uid = getUID(rfid1.uid.uidByte, rfid1.uid.size);
    if (uid.equalsIgnoreCase(itfaiyeID) || uid.equalsIgnoreCase(polisID) || uid.equalsIgnoreCase(ambulansID)) {
      String gonderilecek = "R1->" + uid;
      nano2Serial.println(gonderilecek);
      Serial.println("Gonderildi: " + gonderilecek);
    }
    rfid1.PICC_HaltA();
  }

  // 2. Okuyucu Kontrolü
  if (rfid2.PICC_IsNewCardPresent() && rfid2.PICC_ReadCardSerial()) {
    String uid = getUID(rfid2.uid.uidByte, rfid2.uid.size);
    if (uid.equalsIgnoreCase(itfaiyeID) || uid.equalsIgnoreCase(polisID) || uid.equalsIgnoreCase(ambulansID)) {
      String gonderilecek = "R2->" + uid;
      nano2Serial.println(gonderilecek);
      Serial.println("Gonderildi: " + gonderilecek);
    }
    rfid2.PICC_HaltA();
  }
}

String getUID(byte *buffer, byte bufferSize) {
  String uidStr = "";
  for (byte i = 0; i < bufferSize; i++) {
    uidStr += String(buffer[i] < 0x10 ? "0" : "");
    uidStr += String(buffer[i], HEX); // HEX dönüşümü standart olarak küçük harf verir
    if (i < bufferSize - 1) {
      uidStr += ":";
    }
  }
  return uidStr; 
}