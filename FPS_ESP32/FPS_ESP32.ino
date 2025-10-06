#include "WiFi.h"
#include "ThingSpeak.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Ορισμός GPIO Pins
#define DHTPIN 4
#define DHTTYPE DHT11
#define FLAME_SENSOR_ANALOG 33
#define MQ2_ANALOG_PIN 34
#define MQ7_ANALOG_PIN 32
#define BUZZER_PIN 5
#define LDR_PIN 35

// Ορισμός παραμέτρων
const char* ssid = "xxxxxxxxx";
const char* password = "xxxxxxxxxxx";

const unsigned long myChannelNumber = xxxxxxxxx;
const char* myWriteAPIKey = "xxxxxxxxxxxx";
unsigned long myTalkBackID = xxxxxxxx;
const char* myTalkBackKey = "xxxxxxxxxxxx";

WiFiClient client;
DHT dht(DHTPIN, DHTTYPE);

unsigned long lastTime = 0;
unsigned long timerDelay = 15000;

// Μεταβλητές αισθητήρων
float temperatureC, humidity;
int flameAnalog, mq2Analog, mq7Analog, ldrValue;
bool fireDetected = false;

// Συνάρτηση WiFi Σύνδεσης
void connectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
      delay(5000);
    }
    Serial.println("\nConnected to WiFi.");
  }
}

// Συνάρτηση Ανάγνωσης Αισθητήρων 
void readSensors() {
  temperatureC = dht.readTemperature();
  humidity = dht.readHumidity();
  flameAnalog = analogRead(FLAME_SENSOR_ANALOG);
  mq2Analog = analogRead(MQ2_ANALOG_PIN);
  mq7Analog = analogRead(MQ7_ANALOG_PIN);
  ldrValue = analogRead(LDR_PIN);

// Δοκιμή εάν δουλεύει ο DHT11
  if (isnan(temperatureC) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
  }
}

//Συνάρτηση Ανίχνευσης Φλόγας και ρύθμιση κατοφλίου με χρήση LDR
void detectFire() {
  int flameThreshold;

  if (ldrValue < 400) {
    flameThreshold = 400;  // Νύχτα υψηλή ευαισθησία
  } else if (ldrValue >= 400 && ldrValue <= 2000) {
    flameThreshold = 250;  // Σκιερό περιβάλλον κανονική ευαισθησία

// Δοκιμαστικη συνθήκη 

    // else if (ldrValue >= 400 && ldrValue <= 3500) {
    // flameThreshold = 210; // Σκιερό περιβάλλον κανονική ευαισθησία

  } else {
    flameThreshold = 350;  // Ημέρα Αγνοεί το φως του ήλιου αλλά ανιχνεύει φλόγα
  }

  fireDetected = (flameAnalog < flameThreshold);
  if (fireDetected) {
    Serial.println(" Fire Detected!");
  }
}


