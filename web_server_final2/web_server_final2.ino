#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>  // Include mDNS library
WebServer server(80);
// FreeRTOS timer handle
TimerHandle_t timerHandle;


#define buzzerPin 26
#define relayPin 2


int enteredHours = 0;          // For user input
int enteredMinutes = 0;        // For user input
bool mode = 0;                  // 0 = Loop, 1 = OneTime
bool reset = false;            // Reset state
bool settingsChanged = false;  // Flag for settings change




// Real-time counter values
unsigned int realHours = 0;
unsigned int realMinutes = 0;




uint8_t ButtoninterruptPin = 25;
unsigned long ButtoncurrentTime = 0;

bool timerFlag = false;
bool ledFlag = false;

// Load settings from LittleFS
void loadSettings() {
  if (LittleFS.exists("/settings.json")) {
    File file = LittleFS.open("/settings.json", "r");
    if (file) {
      StaticJsonDocument<256> doc;
      deserializeJson(doc, file);
      enteredHours = doc["hours"];
      enteredMinutes = doc["minutes"];
      mode = doc["mode"];
      Serial.println("settings josn read check");
      file.close();
    }
  }
}

void loadCounter() {
  if (LittleFS.exists("/counter.json")) {
    File file = LittleFS.open("/counter.json", "r");
    if (file) {
      StaticJsonDocument<128> doc;
      deserializeJson(doc, file);
      realHours = doc["realHours"];
      realMinutes = doc["realMinutes"];
      Serial.println("counter josn read check");
      file.close();
    }
  }
}

void loadReset() {
  if (LittleFS.exists("/reset.json")) {
    File file = LittleFS.open("/reset.json", "r");
    if (file) {
      StaticJsonDocument<128> doc;
      deserializeJson(doc, file);
      reset = doc["reset"];
      Serial.println("reset josn read check");
      file.close();
    }
  }
}

void realTimerCounterUpdate() {
  realHours = enteredHours;
  realMinutes = enteredMinutes;
  deleteTimer();
  if(realHours>0 || realMinutes >0){
   Serial.println("Timer start in realhours and realminutes");
  if (loadTimerConfig(mode, realHours, realMinutes)) {
    xTimerStart(timerHandle, 0);  // Start the timer with no delay
  }
  }
  void saveCounter();
}

// Save main settings to LittleFS
void saveSettings() {
  if (settingsChanged) {  // Only save if there was a change
    File file = LittleFS.open("/settings.json", "w");
    if (file) {
      StaticJsonDocument<256> doc;
      doc["hours"] = enteredHours;
      doc["minutes"] = enteredMinutes;
      doc["mode"] = mode;
      doc["reset"] = reset;
      serializeJson(doc, file);
      file.close();
      realTimerCounterUpdate();
      Serial.println("settings josn write check");
      settingsChanged = false;  // Reset the flag
    }
  }
}

// Save reset state to a separate file
void saveResetState() {
  File file = LittleFS.open("/reset.json", "w");
  if (file) {
    StaticJsonDocument<128> doc;
    doc["reset"] = reset;
    serializeJson(doc, file);
    Serial.println("reset josn write check");
    file.close();
  }
}

// Save counter to a separate file every minute
void saveCounter() {
  File file = LittleFS.open("/counter.json", "w");
  if (file) {
    StaticJsonDocument<128> doc;
    doc["realHours"] = realHours;
    doc["realMinutes"] = realMinutes;
    serializeJson(doc, file);
    Serial.println("counter josn write check");
    file.close();
  }
}

// Increment real-time counter every minute and save each time
void updateRealCounter() {
  static unsigned long lastUpdateTime = 0;

  if (millis() - lastUpdateTime >= 60000) {  // 60000 ms = 1 minute

    lastUpdateTime = millis();
   
    if (realMinutes <= 0 && realHours > 0) {
      realMinutes = 59;
      realHours--;
    } else if (realMinutes <= 0) {
      return;
    } else {
    }
    realMinutes--;
    saveCounter();  // Save real-time counter data
  }
}

