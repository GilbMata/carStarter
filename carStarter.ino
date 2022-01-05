//Carstart by GilMata
#include <Button.h>
#include <EEPROM.h>
// NFC librerias
#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>
#include "pitches.h" 
#include "themes.h" 
//Bluetooth-
//#include <SoftwareSerial.h>
//SoftwareSerial BT(11, 12); // Bluetooth RX-TX

Button startB(2);     //Boton.
// int startB = 2;  //Start button.
byte fbrakeB = 10;   // Freno
byte accR = 9;       // Accesorio relay.
byte ignR = 8;       // Inicio relay.
byte startR = 7;     // Ignicion relay.
byte buzz = 5;       // Buzzer.
byte led = 6;        // Led indicator.
//byte byPassPin = 11;
bool byPass = EEPROM[50];
int RPM_sens = 3;
int RPM;
String btdata;
int estado = 0;
long unsigned ignTime = 0;
long unsigned ignTimeMax = 6000; //Aproximadamente 4.5 seg
unsigned long stnbTimePrev = 0;
long unsigned stnbTime = 60000;
unsigned long tbpi = 0;
unsigned long accTime = 0;
// NFC
PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);
const uint8_t uidKnownCount = 4; 
const uint8_t uidKnownLength = 4;
/*const uint8_t uidsKnown[uidKnownCount][uidKnownLength]
{
  0x34, 0x9A, 0x23, 0xD9,
  0xD2, 0x45, 0x7B, 0x25,
  0x01, 0x02, 0x03, 0x04
};*/
// Match found?
boolean success = false;
unsigned long timeInit = 0;
unsigned long timeActive = 30000;
byte nTimes = 0;
unsigned long waittime = 3000;
unsigned long entrytime = millis();

void buzzer(int n, int d = 300, int t = 300){
  for (int i = 0; i < n; ++i){
    tone(buzz, 1500, t);
    delay(d);
  } 
}

void setup(){
  EEPROM[40] = 63;  EEPROM[41] = 191;  EEPROM[42] = 186;  EEPROM[43] = 41;
  Serial.begin(115200);
  // BT.begin(9600);
  startB.begin();
  pinMode(fbrakeB, INPUT);
  pinMode(accR, OUTPUT);
  pinMode(startR, OUTPUT);
  pinMode(ignR, OUTPUT);
  pinMode(buzz, OUTPUT);
  pinMode(led, OUTPUT);
  // pinMode(byPassPin, INPUT);
  pinMode(RPM_sens, INPUT);
  digitalWrite(accR, LOW);
  digitalWrite(startR, LOW);
  digitalWrite(ignR, LOW);
  Serial.print(F("\n"));
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  for(byte i=0; i<16; i++){
    //EEPROM[i] = 255;
    Serial.print(EEPROM[i]);
    Serial.print('\t');
  }
  if(!versiondata){
      Serial.println(F("No se encontro lector PN53x"));
      buzzer(5,500);
      while(1){
          digitalWrite(led, HIGH);
          delay(1000);
          digitalWrite(led, LOW);
          delay(1000);
      }
  }else{
      Serial.print(F("Chip NFC PN5"));
      Serial.println((versiondata >> 24) & 0xFF, HEX);
      Serial.print(F("Firmware ver. "));
      Serial.print((versiondata >> 16) & 0xFF, DEC);
      Serial.print('.');
      Serial.println((versiondata >> 8) & 0xFF, DEC);
      nfc.setPassiveActivationRetries(0xFF);
      nfc.SAMConfig();
      Serial.println(F("Esperando llave ISO14443A"));
      Serial.print(F("\n"));
      for(int i=0; i<3; i++){
        digitalWrite(led, HIGH);
        buzzer(1);
        digitalWrite(led, LOW);
        delay(50);
      }
      //Play_MarioUW();
  }
  if (byPass){
    digitalWrite(led, HIGH);
    debug("ByPass");
  }
  rpms();
  //leeKeys();
}