// Συνάρτηση αυτόματης ενεργοποίησης Buzzer 
void checkAlarm() {
  bool smokeDetected = (mq2Analog > 2000);

  if (fireDetected && smokeDetected) {
    Serial.println(" Fire AND Smoke Detected! ");
    for (int i = 0; i < 5; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(BUZZER_PIN, LOW);
      delay(100);
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// Συνάρτηση αποστολής στο ThingSpeak 
void sendToThingSpeak() {
  ThingSpeak.setField(1, flameAnalog);
  ThingSpeak.setField(2, temperatureC);
  ThingSpeak.setField(3, mq2Analog);
  ThingSpeak.setField(4, mq7Analog);
  ThingSpeak.setField(5, humidity);
  ThingSpeak.setField(6, ldrValue);

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200) {
    Serial.println("Channel update successful.");
  } else {
    Serial.println("Problem updating channel. HTTP error code: " + String(x));
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(FLAME_SENSOR_ANALOG, INPUT);
  pinMode(MQ2_ANALOG_PIN, INPUT);
  pinMode(MQ7_ANALOG_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(LDR_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client);
}


// Συνάρτηση για την ανάγνωση της τιμής του alert από το ThingSpeak
void checkAlert() {
  HTTPClient http;
  String url = "https://api.thingspeak.com/channels/xxxxxxx/fields/1.json?api_key=xxxxxxxxxxxxxxxx";

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    //Serial.println("Payload received: " + payload); 

    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);

    JsonArray feeds = doc["feeds"];
    if (feeds.size() > 0) {              // Έλεγχος αν υπάρχουν δεδομένα
      int lastIndex = feeds.size() - 1;  // Παίρνουμε την πιο πρόσφατη εγγραφή
      String alertValue = feeds[lastIndex]["field1"].as<String>();

      Serial.print("Latest Alert Value: ");
      Serial.println(alertValue);

      if (alertValue == "1") {
        Serial.println("Alert received! Turning Buzzer ON for 4 seconds...");
        digitalWrite(BUZZER_PIN, HIGH);
        delay(4000);
        digitalWrite(BUZZER_PIN, LOW);
      } else {
        Serial.println("No alert triggered");
      }
    } else {
      Serial.println("No data available in ThingSpeak");
    }
  } else {
    Serial.println("Error reading alert value from ThingSpeak");
  }

  http.end();
}



void talkBack() {
  if (WiFi.status() == WL_CONNECTED) {
    // Constuctr TalkBack URL
    String tbURI = "/talkbacks/" + String(myTalkBackID) + "/commands/execute";
    String postMessage = "api_key=" + String(myTalkBackKey);

    // ελεγχος για νεα εντολή
    String newCommand;
    int httpCode = httpPOST(tbURI, postMessage, newCommand);
    client.stop();

    if (httpCode == 200) {
      //Serial.println("Checking TalkBack queue...");
      //Serial.print("Full TalkBack Response:\n");
      //Serial.println(newCommand); // απαντηση thingspeak

      // Διαχωρισμός των γραμμών από το TalkBack
      int firstNewline = newCommand.indexOf('\n');
      int secondNewline = newCommand.indexOf('\n', firstNewline + 1);

      String actualCommand;
      if (firstNewline != -1 && secondNewline != -1) {
        actualCommand = newCommand.substring(firstNewline + 1, secondNewline);  // Παίρνουμε τη δεύτερη γραμμή
      } else {
        actualCommand = newCommand;
      }

      actualCommand.trim();  // Αφαιρούμε κενά και νέα γραμμή
     // Serial.print("Processed command: ");
      //Serial.println(actualCommand);

      // Έλεγχος και εκτέλεση εντολών
      if (actualCommand.equalsIgnoreCase("Buzzer_ON")) {
        //Serial.println("Turning Buzzer ON...");
        digitalWrite(BUZZER_PIN, HIGH);
        delay(3000);
        digitalWrite(BUZZER_PIN, LOW);
        Serial.println("Buzzer OFF");
      } else if (actualCommand.equalsIgnoreCase("Buzzer_OFF")) {
        //Serial.println("Turning Buzzer OFF...");
        digitalWrite(BUZZER_PIN, LOW);
      } else {
        Serial.println(" Unknown command received.");
      }
    } else {
      Serial.println("Error checking queue. HTTP Code: " + String(httpCode));
    }
  }
}

void loop() {
  talkBack();
  checkAlert();

  if ((millis() - lastTime) > timerDelay) {
    connectWiFi();
    readSensors();
    detectFire();
    checkAlarm();

    // Εμφάνιση μετρήσεων
    Serial.println("----- Sensor Readings -----");
    Serial.print("Temperature: ");
    Serial.print(temperatureC);
    Serial.println(" ºC");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
    Serial.print("Flame Analog Value: ");
    Serial.println(flameAnalog);
    Serial.print("MQ-2 Analog Value: ");
    Serial.println(mq2Analog);
    Serial.print("MQ-7 Analog Value: ");
    Serial.println(mq7Analog);
    Serial.print("LDR Value: ");
    Serial.println(ldrValue);
    Serial.println("---------------------------");
    sendToThingSpeak();
    lastTime = millis();
  }
}

// συνάρτηση send HTTP POST request to ThingSpeak
int httpPOST(String uri, String postMessage, String& response) {
  if (!client.connect("api.thingspeak.com", 80)) {
    return -301;  // Connection failed
  }

  postMessage += "&headers=false";

  String Headers = "POST " + uri + " HTTP/1.1\r\n" + "Host: api.thingspeak.com\r\n" + "Content-Type: application/x-www-form-urlencoded\r\n" + "Connection: close\r\n" + "Content-Length: " + String(postMessage.length()) + "\r\n\r\n";

  client.print(Headers);
  client.print(postMessage);

  long startTime = millis();
  while (client.available() == 0 && millis() - startTime < 5000) {
    delay(100);
  }

  if (client.available() == 0) return -304;  // No response from server

  if (!client.find("HTTP/1.1")) return -303;

  int status = client.parseInt();
  if (status != 200) return status;

  if (!client.find("\n\r\n")) return -303;

  response = client.readString();
  return status;
}