// HTML page with CSS styling and counter display in the main box
String htmlPage() {
  String html = "<!DOCTYPE html><html><head><title>Timer</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; transform: scale(1.3); transform-origin: center; }";
  html += "h1 { text-align: center; }";
  html += ".container { max-width: 400px; padding: 25px; border: 1px solid #ccc; border-radius: 10px; background-color: #f9f9f9; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); }";
  html += "label { display: block; margin: 10px 0 5px; font-weight: bold; }";
  html += "input[type='number'], select { width: 100%; padding: 10px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 5px; font-size: 1.1em; }";
  html += "input[type='submit'], button { width: 100%; padding: 12px; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 1.1em; margin-bottom: 10px; background-color: #2196F3;}";

  // Change button color based on reset state
  if (reset) {
    html += "button { background-color: #2196F3; }";  // Blue when true
  } else {
    html += "button { background-color: #f44336; }";  // Red when false
  }

  html += "input[type='submit']:hover, button:hover { background-color: #f44336; }";
  html += ".previous-values { text-align: center; margin-top: 20px; font-size: 1.1em; }";
  html += ".counter { font-size: 1.5em; text-align: center; margin-bottom: 20px; padding: 10px; border-radius: 8px; background-color: #e0f7fa; color: #00796b; }";
  html += "</style>";
  html += "<script>setTimeout(function(){ location.reload(); }, 60000);</script>";  // Auto-refresh every 60 seconds
  html += "</head><body>";

  html += "<div class='container'>";  // Start of main box

  // Counter display within the main box
  html += "<div class='counter'><h2>Real-Time Counter</h2>";
  html += "<p>Time left Hours: " + String(realHours) + " | Minutes: " + String(realMinutes) + "</p>";
  html += "</div>";

  html += "<h1>Enter Time</h1>";
  html += "<form action=\"/submit\" method=\"POST\">";
  html += "<label for=\"hours\">Hours:</label><input type=\"number\" id=\"hours\" name=\"hours\" min=\"0\" value=\"" + String(enteredHours) + "\">";
  html += "<label for=\"minutes\">Minutes:</label><input type=\"number\" id=\"minutes\" name=\"minutes\" min=\"0\" max=\"59\" value=\"" + String(enteredMinutes) + "\">";

  // Mode Dropdown
  html += "<label for=\"mode\">Mode:</label>";
  html += "<select id=\"mode\" name=\"mode\">";
  html += "<option value=\"0\"" + String(mode == 0 ? " selected" : "") + ">Loop</option>";
  html += "<option value=\"1\"" + String(mode == 1 ? " selected" : "") + ">OneTime</option>";
  html += "</select>";

  html += "<input type=\"submit\" value=\"Submit\">";
  html += "</form>";

  // Restart Button
  html += "<form action=\"/restart\" method=\"POST\">";
  html += "<button type=\"submit\">Restart</button>";
  html += "</form>";

  // Display Previous Values
  html += "<div class='previous-values'><h2>Previous Values</h2>";
  html += "<p>Entered Hours: " + String(enteredHours) + "</p>";
  html += "<p>Entered Minutes: " + String(enteredMinutes) + "</p>";
  html += "<p>Mode: " + String(mode == 0 ? "Loop" : "OneTime") + "</p>";
  html += "<p>Reset: " + String(reset ? "True" : "False") + "</p>";
  html += "</div></div></body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleSubmit() {
  if (server.hasArg("hours") && server.hasArg("minutes") && server.hasArg("mode")) {
    int newHours = server.arg("hours").toInt();
    int newMinutes = server.arg("minutes").toInt();
    int newMode = server.arg("mode").toInt();

    // Check if any settings have changed
    if (enteredHours != newHours || enteredMinutes != newMinutes || mode != newMode) {
      enteredHours = newHours;
      enteredMinutes = newMinutes;
      mode = newMode;
      settingsChanged = true;  // Mark settings as changed
      saveSettings();
    }
  }
  server.sendHeader("Location", "/", true);
  server.send(303);  // Redirect to home
}

void handleRestart() {
  reset = !reset;          // Toggle reset state
  settingsChanged = true;  // Mark settings as changed for restart state
  saveSettings();          // Save the main settings
  saveResetState();        // Save the reset state
  server.sendHeader("Location", "/", true);
  server.send(303);  // Redirect to home
}