void loop(){
  // DataBT();
  switch (estado) {
    case 0:     //apagado
      if (nfcValid()){
        digitalWrite(led, HIGH);
        timeInit = millis();
        do{
          if(startB.pressed()){
            debug("Start Precionado!");
            if(!encendido()){
              carStart();
              break;
            }
          }
        }while ((millis() < timeInit + timeActive));
        if(!estado && !byPass){
          debug("Tiempo Terminado!");
          digitalWrite(led, LOW);
          buzzer(2,250);
        }
      }
      if(startB.pressed() && byPass){
        debug("Start Precionado!");
        if(!encendido()){
          carStart();
        }
      }
      if (byPass){
        if(digitalRead(fbrakeB)){ //revisar pulso
          digitalWrite(led, HIGH);
        }else{
          digitalWrite(led, LOW);
        }
      }
      break;

    case 1:   //accessorio
      if(startB.pressed()){
        debug("Start Precionado en Accessorio");
        carStart();
      }else{
        digitalWrite(led, !digitalRead(led));
        delay(250);
      }
      if (millis() > accTime + 900000){
        playSmb_under(buzz);
        accTime = millis();
      }
      break;

    case 2:   //encendido
      if (startB.read() == Button::PRESSED){
        long unsigned time = millis();
        bool stop = true;
        while(startB.read() == Button::PRESSED){
          if (millis() > time + 2000){
            stop = false;
            break;
          } 
        }
        if (stop){
          buzzer(1,150);
          carStop();
        }else{
          debug("Marcha ON");
          debug("--------------");
          do{    
            digitalWrite(startR, HIGH); // Ignition relay ON
            if ((millis() > time + ignTime)){
              break;
            }
          }while(startB.read() == Button::PRESSED);
          digitalWrite(startR, LOW);
          debug("Marcha OFF");
        }
        delay(500);
      }
      break;

    case 3:   //standby
      if(startB.pressed()){
        debug("Start Precionado en Standby!");
        if(encendido() == false){
          carStart();
        }
        break;
      }
      if(stnbTimePrev + stnbTime < millis()){
        estado = 0;
        digitalWrite(led, LOW);
        playDeath(buzz);
        debug("Stanby Fuera");
      }

      break;
  }
  
}

void accessory(int st){
  if (st){
    debug(F("Accesorio ON"));
    digitalWrite(accR, HIGH);
    estado = 1;
    accTime = millis();
  }else{
    debug(F("Accesorio OFF"));
    digitalWrite(accR, LOW);
    estado = 0;
  }
}

void carStart(){
  digitalWrite(ignR, HIGH); // Start relay ON
  debug("Ignicion ON");
  delay(50);
  if(digitalRead(fbrakeB) == HIGH){
    accessory(false);
    debug("Freno Precionado!");
    debug("Marcha ON");
    debug("--------------");
    ignTime = millis();
    do{    
      digitalWrite(startR, HIGH); // Ignition relay ON
      if ((millis() > ignTime + ignTimeMax)){
        break;
      }
    }while ((millis() < ignTime + 900) || startB.read() == Button::PRESSED);
    digitalWrite(startR, LOW);
    debug("Marcha OFF");
    digitalWrite(led, HIGH);
    accessory(true);
    estado = 2;
  }else{
    debug("Freno No Precionado!");
    digitalWrite(ignR, LOW);
    debug("Ignicion Off");
    if (estado == 1){
      carStop();
    }else if(estado == 0 || estado == 3){      
      accessory(true);
    } 
  }
  delay(1000);
}

void carStop(){
  digitalWrite(startR, LOW);
  digitalWrite(ignR, LOW);
  digitalWrite(accR, LOW);
  digitalWrite(led, HIGH);
  stnbTimePrev = millis();
  debug(F("Sistema OFF"));
  if (byPass){
    estado = 0;
    digitalWrite(led, LOW);
  }else{
    estado = 3;
  }
}

int encendido(){
  if (digitalRead(ignR) == HIGH){
    return true;
  }else{
    return false;
  }
}

