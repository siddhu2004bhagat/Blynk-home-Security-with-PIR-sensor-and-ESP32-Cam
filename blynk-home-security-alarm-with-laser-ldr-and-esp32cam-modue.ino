// Blynk Laser Security System - Integrated into the standard ESP32 CameraWebServer structure
//
// NOTE: This sketch MUST be placed in a folder that contains the supporting files 
//       from the CameraWebServer example: app_httpd.cpp, camera_index.h, and camera_pins.h
//
// Viral Science www.youtube.com/c/viralscience  www.viralsciencecreativity.com

// --- FIX: BLYNK TEMPLATE DEFINITIONS MOVED TO TOP ---
#define BLYNK_TEMPLATE_ID "TMPL3teSEP15W"
#define BLYNK_TEMPLATE_NAME "Home Laser Security"
#define BLYNK_FIRMWARE_VERSION "1.0.0"

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h> // Ensure you have the Blynk library installed

// ===========================
// Select camera model in board_config.h - **NOTE: This is now handled by INCLUDING board_config.h**
// ===========================
#include "board_config.h" 

// --- BLYNK VIRTUAL PIN DEFINITIONS (Updated) ---
#define BLYNK_PHOTO_BUTTON V0  // Button to manually take a photo (App to Device)
#define BLYNK_IMAGE_GALLERY V1 // Image Gallery to display captured photo (Device to App)
#define BLYNK_LASER_SWITCH V2  // Switch to turn the Laser ON/OFF (App to Device)
#define BLYNK_STATUS_LABEL V3  // Widget Box/Label to show "Intruder Alert!" or "System Armed" (Device to App)
#define BLYNK_PIR_SWITCH V4    // New: Switch to enable/disable the PIR sensor (App to Device)
#define BLYNK_PIR_STATUS V5    // New: Widget Box/Label to show PIR status (Enabled/Disabled) (Device to App)

// --- HARDWARE PIN DEFINITIONS (User Components) (Updated) ---
#define LDR 13     // Input: LDR Sensor (Digital Pin - Check wiring for internal pullup/down)
#define Laser 12   // Output: Laser Module (Controlled by V2 button)
#define BUZZER 2   // Output: Passive/Active Buzzer
#define PIR_SENSOR 14 // New: Input Pin for PIR Sensor Data (GPIO 14 is often available)

// Note: LED (Flash) control is typically handled internally by the camera server code

// ===========================
// Enter your WiFi credentials and BLYNK AUTH TOKEN
// ===========================
const char *ssid = "Siddhu";
const char *password = "12345671";
char auth[] = "4PUj6-ILI2E8p_b198oAx1-nh3ehh58s";  // <<< PASTE YOUR 32-CHARACTER AUTH TOKEN HERE >>>

String local_IP;

// --- GLOBAL VARIABLES (New) ---
bool pirEnabled = false; // To store the state of the PIR switch from Blynk

// Forward declarations for functions defined in app_httpd.cpp and the custom code
void startCameraServer();
void setupLedFlash(); 

// --- BUZZER FUNCTION ---
void beep(int duration_ms) {
  digitalWrite(BUZZER, HIGH);
  delay(duration_ms);
  digitalWrite(BUZZER, LOW);
}

// --- PHOTO CAPTURE FUNCTION ---
void takePhoto()
{
  // The official setupLedFlash function uses a LED_GPIO_NUM define, but the 
  // original code used GPIO 4, which is often the Flash LED. We will use the 
  // official check but also support the original pin 4 if needed.
#if defined(LED_GPIO_NUM)
  digitalWrite(LED_GPIO_NUM, HIGH); // Turn Flash ON
#else
  digitalWrite(4, HIGH); 
#endif
  
  delay(200);
  uint32_t randomNum = random(50000);
  // URL to capture a still image from the running web server
  String imageUrl = "http://" + local_IP + "/capture?_cb=" + (String)randomNum; 

  // Send the URL to the Image Gallery widget on V1
  Blynk.setProperty(BLYNK_IMAGE_GALLERY, "urls", imageUrl);
  Serial.println("Photo Captured: " + imageUrl);
  
#if defined(LED_GPIO_NUM)
  digitalWrite(LED_GPIO_NUM, LOW); // Turn Flash OFF
#else
  digitalWrite(4, LOW);
#endif
  delay(1000);
}

