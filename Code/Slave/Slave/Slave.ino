#include <SPI.h>
#include <MFRC522.h>
#include <CAN.h>
#include <Wire.h> 
#include <SSD1306AsciiWire.h>


String device_name = "N4";

int card_call = 0xA4;
int beat_call = 0xB4;

int device_name_len = device_name.length();

// Define OLED display I2C adress and library variable
// Adress can be 0x3C or 0x3D
#define I2C_ADDRESS 0x3C
SSD1306AsciiWire lcd;

// Define RFID pins and library variable
#define RST_PIN         9          
#define SS_PIN          8         
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Define CAN module pins
#define CAN_CS_PIN 7 // CAN
#define CAN_INT_PIN 3 //CAN

// Variables for switching state of RFID(when RFID reads card card_state = 1, otherwise card_state = 0)
bool card_state = 0;
bool last_state = 0;
bool read_card;
String no_card_str = "0xFFFFFF";

// Pins for LEDs
int RFID_led = A2;
int CAN_led = A3;

// Variables for counting time
unsigned long interval = 10000;
unsigned long previous_millis = 0;

// void setup() runs only once, it initialize all physical modules and check their state
void setup() {
  
  // Begin serial monitor for debugging
  Serial.begin(9600);

  // Init SPI bus and MFRC522
	Wire.begin();
  Wire.setClock(400000L);
  SPI.begin();			
	mfrc522.PCD_Init();

  // Init OLED
  lcd.begin(&Adafruit128x64, I2C_ADDRESS);
  lcd.setFont(System5x7);
  lcd.setCursor(0,0);
  lcd.print("Connecting");
  delay(2000);

  // set led pins as OUTPUT
  pinMode(RFID_led, OUTPUT);
  pinMode(CAN_led, OUTPUT);
  
  // Init CAN
  CAN.setPins(CAN_CS_PIN);
  Serial.println("Starting CAN Bus...");
  // start the CAN bus at 500 kbps
  while (true) {
    if (!CAN.begin(500E3)){
      Serial.println("Starting CAN failed!");
      delay(2000);
    }
    else {
    lcd.clear();
    Serial.println("Starting CAN bus is succesfull!");
    analogWrite(CAN_led, 255);
    lcd.setCursor(0,0);
    lcd.print("CANBUS ON");
    break;
    }
  }

  // return True when mfrc522.PICC_IsNewCardPresent() and False when !mfrc522.PICC_IsNewCardPresent()
  // this statement check if reader is working
  if (mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_IsNewCardPresent()) {
    analogWrite(RFID_led, 255);
    Serial.println("RFID ready");
    lcd.setCursor(0,1);
    lcd.print("RFID ON");
  }
  else {
    Serial.println("RFID not working!");
    lcd.setCursor(0,1);
    lcd.print("RFID not working!");
  }
  
  // End of Setup, device is ready
  delay(2000);
  lcd.clear();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready!");
}

void loop() {

// Device reads card ID once, working like toggle switch.
// Save card ID to variable card_id, send it through CAN bus and write out card_id on OLED display

// Loop variables 
// Step 1. Reading 
// Step 2. Heartbeat
// Step 3. Troubleshoot

//  Loop variables
read_card = mfrc522.PICC_IsNewCardPresent(); // No card = 0(False), card = 1(True)
String card_id;


  // Step 1.

  if (CAN.begin(500E3) == true) {
    // Check if CAN bus work properly and light up LED
    //analogWrite(CAN_led, 255);
    if (read_card) {
      if (mfrc522.PICC_ReadCardSerial()){
      // Read card ID and save it to card_id variable
      for (byte i = 0; i <= mfrc522.uid.size; i++) {
        card_id += String(mfrc522.uid.uidByte[i], HEX);
        card_id.toUpperCase();
        card_state = mfrc522.PICC_ReadCardSerial();
        }
      }

    if (read_card > last_state) {
      Serial.println("Card in!");
      Serial.println(card_id);
      CAN.beginPacket(card_call);
      for (int bytes_num = 0; bytes_num <= 7; bytes_num++){
        CAN.write(card_id[bytes_num]);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Card ID:");
        lcd.setCursor(0, 1);
        lcd.print(card_id);
        }
      CAN.endPacket();
      }
    }
    
    if (read_card < last_state) {
      Serial.println("No card!");
      Serial.println(no_card_str);
      CAN.beginPacket(card_call);
      for (int bytes_num = 0; bytes_num <= 7; bytes_num++) {
        CAN.write(no_card_str[bytes_num]);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Card ID:");
        lcd.setCursor(0, 1);
        lcd.print(no_card_str);
      }
      CAN.endPacket();  
    }

    last_state = read_card;
    mfrc522.PICC_HaltA();
    }

    else {
     analogWrite(CAN_led, 0);
    }
    
    // End of Step 1

    // Step 2

    // Slave device send "hearbteat" to master every 10seconds

    unsigned long current_millis = millis();
    
    if ((unsigned long)(current_millis - previous_millis) >= interval){
      // Serial.println("hb");
      CAN.beginPacket(beat_call);
      for (int i = 0; i < device_name_len; i++) {
        CAN.write(device_name[i]);
      //  Serial.print(device_name[i]);
      }
      Serial.println("");
      CAN.endPacket();
      previous_millis = current_millis;
    }
    
    // End of Step 2

    // Step 3

    // Troubleshoot messages when is RFID or CAN module disconnected

    while (!CAN.begin(500E3)){
      analogWrite(CAN_led, 0);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CAN Disconn.!");
      delay(1000);
      if (CAN.begin(500E3)){
        analogWrite(CAN_led, 255);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Ready!");
        break;
      }
    }

    while (mfrc522.PCD_PerformSelfTest() == false)  {
      analogWrite(RFID_led, 0);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("RFID Disconn.!");
      delay(1000);
      if (mfrc522.PCD_PerformSelfTest() == true) {
        analogWrite(RFID_led, 255);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Ready!");
        break;
      }
    }
  }