// This module include functionality for Master device

#include <SPI.h>
#include <Ethernet.h>
#include <CAN.h>
#include <Timer.h>
#include <Wire.h>
#include <SSD1306AsciiWire.h>




/////////////////////////////////////////////////////////////////////////////////////
// UUID strings

String master_device = "4f1c831d-066b-4df5-8af1-8e5bea8dad35";
//List of slaves, 32 UUID strings
String device[11] = {
  "Master",
  "b3bc59f5-c4a2-4b59-994b-a877ddcac043", // N1
  "2f1f5702-9e43-4a3f-9147-cd8e9b293cc7", // N2
  "de751782-e5a9-48be-b360-ed23594cbd21", // N3
  "32d324d3-177c-4f01-b076-9a1f98b5f4dc", // N4
  "5f39ba71-182f-4504-8aeb-a297ba81c11f", // N5
  "620330ac-57be-4585-9121-f68b3183d506", // N6
  "0b49894f-17c1-473c-9472-406df94985b6", // N7
  "b29abe73-97cf-406b-9c48-226e98142e28", // N8
  "5d3fc628-bd55-4fca-9fb1-4440984a35b0", // N9
  "9d70dca8-47c2-4edb-9759-6aad8bdb2b90" //  N10
};
///////////////////////////////////////////////////////////////////////////////////////



// each slave send packet with id of the packet 
// for example:
//              packet id: 0xA1
//              packet: Card ON
// this means that Slave N1 with UUID "b3bc59f5-c4a2-4b59-994b-a877ddcac043" reads card

// every 10 seconds Slave sends heartbeat packet to Master 
// for example:
//              packet id: 0xB4
//              packet: N4
// this means that Slave N4 with UUID "32d324d3-177c-4f01-b076-9a1f98b5f4dc" sends hearbeat packet and device_state is changed to true



// number of devices in set
int lineup_size = 11; // 1 master + 10 slaves


// set up timers for hearbeat
Timer timer[11] = {  
  timer[0],
  timer[1],
  timer[2],
  timer[3],
  timer[4],
  timer[5],
  timer[6],
  timer[7],
  timer[8],
  timer[9],
  timer[10]
};

// each Slave send timer_code similiar to the table
String device_timer_code[11] = {
  "Master",
  "N1",
  "N2",
  "N3",
  "N4",
  "N5",
  "N6",
  "N7",
  "N8",
  "N9",
  "N10"
};

// device_state define if Slave its on or off
bool device_state[11] = {
  device_state[0] = false, //Master
  device_state[1] = false,
  device_state[2] = false,
  device_state[3] = false,
  device_state[4] = false,
  device_state[5] = false,
  device_state[6] = false,
  device_state[7] = false,
  device_state[8] = false,
  device_state[9] = false,
  device_state[10] = false
};


int packets[] = {
  packets[0] = 0xFF,
  packets[1] = 0xA1,
  packets[2] = 0xA2,
  packets[3] = 0xA3,
  packets[4] = 0xA4,
  packets[5] = 0xA5,
  packets[6] = 0xA6,
  packets[7] = 0xA7,
  packets[8] = 0xA8,
  packets[9] = 0xA9,
  packets[10] = 0xA10
};

// 0X3C+SA0 - 0x3C or 0x3D
#define I2C_ADDRESS 0x3C

SSD1306AsciiWire lcd;

char server[] = "lan2can.stino.cloud";

byte mac[] = {0x10,0xD4, 0x61, 0xDB, 0xE3, 0x88};
IPAddress ip(192, 168, 0, 101);
//IPAddress server(192,168,0,107);
//IPAddress gateway(192,168,0,1);
//IPAddress subnet(255,255,255,0);
//IPAddress myDns(192, 168, 0, 1);
EthernetClient client;

#define CAN_CS_PIN 7 // CAN
#define CAN_INT_PIN 3 //CAN

unsigned long interval = 300000;
unsigned long previous_millis = 0;

unsigned long nano_reset_interval = 10000000;
unsigned long nano_reset_previous_millis = 0;

int ETH_led = 12;
int CAN_led = 11;