void Buttonbegin() {

  pinMode(ButtoninterruptPin, INPUT_PULLUP);
  // Create a FreeRTOS task to handle the button press logic
  xTaskCreate(Buttonhandle, "ButtonHandler", 2048, nullptr, 2, nullptr);
}

void Buttonhandle(void *parameter) {
  bool lastState = true;
  for (;;) {
    bool currentState = digitalRead(ButtoninterruptPin);
    if (currentState == LOW && lastState) {  // Button pressed
      ButtoncurrentTime = millis();
      lastState = false;
    } else if (currentState == HIGH && lastState == false) {  // Button released

      int buttonTime = millis() - ButtoncurrentTime;
      ButtoncurrentTime = 0;
      uint8_t taskSwitch = buttonTime / 10000;  // Divide by 5000 to get the task switch value directly
                                                // Logger::DEBUG("Master Hardware Button Press : " + String(taskSwitch));
      Buttonaction(taskSwitch);
      lastState = true;
    } else {
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Delay for 10ms to prevent busy-waiting
  }
}
void Buttonaction(uint8_t taskSwitch) {
  if (taskSwitch == 0) {
    Serial.println("Restart TIMER");
    xTimerStart(timerHandle, 0);
    // loadReset();
  } else if (taskSwitch > 3) {
    Serial.println("Restart TIMER");
  } else {
  }
}





void onTimer(TimerHandle_t xTimer) {
  Serial.println("Timer callback triggered!");
  timerFlag = true;
}

bool loadTimerConfig(bool mode1, int hours, int minute) {
  // Read the loop setting from JSON
  bool loopMode = mode1;  // Default to single-shot if not set
  unsigned long timeMs = ((hours * 60 * 60) + (minute * 60)) * 1000;
  Serial.print("time in minute: ");
  Serial.println(timeMs);
  Serial.print("Loop mode: ");
  Serial.print(mode1);
  Serial.println(loopMode ? "Single Execution" : "Enabled");

  // Create the FreeRTOS timer based on loop mode
  timerHandle = xTimerCreate(
    "MyTimer",                    // Timer name
    pdMS_TO_TICKS(timeMs),        // Timer period in ticks (1000 ms)
    loopMode ? pdFALSE : pdTRUE,  // Auto-reload based on loopMode
    (void *)0,                    // Timer ID (not used here)
    onTimer                       // Callback function
  );

  if (timerHandle == NULL) {
    Serial.println("Failed to create timer");
    return false;
  }

  return true;
}

void deleteTimer() {
    if (timerHandle != NULL) {
        xTimerDelete(timerHandle, 0); // Delete the timer
        timerHandle = NULL;           // Clear the handle
    }
}



void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  Buttonbegin();
  loadSettings();
  loadCounter();
  loadReset();
  // Initialize ESP32 as an Access Point
  WiFi.softAP("Timer", "123456789h");
  if (MDNS.begin("timer")) {  // mDNS setup
    Serial.println("mDNS responder started");
  }
  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);
  server.on("/restart", handleRestart);
  server.begin();
  Serial.println("HTTP server started");
  deleteTimer();
  if(realHours>0 || realMinutes >0){
   Serial.println("Timer start in realhours and realminutes");
  if (loadTimerConfig(mode, realHours, realMinutes)) {
    xTimerStart(timerHandle, 0);  // Start the timer with no delay
  }
  }
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
  delay(1000);
}



void loop() {
  server.handleClient();
  updateRealCounter();  // Update the real-time counter
  if (timerFlag) {
    if (!mode) {
      if (ledFlag) {
        digitalWrite(relayPin, LOW);
        ledFlag = false;
        Serial.println("LEDflag relay off");
      } else {
        digitalWrite(relayPin, HIGH);
        ledFlag = true;
        Serial.println("LEDflag relay On");
      }
    }
    else {
      digitalWrite(relayPin, LOW);
      Serial.println("LEDwithout flag relay OFF");
    }
    timerFlag=false;
  }
  delay(10);
}
