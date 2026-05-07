#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>

// 1. RC522 Okuyucu Pin Tanımlamaları
#define RST_1_PIN 7
#define SS_1_PIN  8

// 2. RC522 Okuyucu Pin Tanımlamaları
#define RST_2_PIN 9
#define SS_2_PIN  10

MFRC522 rfid1(SS_1_PIN, RST_1_PIN);
MFRC522 rfid2(SS_2_PIN, RST_2_PIN);

// Nano 1'den veri okumak için (RX: 2, TX: 3)
SoftwareSerial nano1Serial(6, 2);
// Uno'ya veri göndermek için (RX: 4, TX: 5)
SoftwareSerial unoSerial(4, 3); 

// Araç ID'leri
String itfaiyeID  = "F3:E3:47:14"; 
String polisID    = "83:B0:33:14"; 
String ambulansID = "23:8D:74:13";

void setup() {
  Serial.begin(9600);
  
  nano1Serial.begin(9600);
  unoSerial.begin(9600);
  
  // İki farklı SoftwareSerial portu olduğundan, okuma yapacağımız portu dinlemeye alıyoruz
  nano1Serial.listen(); 

  SPI.begin();
  rfid1.PCD_Init();
  rfid2.PCD_Init();
  
  Serial.println("Nano 2 Basladi. Nano 1 ve Sensorler dinleniyor...");
}

void loop() {
  // 1. Görev: Nano 1'den gelen veriyi kontrol et ve dogrudan Uno'ya aktar
  if (nano1Serial.available()) {
    String gelenVeri = nano1Serial.readStringUntil('\n');
    gelenVeri.trim(); 
    if (gelenVeri.length() > 0) {
      unoSerial.println(gelenVeri);
      Serial.println("Nano 1'den aktarildi: " + gelenVeri);
    }
  }

  // 2. Görev: Nano 2'nin kendi 1. Okuyucusunu kontrol et (D7-D8 bağlı olan)
  if (rfid1.PICC_IsNewCardPresent() && rfid1.PICC_ReadCardSerial()) {
    String uid = getUID(rfid1.uid.uidByte, rfid1.uid.size);
    if (uid.equalsIgnoreCase(itfaiyeID) || uid.equalsIgnoreCase(polisID) || uid.equalsIgnoreCase(ambulansID)) {
      String gonderilecek = "G2-R1->" + uid;
      
      // Uno G2 serisi verilerde harfleri BÜYÜK harf bekliyor
      gonderilecek.toUpperCase(); 
      
      unoSerial.println(gonderilecek);
      Serial.println("Gonderildi: " + gonderilecek);
      
      // Gönderimden sonra dinlemeyi tekrar nano1Serial'e devret
      nano1Serial.listen(); 
    }
    rfid1.PICC_HaltA();
  }

  // 3. Görev: Nano 2'nin kendi 2. Okuyucusunu kontrol et (D9-D10 bağlı olan)
  if (rfid2.PICC_IsNewCardPresent() && rfid2.PICC_ReadCardSerial()) {
    String uid = getUID(rfid2.uid.uidByte, rfid2.uid.size);
    if (uid.equalsIgnoreCase(itfaiyeID) || uid.equalsIgnoreCase(polisID) || uid.equalsIgnoreCase(ambulansID)) {
      String gonderilecek = "G2-R2->" + uid;
      
      gonderilecek.toUpperCase();
      
      unoSerial.println(gonderilecek);
      Serial.println("Gonderildi: " + gonderilecek);
      
      nano1Serial.listen();
    }
    rfid2.PICC_HaltA();
  }
}

String getUID(byte *buffer, byte bufferSize) {
  String uidStr = "";
  for (byte i = 0; i < bufferSize; i++) {
    uidStr += String(buffer[i] < 0x10 ? "0" : "");
    uidStr += String(buffer[i], HEX);
    if (i < bufferSize - 1) {
      uidStr += ":";
    }
  }
  return uidStr;
}