bool nfcValid(){
  success = false;
  uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer para guardar UID 
  uint8_t uidLength;                     // Tamaño del  UID (4 o 7 bytes dependiendo del tipo de tarjeta ISO14443A)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, & uid[0], & uidLength);
  if (success){
    Serial.println(F("Tarjeta encontrada!"));
    Serial.print(F("UID Tamaño: "));
    Serial.print(uidLength, DEC);
    Serial.println(F(" bytes"));
    Serial.print(F("UID Valor: "));
    nfc.PrintHex(uid, uidLength);
    Serial.print(F("\n"));
    Serial.println(uid[0]); Serial.println(uid[1]); Serial.println(uid[2]); Serial.println(uid[3]);
    Serial.println(EEPROM[40]); Serial.println(EEPROM[41]); Serial.println(EEPROM[42]); Serial.println(EEPROM[43]);
    Serial.print(F("\n"));
    delay(500);
    long unsigned readTime;
    unsigned long readLimit = 5000;
    //if (uidKnownLength == uidLength){
      //for (byte it = 0; it < uidKnownCount; it++){
    if (EEPROM[40] == uid[0] && EEPROM[41] == uid[1] && EEPROM[42] == uid[2] && EEPROM[43] == uid[3]){
      registraKeys();
    }else{
      for (byte it = 0; it <= (uidKnownCount*uidKnownLength)-uidKnownLength; it+=4){
        // Valida si el uid esta en el arreglo
        if(EEPROM[it] == uid[0] && EEPROM[it+1] == uid[1] && EEPROM[it+2] == uid[2] && EEPROM[it+3] == uid[3]  ){
        //if (uidsKnown[it][0] == uid[0] && uidsKnown[it][1] == uid[1] && uidsKnown[it][2] == uid[2] && uidsKnown[it][3] == uid[3]){
          debug("Tarjeta Valida!");
          //buzzer(1);
          Play_MarioUW(1);
          delay(500);
          Play_MarioUW(2);
          if (byPass){
            byPass = false;
            debug("byPass Off!");
            EEPROM[50] = 0;
          }else{
            readTime = millis();
            do{
              uid[0] = 0;
              success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, & uid[0], & uidLength);
              if (millis() > readTime + readLimit){
                  if (!byPass){
                    byPass = true;
                    debug("byPass On!");
                    EEPROM[50] = 1;
                    buzzer(3,150);
                    delay(2000);
                    break;
                  }
              }
              delay(10);
            }while (EEPROM[it] == uid[0] && EEPROM[it+1] == uid[1] && EEPROM[it+2] == uid[2] && EEPROM[it+3] == uid[3]);  
          }
          return true;
        }
      }
    }
    debug("Tarjeta Invalida!");
    //}
  }
  return false;
}

void debug(String str){
  uint32_t now = millis();
  //Serial.printf("%07u.%03u: %s\n", now / 1000, now % 1000, str.c_str());
  Serial.print(now / 1000);
  Serial.print(".");
  Serial.print(now % 1000);
  Serial.print(": ");
  Serial.println(str.c_str());
  //Serial << atoi(now / 1000) << now % 1000 << ": ";
  //Serial.println(str);
  //BT.println(message);
}

void registraKeys(){
  buzzer(4);
  int dirmem = 0;
  int numKey = 1;
  debug("Entrando en modo de registro de llave");
  uint32_t t = millis();
  while(millis() - t  < 30000 && numKey <= 4){
    digitalWrite(led, HIGH);
    debug("Presentar llave para registrar ");
    debug(String(numKey));
    success = false;
    uint8_t uid[] = {0, 0, 0, 0}; // Buffer to store the returned UID
    uint8_t uidLength; // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, & uid[0], & uidLength);
    if (success){
      Serial.println(F("Tarjeta encontrada!"));
      Serial.print(F("UID Tamaño: "));
      Serial.print(uidLength, DEC);
      Serial.println(F(" bytes"));
      Serial.print(F("UID Valor: "));
      nfc.PrintHex(uid, uidLength);
      Serial.print(F("\n"));
      delay(100);
      Serial.println(F("Grabando en memoria"));
      Serial.println(F("*******************"));
      for(byte i=0; i<4; i++){
        EEPROM[dirmem] = uid[i];
        dirmem += 1;
      }
      Serial.print(F("Llave "));
      nfc.PrintHex(uid, uidLength);
      Serial.print(F(" grabada en memoria "));
      Serial.print(F(""));
      Serial.print(numKey);
      Serial.println("!\n");
      numKey += 1;
      t = millis();
      buzzer(1);      
    }
    digitalWrite(led, LOW);
    delay(300);
  }
  if (numKey > 1)
  {
    for (int x = dirmem; x < 20; ++x)
    {
      EEPROM[x] = 0xFF;
    }
    buzzer(4);
  }
}

void rpms(){
  RPM = pulseIn(RPM_sens, HIGH, 1000000);
  Serial.print("RPM motor");
  Serial.println(RPM);
  delay(500);
}

/*void leeKeys(){
  for(int e=0; e<16; e++){
    for(int x=0; x<uidKnownCount; x++){
      for(int y=0; y<uidKnownLength; y++){
        uidsKnown[x][y] = EEPROM[e];
      }
    } 
  }
  Serial.println("Cargadas keys de memoria");
}*/

/*String DataBT(){
  if (BT.available()){
    debug("bt available");
    while (BT.available()){
      btdata = (BT.readStringUntil('\n'));
      btdata.trim();
      btdata.toUpperCase();
    }
    Serial.println(btdata);
    return btdata;
    // if (btdata.equals("?"))
    //   progBT();
    // else
    //   debug("Opcion Invalida");
  }
}*/

/*if(DataBT().equals("ON")){
        if(!encendido()){
          digitalWrite(led, HIGH);
          debug("Inicio desde BT");
          if(digitalRead(fbrakeB) == LOW){
            debug("Freno Precionado!");
            carStart();
          }else{
            debug("Freno No Precionado!");
            accessory();
          }
        }*/

