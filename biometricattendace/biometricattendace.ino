#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>

// Use Hardware Serial (UART2) for fingerprint sensor
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

const char *ssid = "Error 404";
const char *password = "1theke404";
String serverURL = "http://192.168.0.146/biometricattendance/getdata.php";  
int FingerID = 0;     
uint8_t id;
bool sensorInitialized = false;

// FUNCTION DECLARATIONS
void connectToWiFi();
bool initFingerprintSensor();
void softResetSensor();
int getFingerprintID();
void DisplayFingerprintID();
void SendFingerprintID(int fingerID);
void ChecktoDeleteID();
uint8_t deleteFingerprint(int id);
void ChecktoAddID();
uint8_t getFingerprintEnroll();
void confirmAdding();

void setup() {
  Serial.begin(115200);
  delay(1000);  // Critical delay for serial initialization
  
  Serial.println("\n\nStarting Biometric System...");
  
  // Initialize WiFi properly
  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_STA);
  
  connectToWiFi();
  
  // Initialize fingerprint sensor with retry mechanism
  if (!initFingerprintSensor()) {
    Serial.println("Sensor initialization failed! Retrying...");
    delay(2000);
    if (!initFingerprintSensor()) {
      Serial.println("Fatal error: Could not initialize sensor");
      while(1); // Halt if sensor can't be initialized
    }
  }

  // Sensor info
  finger.getTemplateCount();
  Serial.print("Sensor contains "); 
  Serial.print(finger.templateCount); 
  Serial.println(" templates");
  Serial.println("Waiting for valid finger...");
}

void loop() {
  // Maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
  
  // Periodically verify sensor connection
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {  // Check every 10 seconds
    if (!finger.verifyPassword()) {
      Serial.println("Sensor not responding! Reinitializing...");
      initFingerprintSensor();
    }
    lastCheck = millis();
  }
  
  // Main operations
  FingerID = getFingerprintID();  
  DisplayFingerprintID();
  ChecktoAddID();
  ChecktoDeleteID();
  
  delay(50);
}

bool initFingerprintSensor() {
  Serial.println("Initializing fingerprint sensor...");
  
  // Try multiple baud rates
  long baudRates[] = {57600, 115200, 9600, 38400, 19200};
  
  for (int i = 0; i < 5; i++) {
    mySerial.end();
    delay(100);
    mySerial.begin(baudRates[i], SERIAL_8N1, 16, 17);  // UART2 pins
    finger.begin(baudRates[i]);
    
    // Give sensor time to initialize
    delay(300);
    
    if (finger.verifyPassword()) {
      Serial.print("Connected at ");
      Serial.print(baudRates[i]);
      Serial.println(" baud");
      sensorInitialized = true;
      return true;
    }
    Serial.print("Failed at ");
    Serial.print(baudRates[i]);
    Serial.println(" baud");
  }
  
  // Final attempt with software reset
  softResetSensor();
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);
  delay(500);
  
  if (finger.verifyPassword()) {
    Serial.println("Connected after reset at 57600 baud");
    sensorInitialized = true;
    return true;
  }
  
  Serial.println("Sensor initialization failed");
  sensorInitialized = false;
  return false;
}

void softResetSensor() {
  Serial.println("Performing software reset...");
  uint8_t resetCmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x15, 0x00, 0x19};
  mySerial.write(resetCmd, sizeof(resetCmd));
  delay(1500);  // Allow time for reset
}

int getFingerprintID() {
  if (!sensorInitialized) return -2;
  
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      return 0;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -2;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return -2;
    default:
      Serial.println("Unknown error");
      return -2;
  }

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -2;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find features");
      return -2;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Invalid image");
      return -2;
    default:
      Serial.println("Unknown error");
      return -2;
  }

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a match!");
  } 
  else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return -2;
  } 
  else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("No match found");
    return -1;
  } 
  else {
    Serial.println("Unknown error");
    return -2;
  }   

  Serial.print("Found ID #"); 
  Serial.print(finger.fingerID); 
  Serial.print(" with confidence "); 
  Serial.println(finger.confidence);
  return finger.fingerID;
}

