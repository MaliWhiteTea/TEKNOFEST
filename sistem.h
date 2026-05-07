#include "WString.h"
#include "HardwareSerial.h"
#include <stdint.h>
#include "TrafficMonitor.h"
#include <SoftwareSerial.h>

#ifndef SISTEM_H
#define SISTEM_H

SoftwareSerial grup2(7,5); //rx4 tx5

String gelenveri = "";
int an = 0;
int targetAn = -1;   
bool specialDelayActive = false;
unsigned long specialDelayEnd = 0;
int delayedTarget = -1;   
int isikBeklemeSuresi = 3000;
bool doguHandled = false;
int setIsikBeklemeSuresi = 5000;
bool emergencyResumePending = false;
int normalResumeAn = -1;
int emergencyExitAn = -1;


inline void veriGonder(uint16_t veri) {

  uint16_t veriFixed = veri;
  veriFixed ^= 0b0000000000000011; 

  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLOCK, MSBFIRST, highByte(veriFixed));
  shiftOut(DATA, CLOCK, MSBFIRST, lowByte(veriFixed));
  digitalWrite(LATCH, HIGH);
}
struct Bitler {
  char key[5];   // "a1k" gibi 4 karakter + '\0'
  char value[6]; // "0" veya "83" gibi
};
const Bitler tablo[] PROGMEM = {
  //kuzey
  {"KS", "2345"}, //kuzey sarı 
  {"KY", "2353"}, //kuzey yeşil
  {"KKS", "2349"}, //kuzey kırmızı sarı
  //doğu
  {"DS", "2373"}, //doğu sarı
  {"DY", "2437"}, //doğu yeşil
  {"DKS", "2405"}, //doğu kırmızı sarı
  //güney
  {"GS", "2597"}, //güney sarı
  {"GY", "3109"}, //güney yeşil
  {"GKS", "2853"}, //güney kırmızı sarı
  //batı
  {"BS", "4389"}, //batı sarı
  {"BY", "8485"}, //batı yeşil
  {"BKS", "6437"}, //batı kırmızı sarı
  //yayalar
  {"YB", "2340"}, //yayalar boş
  {"YY", "2342"}, //yayalar yeşil
  //kırmızı
  {"K", "2341"},
  //debug
  {"ALL", "16383"},
  {"BOS", "0"}
};
const int tabloSize = sizeof(tablo) / sizeof(tablo[0]);
inline uint16_t veri(const char* aranan) {
  Bitler kayit;
  for (int i = 0; i < tabloSize; i++) {
    memcpy_P(&kayit, &tablo[i], sizeof(Bitler));
    if (strcmp(kayit.key, aranan) == 0) {
      return (uint16_t)strtoul(kayit.value, NULL, 10); // string → uint16_t
    }
  }
  return 0; // bulunamadıysa 0 döner
}
int basladi = 0;

inline bool yesilAsamaMi(int durum) {
  return (durum == 3 || durum == 7 || durum == 11 || durum == 15);
}

inline int yesildenSariya(int yesilAsama) {
  switch (yesilAsama) {
    case 3: return 4;
    case 7: return 8;
    case 11: return 12;
    case 15: return 16;
    default: return -1;
  }
}

inline int yesildenSonrakiAkis(int yesilAsama) {
  switch (yesilAsama) {
    case 3: return 6;
    case 7: return 10;
    case 11: return 14;
    case 15: return 2;
    default: return -1;
  }
}

inline int yesileGirisAsamasi(int yesilAsama) {
  switch (yesilAsama) {
    case 3: return 2;
    case 7: return 6;
    case 11: return 10;
    case 15: return 14;
    default: return -1;
  }
}

inline int mesajaGoreAcilYesil(const String& mesaj) {
  // Mesaj prefix -> fiziksel yonun yesil asamasi
  if (mesaj.startsWith("G2-R1->")) return 3;  // Kuzey
  if (mesaj.startsWith("G2-R2->")) return 7;  // Dogu
  if (mesaj.startsWith("R1->")) return 11;    // Guney
  if (mesaj.startsWith("R2->")) return 15;    // Bati
  return -1;
}

