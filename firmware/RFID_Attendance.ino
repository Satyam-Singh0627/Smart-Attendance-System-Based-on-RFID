#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// --- Wi-Fi Configuration ---
// Structure to hold multiple SSID/Password pairs
struct WiFiCredentials {
  const char* ssid;
  const char* password;
};

// VITAL: Add your preferred Wi-Fi networks here. The device will try them in order.
 const WiFiCredentials networks[] = {
  {"YOUR_WIFI_1", "YOUR_PASSWORD_1"},
  {"YOUR_WIFI_2", "YOUR_PASSWORD_2"},
  {"YOUR_WIFI_3", "YOUR_PASSWORD_3"}   // <-- REPLACE THIS EXAMPLE WITH YOUR FIRST WIFI // <-- REPLACE THIS EXAMPLE WITH YOUR SECOND WIFI  
};
const int NUM_NETWORKS = sizeof(networks) / sizeof(networks[0]);

// Maximum number of 20-second connection cycles before a cool-down period.
const int MAX_WIFI_RETRIES = 5; 
// Delay (in ms) after all MAX_WIFI_RETRIES have failed before attempting a full connection sequence again. (1 minute)
const unsigned long WIFI_RETRY_DELAY_MS = 60000; 
unsigned long lastFailedConnectionTime = 0; // Tracks the last time a full retry sequence failed.

// --- Google Apps Script (GAS) Configuration ---
const String GAS_ID = "YOUR GAS ID";
const String host = "script.google.com";

// --- RFID Reader Configuration ---
#define SS_PIN D4 // Your pin for RFID SDA (Slave Select)
#define RST_PIN D3 // Your pin for RFID RST

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

// --- I2C LCD Configuration ---
const int I2C_ADDR = 0x27;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLS, LCD_ROWS);

// --- Buzzer Configuration ---
#define BUZZER_PIN D8 // D8 for the buzzer

// --- State and Timing Management ---
const unsigned long DISPLAY_TIME = 3000;
unsigned long lastDisplayTime = 0;
bool displayActive = false;

// --- Error Handling ---
const int MAX_NETWORK_ERRORS = 3;
int consecutiveNetworkErrors = 0;

// --- NTP Client Configuration ---
const long gmtOffset_sec = 5.5 * 3600;
const int daylightOffset_sec = 0;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", gmtOffset_sec, daylightOffset_sec);


// --- Function Prototypes ---
void displayMessage(String line1, String line2, int delayMs = 0);
void buzz(int duration, int frequency);
void syncTime();
bool connectWiFi(); // Returns true on success, false on failure

// --- Arduino Setup Function ---
void setup() {
  Serial.begin(115200);
  delay(1000); 

  // Initialize LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  displayMessage("System Booting...", "Please Wait...");

  // Initialize RFID Reader
  SPI.begin();
  mfrc522.PCD_Init();
  displayMessage("RFID Reader", "Initialized!");

  // Initialize Buzzer Pin
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Connect to WiFi
  if (connectWiFi()) {
    syncTime();
    displayMessage("Tap your card...", "");
  } else {
    // If initial connection failed, the loop will handle retries later
    displayMessage("NO WIFI", "System Offline");
    lastFailedConnectionTime = millis(); // Initialize the timer
  }
}

// --- Arduino Loop Function ---
void loop() {
  // --- ROBUSTNESS CHECK: Check if WiFi is still connected ---
  if (WiFi.status() != WL_CONNECTED) {
    // 1. Check if we are outside the cool-down period
    if (millis() - lastFailedConnectionTime >= WIFI_RETRY_DELAY_MS) {
      Serial.println("\n--- Starting New WiFi Retry Cycle ---");
      displayMessage("WiFi Disconn.", "Connecting...");
      buzz(500, 800); // Distinct sound for connection loss
      
      // Attempt connection (tries all networks up to MAX_WIFI_RETRIES cycles)
      if (connectWiFi()) {
        // Success
        syncTime(); 
        displayMessage("Tap your card...", "");
      } else {
        // Full failure: set the timestamp for the next retry attempt
        lastFailedConnectionTime = millis();
        displayMessage("NO WIFI", "Wait 60s");
        Serial.println("Next WiFi retry in 60 seconds.");
      }
    } else {
      // Still in cool-down period
      long remainingSeconds = (WIFI_RETRY_DELAY_MS - (millis() - lastFailedConnectionTime)) / 1000;
      // Only update the display every second to show the countdown
      if (remainingSeconds >= 0 && millis() % 1000 < 50) { 
        displayMessage("NO WIFI", "Wait " + String(remainingSeconds) + "s");
      }
      delay(100); // Short delay to prevent excessive loop cycling
      return; // Skip the rest of the loop while disconnected and cooling down
    }
  }

  // Check if it's time to clear the display after a successful scan
  if (displayActive && millis() - lastDisplayTime >= DISPLAY_TIME) {
    lcd.clear();
    displayMessage("Tap your card...", "");
    displayActive = false;
  }

  // Look for new RFID cards
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  
  // --- Start HTTP Process (Only runs if connected) ---
  
  // Get the RFID UID
  String rfidUid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      rfidUid.concat("0");
    }
    rfidUid.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  rfidUid.toUpperCase();
  Serial.print("RFID Detected: ");
  Serial.println(rfidUid);

  // --- IMMEDIATE FEEDBACK BEFORE LONG NETWORK WAIT ---
  buzz(150, 1000);
  displayMessage("Connecting", "Server..."); 

  // Send request to Apps Script
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = "https://script.google.com/macros/s/" + String(GAS_ID) + "/exec?rfid=" + rfidUid;
  http.begin(client, url);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET(); 
  Serial.print("HTTP Response Code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    // --- SUCCESS: RESET ERROR COUNTER ---
    consecutiveNetworkErrors = 0; 

    if (httpCode == HTTP_CODE_OK) {
      String response = http.getString();
      Serial.print("Response: ");
      Serial.println(response);

      // Parse the response
      int firstPipe = response.indexOf('|');
      String status = response.substring(0, firstPipe);
      String payload = response.substring(firstPipe + 1);

      if (status == "OK") {
        int rollNoEnd = payload.indexOf('|');
        String rollNo = payload.substring(0, rollNoEnd);
        String nameAndSubject = payload.substring(rollNoEnd + 1);
        int nameEnd = nameAndSubject.indexOf('|');
        String studentName = nameAndSubject.substring(0, nameEnd); 
        
        displayMessage("Roll No: " + rollNo, studentName, DISPLAY_TIME);
        
      } else if (status == "HOLIDAY" || status == "ERROR") {
        displayMessage(status, payload, DISPLAY_TIME);
        buzz(500, 500);
        
      } else {
        displayMessage("Unknown Response", response, DISPLAY_TIME);
      }
    } else {
      displayMessage("HTTP Error", String(httpCode), DISPLAY_TIME);
      buzz(500, 500);
    }
  } else {
    // --- FAILURE: NETWORK ERROR LOGIC (During HTTP Request) ---
    consecutiveNetworkErrors++;

    if (consecutiveNetworkErrors >= MAX_NETWORK_ERRORS) {
      Serial.println("\n!!! Max network errors reached. Forcing WiFi reconnect...");
      displayMessage("Network Fail", "Reconnecting...");
      buzz(1000, 400); 
      
      // Force a full reconnect cycle (uses the bounded retry logic)
      connectWiFi();
    } else {
      displayMessage("Network Error", "Check connection", DISPLAY_TIME);
      buzz(500, 500);
    }
  }
  http.end();
}