void DisplayFingerprintID() {
  if (FingerID > 0) {
    Serial.print("Valid fingerprint detected! ID: ");
    Serial.println(FingerID);
    SendFingerprintID(FingerID);
  } else if (FingerID == 0) {
    // Normal "no finger" state
  } else if (FingerID == -1) {
    Serial.println("Finger not recognized");
  } else if (FingerID == -2) {
    Serial.println("Sensor error");
  }
}

void SendFingerprintID(int fingerID) {
  HTTPClient http;
  WiFiClient client;
  
  String postData = "FingerID=" + String(fingerID);
  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  int httpCode = http.POST(postData);
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.print("HTTP Code: ");
    Serial.println(httpCode);
    Serial.print("Payload: ");
    Serial.println(payload);
    
    if (payload.substring(0, 5) == "login") {
      String user_name = payload.substring(5);
      Serial.print("Welcome: ");
      Serial.println(user_name);
    } else if (payload.substring(0, 6) == "logout") {
      String user_name = payload.substring(6);
      Serial.print("Goodbye: ");
      Serial.println(user_name);
    }
  } else {
    Serial.print("HTTP request failed, error: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }
  
  http.end();
  delay(1000);
}

void ChecktoDeleteID() {
  if (!sensorInitialized) return;
  
  WiFiClient client;
  HTTPClient http;
  String postData = "DeleteID=check";
  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  int httpCode = http.POST(postData);
  if (httpCode > 0) {
    String payload = http.getString();
    if (payload.substring(0, 6) == "del-id") {
      String del_id = payload.substring(6);
      Serial.print("Deleting ID: ");
      Serial.println(del_id);
      deleteFingerprint(del_id.toInt());
    }
  } else {
    Serial.print("Delete check failed, error: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }
  
  http.end();
}

uint8_t deleteFingerprint(int id) {
  Serial.print("Attempting to delete ID ");
  Serial.println(id);
  
  uint8_t p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    Serial.println("Deleted!");
  } 
  else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
  } 
  else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Invalid location");
  } 
  else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Flash write error");
  } 
  else {
    Serial.print("Unknown error: 0x");
    Serial.println(p, HEX);
  }   
  return p;
}

void ChecktoAddID() {
  if (!sensorInitialized) return;
  
  WiFiClient client;
  HTTPClient http;
  String postData = "Get_Fingerid=get_id";
  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  int httpCode = http.POST(postData);
  if (httpCode > 0) {
    String payload = http.getString();
    if (payload.substring(0, 6) == "add-id") {
      String add_id = payload.substring(6);
      Serial.print("Enrolling ID: ");
      Serial.println(add_id);
      id = add_id.toInt();
      getFingerprintEnroll();
    }
  } else {
    Serial.print("Add ID check failed, error: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }
  http.end();
}

uint8_t getFingerprintEnroll() {
  Serial.println("Ready to enroll new fingerprint");
  Serial.println("Please place finger on sensor...");
  
  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
    delay(50);
  }

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Invalid image");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

  Serial.println("Place same finger again");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
    delay(50);
  }

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Invalid image");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.print("Creating model for #");
  Serial.println(id);
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } 
  else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } 
  else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } 
  else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("Storing model #");
  Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
    confirmAdding();
  } 
  else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } 
  else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Invalid storage location");
    return p;
  } 
  else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Flash write error");
    return p;
  } 
  else {
    Serial.println("Unknown error");
    return p;
  } 
}

void confirmAdding() {
  WiFiClient client;
  HTTPClient http;
  String postData = "confirm_id=" + String(id);
  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  int httpCode = http.POST(postData);
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.print("Enrollment confirmation: ");
    Serial.println(payload);
  } else {
    Serial.print("Confirmation failed, error: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }
  http.end();
}

void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  Serial.print("\nConnecting to ");
  Serial.println(ssid);
  
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect. Continuing without WiFi");
  }
}