// --- ALERT FUNCTION (New/Refactored) ---
void triggerAlert(String source) {
    Serial.println("!!! INTRUDER DETECTED via " + source + " !!!");

    // 1. Local Alert (Beep)
    beep(500);

    // 2. Blynk App Event Logging & Notification
    Blynk.logEvent("intruder_alarm", "Intruder detected via " + source);

    // 3. Update Blynk Status Label (V3)
    Blynk.virtualWrite(BLYNK_STATUS_LABEL, "INTRUDER ALERT!");
    Blynk.setProperty(BLYNK_STATUS_LABEL, "color", "#FF0000"); // Red color for alert

    // 4. Capture Photo and send to Image Gallery (V1)
    takePhoto();

    // Debounce/Cool-down period before checking again
    delay(3000); 
    
    // Reset status label after delay (if still armed)
    if(digitalRead(Laser) == HIGH){
        Blynk.virtualWrite(BLYNK_STATUS_LABEL, "System Armed");
        Blynk.setProperty(BLYNK_STATUS_LABEL, "color", "#3CB371"); // Green color
    }
}


// --- BLYNK WRITE HANDLERS (App to Device) (Updated) ---

// V0: Manual Photo Button
BLYNK_WRITE(BLYNK_PHOTO_BUTTON) {
  if (param.asInt() == 1) { // Only trigger on push (HIGH)
    Serial.println("Manual photo trigger from V0.");
    takePhoto();
  }
}

// V2: Laser ON/OFF Switch
BLYNK_WRITE(BLYNK_LASER_SWITCH) {
  int laserState = param.asInt();
  digitalWrite(Laser, laserState);
  Serial.print("Laser/System Status: ");
  Serial.println(laserState == HIGH ? "ARMED" : "DISARMED");

  // Update the status label (V3) immediately
  if (laserState == HIGH) {
    Blynk.virtualWrite(BLYNK_STATUS_LABEL, "System Armed");
    Blynk.setProperty(BLYNK_STATUS_LABEL, "color", "#3CB371"); // Green
  } else {
    Blynk.virtualWrite(BLYNK_STATUS_LABEL, "System Disarmed");
    Blynk.setProperty(BLYNK_STATUS_LABEL, "color", "#808080"); // Gray
  }
}

// V4: PIR ON/OFF Switch (New Handler)
BLYNK_WRITE(BLYNK_PIR_SWITCH) {
    pirEnabled = param.asInt();
    Serial.print("PIR Sensor Status: ");
    Serial.println(pirEnabled ? "ENABLED" : "DISABLED");

    // Update the PIR status label (V5)
    if (pirEnabled) {
        Blynk.virtualWrite(BLYNK_PIR_STATUS, "PIR Enabled");
        Blynk.setProperty(BLYNK_PIR_STATUS, "color", "#00BFFF"); // Blue
    } else {
        Blynk.virtualWrite(BLYNK_PIR_STATUS, "PIR Disabled");
        Blynk.setProperty(BLYNK_PIR_STATUS, "color", "#808080"); // Gray
    }
}


void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // --- 1. CUSTOM HARDWARE INITIALIZATION (Updated) ---
  pinMode(LDR, INPUT);
  pinMode(Laser, OUTPUT);
  pinMode(BUZZER, OUTPUT); 
  pinMode(PIR_SENSOR, INPUT); // Initialize PIR pin as Input

  // Ensure components are OFF at boot
  digitalWrite(Laser, LOW); 
  digitalWrite(BUZZER, LOW);
  
  // Existing Camera Configuration... (omitted for brevity, assume it's the same)

  // ... (Camera init code remains the same) ...
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // ... (Sensor configuration code remains the same) ...

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif
  
  // --- 2. WI-FI AND SERVER START ---
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  local_IP = WiFi.localIP().toString(); // Store IP for photo URLs
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  
  // --- 3. BLYNK CONNECTION ---
  Blynk.begin(auth, ssid, password);

  // Set initial status in the Blynk app
  Blynk.virtualWrite(BLYNK_STATUS_LABEL, "System Disarmed");
  Blynk.setProperty(BLYNK_STATUS_LABEL, "color", "#808080");
    
    // Set initial PIR status in the Blynk app (DISABLED by default)
    Blynk.virtualWrite(BLYNK_PIR_STATUS, "PIR Disabled");
    Blynk.setProperty(BLYNK_PIR_STATUS, "color", "#808080"); 
}

void loop() {
  Blynk.run();

  // --- LASER SECURITY CHECK ---
  // Checks: 1. Is the laser ON? 2. Is the beam broken? 
  if((digitalRead(Laser) == HIGH) && (digitalRead(LDR) == HIGH)){
        triggerAlert("Laser Tripwire");
  }
    
    // --- PIR MOTION CHECK (New) ---
    // Checks: 1. Is the laser ON (system armed)? 2. Is PIR switch enabled? 3. Is motion detected?
    int pirState = digitalRead(PIR_SENSOR);

    if (digitalRead(Laser) == HIGH && pirEnabled && (pirState == HIGH)) {
        triggerAlert("PIR Motion Sensor");
    }
}