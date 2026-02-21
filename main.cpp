#include <SoftwareSerial.h>
#include <Arduino.h>

// ============ PIN DEFINITIONS ============
#define BUZZER 9      // Buzzer on pin 9
#define IR_PIN 7      // IR sensor on pin 7
#define PIR_PIN 3     // PIR motion sensor on pin 3

// GSM Module pins
#define GSM_RX 4      // GSM TX connected to Arduino pin 4
#define GSM_TX 5      // GSM RX connected to Arduino pin 5

// ============ PHONE NUMBERS ============
String simPhoneNumber = "+255628986009";    // The number INSIDE the GSM module's SIM
String alertPhoneNumber = "+255752326518";  // Phone to receive alerts

// ============ GLOBAL VARIABLES ============
SoftwareSerial gsm(GSM_RX, GSM_TX);
bool smsSent1 = false;
bool gsmReady = false;
unsigned long lastMotionTime = 0;
unsigned long lastIRTime = 0;
const unsigned long smsCooldown = 60000; // 60 seconds between SMS
int gsmStatus = 0; // 0=Unknown, 1=Registered, 2=Searching, 3=Denied

// ============ FUNCTION PROTOTYPES ============
void initializeGSM();
void checkGSMStatus();
void sendSMS(String message);
String readGSMResponse();
void checkSensors();

// ============ SETUP FUNCTION ============
void setup() {
  // Initialize pins
  pinMode(BUZZER, OUTPUT);
  pinMode(IR_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  
  // Initialize serial communications
  Serial.begin(9600);     // Serial monitor
  gsm.begin(9600);        // GSM module
  
  digitalWrite(BUZZER, LOW);
  
  Serial.println("=================================");
  Serial.println("SECURITY SYSTEM STARTING...");
  Serial.println("=================================");
  
  // Initialize GSM module
  delay(2000);
  initializeGSM();
}

// ============ INITIALIZE GSM ============
void initializeGSM() {
  Serial.println("Initializing GSM module...");
  
  // Check if GSM responds
  gsm.println("AT");
  delay(1000);
  String response = readGSMResponse();
  
  if (response.indexOf("OK") >= 0) {
    Serial.println("GSM Module OK");
    
    // Set SMS to text mode
    gsm.println("AT+CMGF=1");
    delay(1000);
    readGSMResponse();
    
    // Check signal quality
    gsm.println("AT+CSQ");
    delay(1000);
    response = readGSMResponse();
    Serial.print("Signal Quality: ");
    Serial.println(response);
    
    // Check network registration
    gsm.println("AT+CREG?");
    delay(1000);
    response = readGSMResponse();
    
    if (response.indexOf("+CREG: 0,1") >= 0 || response.indexOf("+CREG: 0,5") >= 0) {
      Serial.println("GSM Registered on network");
      gsmReady = true;
    } else {
      Serial.println("GSM NOT registered - Check SIM and antenna");
      Serial.println("Module will keep trying...");
    }
    
    // Get module info
    gsm.println("AT+CGMI"); // Manufacturer
    delay(1000);
    readGSMResponse();
    
    gsm.println("AT+CGMM"); // Model
    delay(1000);
    readGSMResponse();
    
  } else {
    Serial.println("GSM Module NOT RESPONDING!");
    Serial.println("Check connections and power");
  }
  
  Serial.println("=================================");
}

// ============ READ GSM RESPONSE ============
String readGSMResponse() {
  String response = "";
  unsigned long timeout = millis() + 2000;
  
  while (millis() < timeout) {
    while (gsm.available()) {
      char c = gsm.read();
      response += c;
      Serial.print(c); // Echo to serial monitor
    }
  }
  
  return response;
}

// ============ MAIN LOOP ============
void loop() {
  // Check GSM status every 30 seconds
  static unsigned long lastGSMCheck = 0;
  if (millis() - lastGSMCheck > 30000) {
    lastGSMCheck = millis();
    checkGSMStatus();
  }
  
  // Check sensors (but with debounce to prevent flooding)
  checkSensors();
  
  // Print sensor status every 10 seconds
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 10000) {
    lastPrint = millis();
    int motion = digitalRead(PIR_PIN);
    int ir = digitalRead(IR_PIN);
    Serial.print("STATUS - PIR: ");
    Serial.print(motion);
    Serial.print(" | IR: ");
    Serial.print(ir);
    Serial.print(" | GSM: ");
    Serial.println(gsmReady ? "Ready" : "Not Ready");
  }
  
  delay(200); // Small delay
}