inline void acilDurumUygula(int hedefYesilAn) {
  if (hedefYesilAn < 0) return;

  if (!emergencyResumePending) {
    normalResumeAn = yesilAsamaMi(an) ? yesildenSonrakiAkis(an) : an;
    emergencyResumePending = true;
  }

  emergencyExitAn = yesildenSonrakiAkis(hedefYesilAn);
  isikBeklemeSuresi = setIsikBeklemeSuresi;

  if (yesilAsamaMi(an)) {
    targetAn = yesildenSariya(an);
    delayedTarget = yesileGirisAsamasi(hedefYesilAn);
    specialDelayActive = true;
    specialDelayEnd = millis() + 2000;
  } else {
    targetAn = yesileGirisAsamasi(hedefYesilAn);
    delayedTarget = -1;
    specialDelayActive = false;
  }
}

inline bool acilDurumMesajiIsle(const String& mesaj) {
  int hedefYesil = mesajaGoreAcilYesil(mesaj);
  if (hedefYesil < 0) return false;

  Serial.println("ACIL DURUM ALGILANDI!");
  acilDurumUygula(hedefYesil);
  return true;
}



// Bekle fonksiyonun
void bekle(unsigned long sure) {
  unsigned long basla = millis();
  while (millis() - basla < sure) {
    if (grup2.available()) {
      char c = grup2.read();
      Serial.print(c);
      gelenveri += c;

      if (c == '\n') {
        gelenveri.trim(); //
        if (acilDurumMesajiIsle(gelenveri)) {
          gelenveri = "";
          continue;
        }
        
        //İtfaiye opsiyonlar F3:E3:47:14
        if (gelenveri == "G2-R2->F3:E3:47:14") {
          

          

          Serial.println("İTFAİYE ALGILANDI!");
          switch (an) {
            case 1: 
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 2:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 4;
              veriGonder(veri("KS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 4:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 5:
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 6:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              targetAn = 8;
              veriGonder(veri("DS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 8:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9: 
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 13; isikBeklemeSuresi = 3000; break;
            case 11:
              targetAn = 4;
              veriGonder(veri("GS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 12:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13:
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 15:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 16:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
          }


            

          }
        
        if (gelenveri == "G2-R1->F3:E3:47:14") {
          Serial.println("İTFAİYE ALGILANDI!");
          switch (an) {
            case 1: 
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 2:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 4;
              veriGonder(veri("KS"));                        // önce 4'e atla
              delayedTarget = 9;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 4:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 5:
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 6:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              targetAn = 8;
              veriGonder(veri("DS"));                        // önce 4'e atla
              delayedTarget = 9;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 8:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13: 
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 9; isikBeklemeSuresi = 3000; break;
            case 15:
              targetAn = 4;
              veriGonder(veri("BS"));                        // önce 4'e atla
              delayedTarget = 9;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 16:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9:
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 11; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 11:
              targetAn = 11; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 12:
              targetAn = 11; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
          }                              
        }     
        if (gelenveri == "R1->f3:e3:47:14") {
          Serial.println("İTFAİYE ALGILANDI!");
          switch (an) {
            case 1: 
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;             
            case 2:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 4;
              veriGonder(veri("KS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 4:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9:
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 11:
              targetAn = 6;
              veriGonder(veri("GS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 12:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13: 
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 5; isikBeklemeSuresi = 3000; break;
            case 15:
              targetAn = 4;
              veriGonder(veri("BS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 16:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 5:
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 6:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 8:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;                          
          }                                 
        }           
        if (gelenveri == "R2->f3:e3:47:14") {
          Serial.println("İTFAİYE ALGILANDI!");          
          switch (an) {
            case 5: 
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
              
            case 6:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              
              veriGonder(veri("DS"));                        // önce 4'e atla
              delayedTarget = 1;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 8:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9:
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 11:
              
              veriGonder(veri("GS"));                        // önce 4'e atla
              delayedTarget = 1;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 12:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13: 
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 1; isikBeklemeSuresi = 3000; break;
            case 15:
              
              veriGonder(veri("BS"));                        
              delayedTarget = 1;                 
              specialDelayActive = true;          
              specialDelayEnd = millis() + 2000;   
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 16:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 1:
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 2:
              targetAn = 3; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 3; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 4:
              targetAn = 3; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
            

          }
          
          
             
        }               
        //

        //polis opsiyonlar 83:B0:33:14
        if (gelenveri == "G2-R2->83:B0:33:14") {
          Serial.println("POLİS ALGILANDI!");
          
          switch (an) {
            case 1: 
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
              
            case 2:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 4;
              veriGonder(veri("KS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 4:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 5:
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 6:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              targetAn = 8;
              veriGonder(veri("DS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 8:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9: 
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 13; isikBeklemeSuresi = 3000; break;
            case 11:
              targetAn = 4;
              veriGonder(veri("GS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 12:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13:
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 15:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 16:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
            

          }
          
          
             // örnek: doğrudan an=7’ye atla (Doğu Yeşil)
        }
        if (gelenveri == "G2-R1->83:B0:33:14") {
          Serial.println("POLİS ALGILANDI!");
          
          switch (an) {
            case 1: 
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
              
            case 2:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 4;
              veriGonder(veri("KS"));                        // önce 4'e atla
              delayedTarget = 9;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 4:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 5:
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 6:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              targetAn = 8;
              veriGonder(veri("DS"));                        // önce 4'e atla
              delayedTarget = 9;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 8:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13: 
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 9; isikBeklemeSuresi = 3000; break;
            case 15:
              targetAn = 4;
              veriGonder(veri("BS"));                        // önce 4'e atla
              delayedTarget = 9;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 16:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9:
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 11; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 11:
              targetAn = 11; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 12:
              targetAn = 11; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
            

          }
          
          
            
        }     
        if (gelenveri == "R1->83:b0:33:14") {
          Serial.println("POLİS ALGILANDI!");
          
          switch (an) {
            case 1: 
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
              
            case 2:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 4;
              veriGonder(veri("KS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 4:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9:
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 11:
              targetAn = 6;
              veriGonder(veri("GS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 12:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13: 
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 5; isikBeklemeSuresi = 3000; break;
            case 15:
              targetAn = 4;
              veriGonder(veri("BS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 16:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 5:
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 6:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 8:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
            

          }
          
          
             
        }           
        if (gelenveri == "R2->83:b0:33:14") {
          Serial.println("POLİS ALGILANDI!");
          
          switch (an) {
            case 5: 
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
              
            case 6:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              
              veriGonder(veri("DS"));                        // önce 4'e atla
              delayedTarget = 1;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 8:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9:
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 11:
              
              veriGonder(veri("GS"));                        // önce 4'e atla
              delayedTarget = 1;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 12:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13: 
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 1; isikBeklemeSuresi = 3000; break;
            case 15:
              
              veriGonder(veri("BS"));                        
              delayedTarget = 1;                 
              specialDelayActive = true;          
              specialDelayEnd = millis() + 2000;   
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 16:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 1:
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 2:
              targetAn = 3; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 3; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 4:
              targetAn = 3; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
            

          }
          
          
             
        } 
        //

        //ambulans opsiyonlar 23:8D:74:13
        if (gelenveri == "G2-R2->23:8D:74:13") {
          Serial.println("AMBULANS ALGILANDI!");
          
          switch (an) {
            case 1: 
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
              
            case 2:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 4;
              veriGonder(veri("KS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 4:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 5:
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 6:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              targetAn = 8;
              veriGonder(veri("DS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 8:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9: 
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 13; isikBeklemeSuresi = 3000; break;
            case 11:
              targetAn = 4;
              veriGonder(veri("GS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 12:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13:
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 15:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 16:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
            

          }
          
          
             // örnek: doğrudan an=7’ye atla (Doğu Yeşil)
        }
        if (gelenveri == "G2-R1->23:8D:74:13") {
          Serial.println("AMBULANS ALGILANDI!");
          
          switch (an) {
            case 1: 
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
              
            case 2:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 4;
              veriGonder(veri("KS"));                        // önce 4'e atla
              delayedTarget = 9;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 4:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 5:
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 6:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              targetAn = 8;
              veriGonder(veri("DS"));                        // önce 4'e atla
              delayedTarget = 9;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 8:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13: 
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 9; isikBeklemeSuresi = 3000; break;
            case 15:
              targetAn = 4;
              veriGonder(veri("BS"));                        // önce 4'e atla
              delayedTarget = 9;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 16:
              targetAn = 9; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9:
              targetAn = 10; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 11; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 11:
              targetAn = 11; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 12:
              targetAn = 11; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
            

          }
          
          
            
        }     
        if (gelenveri == "R1->23:8d:74:13") {
          Serial.println("AMBULANS ALGILANDI!");
          
          switch (an) {
            case 1: 
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
              
            case 2:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 4;
              veriGonder(veri("KS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 4:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9:
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 11:
              targetAn = 6;
              veriGonder(veri("GS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 12:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13: 
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 5; isikBeklemeSuresi = 3000; break;
            case 15:
              targetAn = 4;
              veriGonder(veri("BS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 16:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 5:
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 6:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 8:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
            

          }
          
          
             
        }           
        if (gelenveri == "R2->23:8d:74:13") {
          Serial.println("AMBULANS ALGILANDI!");
          
          switch (an) {
            case 5: 
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
              
            case 6:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              
              veriGonder(veri("DS"));                        // önce 4'e atla
              delayedTarget = 1;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 8:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9:
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 11:
              
              veriGonder(veri("GS"));                        // önce 4'e atla
              delayedTarget = 1;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 12:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13: 
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 1; isikBeklemeSuresi = 3000; break;
            case 15:
              
              veriGonder(veri("BS"));                        
              delayedTarget = 1;                 
              specialDelayActive = true;          
              specialDelayEnd = millis() + 2000;   
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 16:
              targetAn = 1; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 1:
              targetAn = 2; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 2:
              targetAn = 3; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 3; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 4:
              targetAn = 3; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
            

          }
          
          
             
        } 
        //

        gelenveri = "";
      }
    }
    
    if (getYogunluk() == "dogu") {
      
      
      
      switch (an) {
            case 1: 
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;             
            case 2:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 4;
              veriGonder(veri("KS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 4:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9:
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 11:
              targetAn = 6;
              veriGonder(veri("GS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 12:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13: 
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 5; isikBeklemeSuresi = 3000; break;
            case 15:
              targetAn = 4;
              veriGonder(veri("BS"));                        // önce 4'e atla
              delayedTarget = 5;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 16:
              targetAn = 5; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 5:
              targetAn = 6; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 6:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 8:
              targetAn = 7; isikBeklemeSuresi = setIsikBeklemeSuresi; break;                          
          }
      ;
        
      
      
    

    
    }
    if (getYogunluk() == "bati") {

          
          switch (an) {
            case 1: 
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
              
            case 2:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 3:
              targetAn = 4;
              veriGonder(veri("KS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000; 
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 4:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 5:
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 6:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 7:
              targetAn = 8;
              veriGonder(veri("DS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;              
            case 8:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 9: 
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 10:
              targetAn = 13; isikBeklemeSuresi = 3000; break;
            case 11:
              targetAn = 4;
              veriGonder(veri("GS"));                        // önce 4'e atla
              delayedTarget = 13;                  // sonra 13'e gidecek
              specialDelayActive = true;           // özel beklemeyi başlat
              specialDelayEnd = millis() + 2000;   // 2 saniye sonra tetiklenecek
              isikBeklemeSuresi = setIsikBeklemeSuresi; 
              break;
            case 12:
              targetAn = 13; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 13:
              targetAn = 14; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 14:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;
            case 15:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;          
            case 16:
              targetAn = 15; isikBeklemeSuresi = setIsikBeklemeSuresi; break;              
            

          }
          
          
             // örnek: doğrudan an=7’ye atla (Doğu Yeşil)
        }
    }
  }






inline void mode0(unsigned long delayTime = 200) {
  

  
  an = 1;  
  while (1) {
    switch (an) {
      case 1:
        veriGonder(veri("K"));
        Serial.println("Kırmızı");
        bekle(3000);
        an = 2;
        break;

      case 2:
        veriGonder(veri("KKS"));
        Serial.println("Kuzey Kırmızı Sarı");
        bekle(3000);
        an = 3;
        break;

      case 3:
        veriGonder(veri("KY"));
        Serial.println("Kuzey Yeşil");
        bekle(isikBeklemeSuresi);
        isikBeklemeSuresi = 3000;
        an = 4;
        break;

      case 4:
        veriGonder(veri("KS"));
        Serial.println("Kuzey Sarı");
        bekle(500);
        an = 5;
        break;

      case 5:
        veriGonder(veri("K"));
        Serial.println("Kırmızı");
        bekle(3000);
        an = 6;
        break;

      case 6:
        veriGonder(veri("DKS"));
        Serial.println("Doğu Kırmızı Sarı");
        bekle(3000);
        an = 7;
        break;

      case 7:
        veriGonder(veri("DY"));
        Serial.println("Doğu Yeşil");
        bekle(isikBeklemeSuresi);
        isikBeklemeSuresi = 3000;
        an = 8;
        break;

      case 8:
        veriGonder(veri("DS"));
        Serial.println("Doğu Sarı");
        bekle(500);
        an = 9;
        break;

      case 9:
        veriGonder(veri("K"));
        Serial.println("Kırmızı");
        bekle(3000);
        an = 10;
        break;

      case 10:
        veriGonder(veri("GKS"));
        Serial.println("Güney Kırmızı Sarı");
        bekle(3000);
        an = 11;
        break;

      case 11:
        veriGonder(veri("GY"));
        Serial.println("Güney Yeşil");
        bekle(isikBeklemeSuresi);
        isikBeklemeSuresi = 3000;
        an = 12;
        break;

      case 12:
        veriGonder(veri("GS"));
        Serial.println("Güney Sarı");
        bekle(500);
        an = 13;
        break;

      case 13:
        veriGonder(veri("K"));
        Serial.println("Kırmızı");
        bekle(3000);
        an = 14;
        break;

      case 14:
        veriGonder(veri("BKS"));
        Serial.println("Batı Kırmızı Sarı");
        bekle(3000);
        an = 15;
        break;

      case 15:
        veriGonder(veri("BY"));
        Serial.println("Batı Yeşil");
        bekle(isikBeklemeSuresi);
        isikBeklemeSuresi = 3000;
        an = 16;
        break;

      case 16:
        veriGonder(veri("BS"));
        Serial.println("Batı Sarı");
        bekle(500);
        an = 1;  
        break;
    }
 
        if (specialDelayActive && millis() >= specialDelayEnd) {
          targetAn = delayedTarget;     
          delayedTarget = -1;           
          specialDelayActive = false;    
        }

       
        if (targetAn > 0) {
          an = targetAn;
          targetAn = -1;
          continue;   
        }

        if (emergencyResumePending && emergencyExitAn > 0 && an == emergencyExitAn) {
          an = normalResumeAn;
          emergencyResumePending = false;
          normalResumeAn = -1;
          emergencyExitAn = -1;
          continue;
        }

  }
}





#endif