void Play_MarioUW(int x = 0)
{
  int y = (sizeof(MarioUW_note)/sizeof(int));
  if (x = 1){
    y = 6;
    x = 0;
  }else if(x=2){
    y = 14;
    x = 9;
  }
    for (int thisNote = x; thisNote < y; thisNote++) {

    int noteDuration = 1000 / MarioUW_duration[thisNote];//convert duration to time delay
    tone(buzz, MarioUW_note[thisNote], noteDuration);

    int pauseBetweenNotes = noteDuration * 1.80;
    delay(pauseBetweenNotes);
    noTone(buzz); //stop music on pin 8 
    }
}

void Play_CrazyFrog()
{
  for (int thisNote = 0; thisNote < (sizeof(CrazyFrog_note)/sizeof(int)); thisNote++) {

    int noteDuration = 1000 / CrazyFrog_duration[thisNote]; //convert duration to time delay
    tone(buzz, CrazyFrog_note[thisNote], noteDuration);

    int pauseBetweenNotes = noteDuration * 1.30;//Here 1.30 is tempo, decrease to play it faster
    delay(pauseBetweenNotes);
    noTone(buzz); //stop music on pin 8 
    }
}

void playDeath(int pin) {
 
  tone(pin, 494, 126);
  delay(127);
  delay(31);
  tone(pin, 698, 126);
  delay(127);
  delay(31);
  delay(169);
  tone(pin, 698, 126);
  delay(127);
  delay(31);
  tone(pin, 698, 169);
  delay(170);
  delay(42);
  tone(pin, 659, 169);
  delay(170);
  delay(42);
  tone(pin, 587, 169);
  delay(170);
  delay(42);
  tone(pin, 523, 126);
  delay(127);
  delay(31);
  tone(pin, 330, 126);
  delay(127);
  delay(31);
  delay(169);
  tone(pin, 330, 126);
  delay(127);
  delay(31);
  tone(pin, 262, 126);
  delay(127);
  delay(31);
  delay(21);
  noTone(pin);
}

void playSmbdeath(int pin) {
  tone(pin, 1047, 83);
  delay(84);
  tone(pin, 1047, 83);
  delay(84);
  tone(pin, 1047, 83);
  delay(84);
  delay(333);
  tone(pin, 988, 166);
  delay(167);
  tone(pin, 1397, 166);
  delay(167);
  delay(166);
  tone(pin, 1397, 166);
  delay(167);
  tone(pin, 1397, 249);
  delay(250);
  tone(pin, 1319, 249);
  delay(250);
  tone(pin, 1175, 166);
  delay(167);
  tone(pin, 1047, 166);
  delay(167);
  delay(166);
  tone(pin, 659, 166);
  delay(167);
  delay(166);
  tone(pin, 523, 166);
  delay(167);
  noTone(pin);
}

void playSmb_under(int pin) {
  tone(pin, 1047, 75);
  delay(76);
  delay(75);
  tone(pin, 2093, 75);
  delay(76);
  delay(75);
  tone(pin, 880, 75);
  delay(76);
  delay(75);
  tone(pin, 1760, 75);
  delay(76);
  delay(75);
  tone(pin, 932, 75);
  delay(76);
  delay(75);
  tone(pin, 1865, 75);
  delay(76);
  delay(1200);
  tone(pin, 1047, 75);
  delay(76);
  delay(75);
  tone(pin, 2093, 75);
  delay(76);
  delay(75);
  tone(pin, 880, 75);
  delay(76);
  delay(75);
  tone(pin, 1760, 75);
  delay(76);
  delay(75);
  tone(pin, 932, 75);
  delay(76);
  delay(75);
  tone(pin, 1865, 75);
  delay(76);
  delay(1200);
  tone(pin, 698, 75);
  delay(76);
  delay(75);
  tone(pin, 1397, 75);
  delay(76);
  delay(75);
  tone(pin, 587, 75);
  delay(76);
  delay(75);
  tone(pin, 1175, 75);
  delay(76);
  delay(75);
  tone(pin, 622, 75);
  delay(76);
  delay(75);
  tone(pin, 1245, 75);
  delay(76);
  delay(1200);
  tone(pin, 698, 75);
  delay(76);
  delay(75);
  tone(pin, 1397, 75);
  delay(76);
  delay(75);
  tone(pin, 587, 75);
  delay(76);
  delay(75);
  tone(pin, 1175, 75);
  delay(76);
  delay(75);
  tone(pin, 622, 75);
  delay(76);
  delay(75);
  tone(pin, 1245, 75);
  delay(76);
  noTone(pin);
}