// ============ CHECK GSM STATUS ============
void checkGSMStatus() {
  if (!gsmReady) {
    // Try to re-initialize
    initializeGSM();
    return;
  }
  
  // Check network registration
  gsm.println("AT+CREG?");
  delay(500);
  
  while (gsm.available()) {
    String response = gsm.readString();
    Serial.print("GSM Status: ");
    Serial.println(response);
    
    if (response.indexOf("+CREG: 0,1") >= 0 || response.indexOf("+CREG: 0,5") >= 0) {
      Serial.println("✓ GSM Registered on network");
      
      // Check signal strength
      gsm.println("AT+CSQ");
      delay(500);
      
      // Blink pattern for status (fast blink = good)
      for(int i=0; i<3; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
      }
      
    } else {
      Serial.println("✗ GSM NOT Registered - Slow blink indicates searching");
      
      // Slow blink pattern for searching
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      digitalWrite(LED_BUILTIN, LOW);
      delay(1000);
    }
  }
}

// ============ CHECK SENSORS WITH DEBOUNCE ============
void checkSensors() {
  static bool lastMotionState = false;
  static bool lastIRState = false;
  static unsigned long lastMotionAlert = 0;
  static unsigned long lastIRAlert = 0;
  
  int motion = digitalRead(PIR_PIN);
  int ir = digitalRead(IR_PIN);
  
  unsigned long now = millis();
  
  // Motion detection (PIR) - only trigger on RISING edge
  if (motion == HIGH && lastMotionState == false) {
    Serial.println("🔴 MOTION DETECTED! Intruder alert!");
    
    // Send SMS only if cooldown period has passed
    if (!smsSent1 && (now - lastMotionAlert > smsCooldown)) {
      sendSMS("KUNA MWIZI ANAINGIA GHOST 777 !!!!!!!");
      smsSent1 = true;
      lastMotionAlert = now;
    }
  }
  
  // IR detection (beam broken) - only trigger on FALLING edge (LOW means broken)
  if (ir == LOW && lastIRState == true) {
    Serial.println("🔴 IR BEAM BROKEN! Buzzer ON");
    digitalWrite(BUZZER, HIGH);
    
    // Send SMS for IR alert with cooldown
    if (now - lastIRAlert > smsCooldown) {
      sendSMS("MLINZI: Unauthorized entry detected!");
      lastIRAlert = now;
    }
  }
  
  // Turn off buzzer when IR beam is restored
  if (ir == HIGH && lastIRState == false) {
    Serial.println("🟢 IR Beam restored - Buzzer OFF");
    digitalWrite(BUZZER, LOW);
  }
  
  // Reset smsSent flag after motion stops (with delay)
  static unsigned long motionStopTime = 0;
  if (motion == LOW && lastMotionState == true) {
    // Wait 10 seconds before allowing new SMS
    if (motionStopTime == 0) {
      motionStopTime = now;
    }
    if (now - motionStopTime > 10000) {
      smsSent1 = false;
      motionStopTime = 0;
    }
  } else {
    motionStopTime = 0;
  }
  
  // Update last states
  lastMotionState = motion;
  lastIRState = ir;
}

// ============ SEND SMS FUNCTION ============
void sendSMS(String message) {
  if (!gsmReady) {
    Serial.println("GSM not ready - Cannot send SMS");
    return;
  }
  
  Serial.println("=================================");
  Serial.println("Preparing to send SMS...");
  Serial.println("Message: " + message);
  Serial.println("Sending to: " + alertPhoneNumber);
  
  // Test GSM module
  gsm.println("AT");
  delay(1000);
  
  // Set SMS to text mode
  gsm.println("AT+CMGF=1");
  delay(1000);
  
  // Set recipient phone number
  gsm.print("AT+CMGS=\"");
  gsm.print(alertPhoneNumber);
  gsm.println("\"");
  delay(1000);
  
  // Send message content
  gsm.print(message);
  delay(1000);
  
  // Send Ctrl+Z character to send SMS
  gsm.write(26); // 26 is ASCII for Ctrl+Z
  delay(5000);
  
  Serial.println("✓ SMS Sent Successfully!");
  
  // Check for response
  String response = "";
  while (gsm.available()) {
    char c = gsm.read();
    response += c;
    Serial.write(c);
  }
  
  if (response.indexOf("OK") >= 0 || response.indexOf("+CMGS") >= 0) {
    Serial.println("✓ SMS delivery confirmed");
  } else {
    Serial.println("⚠ SMS may not have been delivered");
  }
  
  Serial.println("=================================");
}

// ============ TEST GSM FUNCTION ============
void testGSM() {
  Serial.println("Testing GSM Module...");
  
  gsm.println("AT");
  delay(1000);
  Serial.println("Response to AT:");
  readGSMResponse();
  
  gsm.println("AT+CSQ");
  delay(1000);
  Serial.println("Signal Quality (0-31, 99=unknown):");
  readGSMResponse();
  
  gsm.println("AT+CREG?");
  delay(1000);
  Serial.println("Network Registration:");
  readGSMResponse();
  
  gsm.println("AT+CMGF=1");
  delay(1000);
  Serial.println("Set SMS Mode:");
  readGSMResponse();
}