/**
 * Attempts to connect to all configured Wi-Fi networks, cycling through them 
 * up to MAX_WIFI_RETRIES times in total.
 * @return {bool} true if connected, false otherwise.
 */
bool connectWiFi() {
  consecutiveNetworkErrors = 0; // Reset network error counter
  WiFi.mode(WIFI_STA); 
  
  // Explicitly disconnect before trying to connect again for robustness
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    delay(100);
  }
  
  // Outer loop: Control the total number of full retry cycles
  for (int cycle = 1; cycle <= MAX_WIFI_RETRIES; cycle++) {
    
    // Inner loop: Try every network in the list once per cycle
    for (int netIndex = 0; netIndex < NUM_NETWORKS; netIndex++) {
      
      const char* currentSsid = networks[netIndex].ssid;
      const char* currentPassword = networks[netIndex].password;
      
      WiFi.begin(currentSsid, currentPassword);
      
      Serial.print("Attempt ");
      Serial.print(cycle);
      Serial.print("/");
      Serial.print(MAX_WIFI_RETRIES);
      Serial.print(" - Connecting to ");
      Serial.print(currentSsid);
      Serial.print("...");
      
      displayMessage("Net: " + String(currentSsid), "Cycle " + String(cycle) + "/" + String(MAX_WIFI_RETRIES));
      
      int attempts = 0;
      // Wait up to 20 seconds for connection (40 * 500ms)
      while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        // Success!
        Serial.println("\nConnected to WiFi!");
        return true; // Exit function successfully
      } else {
        // Connection failed for this network on this cycle
        Serial.println("\nNetwork failed. Trying next in list...");
        WiFi.disconnect(); // Disconnect to ensure clean attempt on next network
        delay(500);
      }
    } // End of inner (network) loop
    
    // If we finished the inner loop and still not connected, start next cycle after a short pause
    if (WiFi.status() != WL_CONNECTED && cycle < MAX_WIFI_RETRIES) {
        Serial.println("Starting next retry cycle...");
        delay(2000); // 2 second pause between full cycles
    }
    
  } // End of outer (retry cycle) loop

  // If we reach here, all retries failed across all networks.
  Serial.println("\nFATAL: All WiFi connection attempts failed. Returning failure.");
  return false;
}

/**
 * Displays a message on the LCD for a specific duration.
 * This function ensures both lines are truncated to LCD_COLS (16) characters.
 * @param {String} line1 The message for the first line.
 * @param {String} line2 The message for the second line.
 * @param {int} delayMs The duration to display the message in milliseconds.
 */
void displayMessage(String line1, String line2 = "", int delayMs) {
  // Truncate line1 to fit display
  if (line1.length() > LCD_COLS) {
    line1 = line1.substring(0, LCD_COLS);
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);

  if (line2.length() > 0) {
    // Truncate line2 to fit display
    if (line2.length() > LCD_COLS) {
      line2 = line2.substring(0, LCD_COLS);
    }
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
  
  if (delayMs > 0) {
    lastDisplayTime = millis();
    displayActive = true;
  }
}

/**
 * Generates a tone on the buzzer.
 * @param {int} duration The duration of the buzz in milliseconds.
 * @param {int} frequency The frequency of the tone in Hz.
 */
void buzz(int duration, int frequency) {
  tone(BUZZER_PIN, frequency, duration);
}

/**
 * Syncs the ESP8266's time with an NTP server.
 */
void syncTime() {
  displayMessage("Syncing time...", "");
  timeClient.begin();
  if (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  timeClient.end();
  displayMessage("Time Synced!", "");
}
