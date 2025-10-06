#include <MKRWAN.h>
#include <DHT.h>

// LoRaWAN Configuration
LoRaModem modem;
String appEui = "xxxxxxxxxxxxxxxx"; 
String appKey = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; 

// Sensor Pins
#define DHTPIN 4  
#define DHTTYPE DHT11 
#define FLAME_SENSOR_ANALOG A1  
#define MQ2_ANALOG_PIN A2  
#define MQ7_ANALOG_PIN A3  
#define BUZZER_PIN 5  
#define LDR_PIN A4  

DHT dht(DHTPIN, DHTTYPE);  

// Sensor Variables
float temperatureC, humidity;  
int flameAnalog, mq2Analog, mq7Analog, ldrValue;  
bool fireDetected = false;  
bool loraConnected = false; 

// Read Sensors
void readSensors() {
  // Ανάγνωση δεδομένων από τον αισθητήρα DHT11
  temperatureC = dht.readTemperature();
  humidity = dht.readHumidity();

  // Έλεγχος αν οι τιμές είναι έγκυρες
  if (isnan(temperatureC) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    // Εκτύπωση θερμοκρασίας και υγρασίας
    Serial.print("Temperature: ");
    Serial.print(temperatureC);
    Serial.println(" °C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
  }

  // Ανάγνωση δεδομένων από τους υπόλοιπους αισθητήρες
  flameAnalog = analogRead(FLAME_SENSOR_ANALOG);
  mq2Analog = analogRead(MQ2_ANALOG_PIN);
  mq7Analog = analogRead(MQ7_ANALOG_PIN);
  ldrValue = analogRead(LDR_PIN);

  // Εκτύπωση των τιμών των αισθητήρων
  Serial.println("-----------------------------");

  Serial.print("Flame Sensor Value: ");
  Serial.println(flameAnalog);

  Serial.print("MQ2 Sensor Value: ");
  Serial.println(mq2Analog);

  Serial.print("MQ7 Sensor Value: ");
  Serial.println(mq7Analog);

  Serial.print("LDR Value: ");
  Serial.println(ldrValue);

  Serial.println("-----------------------------");
}


// Έλεγχος για την ανίχνευση φωτιάς 
void detectFire() {
  int flameThreshold = (ldrValue < 400) ? 400 : (ldrValue <= 2000 ? 250 : 350);  // Κατώφλι για τη φωτιά
  fireDetected = (flameAnalog < flameThreshold);  // Ανιχνεύεται φωτιά αν η τιμή είναι μικρότερη από το όριο
  if (fireDetected) {
    Serial.println("Fire Detected!"); 
  }
}


// Ενεργοποίηση buzzer όταν εντοπιστεί φωτιά και καπνός
void checkAlarm() {
  if (fireDetected && (mq2Analog > 2000)) {  
    Serial.println("Fire AND Smoke Detected!");
    digitalWrite(BUZZER_PIN, HIGH);  
    delay(1000);  
    digitalWrite(BUZZER_PIN, LOW);  
  }
}

// Αποστολή δεδομένων μέσω LoRaWAN με κατάσταση alert (1 ή 0)
void sendToLoRaWAN(bool alertStatus) {
  if (!loraConnected) return;  

  uint8_t payload[11];  
  payload[0] = (uint8_t)(temperatureC * 10);  
  payload[1] = (uint8_t)(humidity * 10);  
  payload[2] = (uint8_t)(flameAnalog >> 8);  
  payload[3] = (uint8_t)(flameAnalog & 0xFF);  
  payload[4] = (uint8_t)(mq2Analog >> 8);  
  payload[5] = (uint8_t)(mq2Analog & 0xFF);  
  payload[6] = (uint8_t)(mq7Analog >> 8);  
  payload[7] = (uint8_t)(mq7Analog & 0xFF);  
  payload[8] = (uint8_t)(ldrValue >> 8);  
  payload[9] = (uint8_t)(ldrValue & 0xFF);  
  payload[10] = alertStatus ? 1 : 0;  // Field7 = 1 όταν το buzzer είναι ενεργό, 0 όταν δεν είναι

  modem.beginPacket();
  modem.write(payload, sizeof(payload));  // Αποστολή δεδομένων στο TTN
  modem.endPacket();
  Serial.print("Sent Data to TTN with Alert Status: ");
  Serial.println(alertStatus);  // Εμφάνιση μηνύματος με την κατάσταση του alert
}

void checkDownlink() {
  if (modem.available()) {  // Αν υπάρχουν δεδομένα από downlink
    int receivedByte = modem.read();  // Ανάγνωση του byte από το downlink
    Serial.print("Received Downlink: ");
    Serial.println(receivedByte);

    if (receivedByte == 1) {  // Αν το downlink byte είναι 1, ενεργοποιούμε το buzzer
      Serial.println(" Remote Buzzer Activation!");
      digitalWrite(BUZZER_PIN, HIGH);  
      sendToLoRaWAN(true);  
      delay(3000);  
      digitalWrite(BUZZER_PIN, LOW);  
      sendToLoRaWAN(false);  
    }
  }
}

// Setup
void setup() {
  Serial.begin(115200); 
  dht.begin();  
  pinMode(BUZZER_PIN, OUTPUT);  
  digitalWrite(BUZZER_PIN, LOW);  

  // Προσπάθεια σύνδεσης στο LoRa δίκτυο
  Serial.println("Attempting to join LoRa network...");
  if (modem.begin(EU868)) {  
    if (modem.joinOTAA(appEui, appKey)) {  
      loraConnected = true;
      Serial.println("Successfully joined TTN.");
    } else {
      Serial.println("Failed to join TTN. Continuing without LoRa.");
    }
  } else {
    Serial.println("Failed to start LoRa modem. Continuing without LoRa.");
  }
}

// Loop
void loop() {
  readSensors();  
  detectFire();  
  checkAlarm();  
  sendToLoRaWAN(false);  
  checkDownlink();  
  delay(15000);  
}