void setup() {

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Ethernet.init(53);
  Wire.begin();
  Wire.setClock(400000L);

  pinMode(ETH_led, OUTPUT);
  pinMode(CAN_led, OUTPUT);

  //OLED init
  lcd.begin(&Adafruit128x64, I2C_ADDRESS);
  lcd.setFont(System5x7);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connecting...");

  // start the Ethernet connection:
  Ethernet.begin(mac, ip);
  Serial.println(Ethernet.localIP()); 
  delay(1000);
  Serial.print("connecting to ");
  Serial.println(server);

  Serial.println(client.connected());

  // loops forever until connection to server is active,
  // then print server IP and number of devices on OLED display
  // and breaks out of loop

  while (true) {
    if (client.connect(server, 80)) {
      digitalWrite(ETH_led, HIGH);
      lcd.clear();   
      lcd.setCursor(0,0);
      lcd.print(server);
      lcd.setCursor(0,1);
      lcd.print("Devices: ");
      break;
    } 
  }
  // if client is active send HTTP request to server, this tells server that Master is ON
  if (client) {
    Serial.println("master");
    client.println("GET /cloud/master/" + master_device + " " + "HTTP/1.1");
    client.println("Host: lan2can.stino.cloud");
    client.println("Connection: close");
    client.println("");
    client.stop();
  }

  delay(100);

  // CAN init
  CAN.setPins(CAN_CS_PIN);
  Serial.println("CAN Receiver Callback");
  // start the CAN bus at 500 kbps
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    while (1);
  }
  digitalWrite(CAN_led, HIGH);
  delay(1000);

  timer[1].start();
  timer[2].start();
  timer[3].start();

  digitalWrite(ETH_led, HIGH);
  digitalWrite(CAN_led, HIGH);
}

void loop() {

// Init variables to save incoming CAN data to String
int device_counter = 0;
String letter;
String message;

int control_status = CAN.beginPacket(0x00);
int packetSize = CAN.parsePacket();
unsigned long nano_current_millis = millis();

//CAN reading
  if (client.connect(server, 80)){
    if (packetSize) {
      //received a packet
      int packet_id = CAN.packetId();
      Serial.println("");
      Serial.print("Received ");
      Serial.print("packet with id 0x");
      Serial.print(packet_id, HEX);

      if (CAN.packetRtr()) {
        Serial.print(" and requested length ");
        Serial.println(CAN.packetDlc());
      } 

      else {
        Serial.print(" and length ");
        Serial.println(packetSize);
        // catch bytes one by one to letter variable and merge them to one String in message variable
        while (CAN.available()) {
          letter = ((char)CAN.read());
          message = message + letter;  
        }
      }

      ethernetUpload(device, message, packet_id, packets);

      timerReset(device_timer_code, device_state, timer, message);
    }
  }

  for (int i = 0; i < lineup_size; i++) {
    device_counter += device_state[i];
  }

  //Master hearbeat
  unsigned long current_millis = millis();
  
  if ((unsigned long)(current_millis - previous_millis) >= interval) {
    Serial.println("hearbeat");
    client.println("GET /cloud/heartbeat/" + master_device + " " + "HTTP/1.1");
    client.println("Host: lan2can.stino.cloud");
    client.println("Connection: close");       
    client.println();
    //client.stop();
    previous_millis = current_millis;
  }

    // Troubleshoting
    // when client isnt connected to server it turns off Eth led and print warning to display
  if (!client.connected()) {
      // Cable is unplugged
      Serial.println("dis");
      digitalWrite(ETH_led, LOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Eth Disconn.!");
      delay(500);
  } 
  // when CAN isnt connected it turns off CAB led and print warning to display
  else if (CAN.packetId() == 536870911) {

        digitalWrite(CAN_led, LOW);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CAN Disconn.!");
        delay(500);
  }
  else {
      // Cable is plugged in
      lcd.setCursor(0, 0);
      lcd.print(server);
      lcd.setCursor(0, 1);
      lcd.print("Devices: ");
      lcd.print(device_counter);
      digitalWrite(ETH_led, HIGH);
      digitalWrite(CAN_led, HIGH);
  }
}

// this function parse incoming messages by packet_id, then assign to them and sends an http request to server

void ethernetUpload(String device_array[], String message, int packet_id, int packets[]) {
    for (int i = 0; i <= lineup_size; i++){
      if (packets[i] == packet_id) {
        Serial.println(device[i]);
        Serial.println(message);
        client.println("GET /cloud/devices/" + device[i] + "/cards/" + message + " " + "HTTP/1.1");
        client.println("Host: lan2can.stino.cloud");
        client.println("Connection: close");
        client.println();
        //client.stop();
        delay(100);     
    }
  } 
}

// this function parse incoming hearbeat messages by device_timer_code and starts or stop device timer
void timerReset(String device_timer_code[], bool device_state[], Timer timer[], String message) {
  for (int i = 0; i <= lineup_size; i++) {
    if (message == device_timer_code[i]) {
        Serial.println(device_timer_code[i]);
        device_state[i] = true;
        timer[i].stop();
        timer[i].start();
    }
    if (timer[i].read() >= nano_reset_interval) {
      device_state[i] = false;
    }
  }
}