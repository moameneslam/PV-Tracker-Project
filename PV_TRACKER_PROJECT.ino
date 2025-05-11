#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// Motor control pins
#define steppinh 12
#define dirpinh  13
#define steppinv 27
#define dirpinv  14
#define LDR1     32  // Top-left LDR
#define LDR2     33  // Top-right LDR
#define LDR3     34  // Bottom-left LDR
#define LDR4     35  // Bottom-right LDR
#define steprevo 200

// Auto tracking parameters
#define LDR_THRESHOLD 30       // Threshold for LDR difference to trigger movement
#define TRACKING_STEPS 50       // Steps to move each tracking adjustment
#define TRACKING_INTERVAL 1000 // Auto tracking interval in milliseconds

// Tracking control variables
bool autoTrackingEnabled = false;
unsigned long lastTrackingTime = 0;
int ldrValues[4] = {0, 0, 0, 0};  // Values from the four LDRs

// INA219 Sensor
Adafruit_INA219 ina219;
bool sensorInitialized = false;

// Preferences for storing motor positions
Preferences preferences;

// Initialize Functions
void forwardx(int steps);
void backwardx(int steps);
void forwardy(int steps);
void backwardy(int steps);
void performAutoTracking();
void readLDRs();

// Motor positions
int motorXPosition = 0;
int motorYPosition = 0;

// Sensor values - with fallback/test values to ensure data flows
float loadvoltage = 5.0;  // Default test value
float current_mA = 100.0; // Default test value
float power_mW = 500.0;   // Default test value

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// WiFi AP credentials
const char* ap_ssid = "PV_Tracker_AP";
const char* ap_password = "12345678";

// HTML with diagnostic tools and auto tracking controls
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>PV Tracker Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; }
        .dashboard { max-width: 1000px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        .header { text-align: center; margin-bottom: 30px; }
        .connection-status { text-align: center; padding: 10px; margin-bottom: 20px; border-radius: 5px; }
        .connected { background-color: #d4edda; color: #155724; }
        .disconnected { background-color: #f8d7da; color: #721c24; }
        .sensor-container { display: flex; flex-wrap: wrap; gap: 20px; margin-bottom: 30px; }
        .sensor-card { flex: 1; min-width: 200px; background-color: #f0f8ff; padding: 15px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .motor-container { display: flex; gap: 20px; margin-bottom: 30px; }
        .motor-card { flex: 1; background-color: #fffaf0; padding: 15px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .sensor-value { font-size: 24px; font-weight: bold; margin: 10px 0; color: #2c3e50; }
        .sensor-unit { color: #7f8c8d; }
        .controls { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; margin-top: 20px; }
        button { padding: 10px 20px; background-color: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; }
        button:hover { background-color: #0056b3; }
        button.active { background-color: #28a745; }
        .debug-area { background-color: #f8f9fa; padding: 15px; border-radius: 8px; margin-top: 20px; overflow-y: auto; height: 200px; font-family: monospace; }
        .last-updated { text-align: right; color: #7f8c8d; font-size: 12px; margin-top: 20px; }
        .ldr-grid { display: grid; grid-template-columns: 1fr 1fr; grid-template-rows: 1fr 1fr; gap: 10px; margin: 20px auto; max-width: 300px; }
        .ldr-sensor { background-color: #e9ecef; text-align: center; padding: 15px; border-radius: 8px; }
        .ldr-value { font-size: 20px; font-weight: bold; }
        .arrow-controls { display: grid; grid-template-columns: 1fr 1fr 1fr; grid-template-rows: 1fr 1fr 1fr; gap: 5px; margin: 20px auto; max-width: 200px; }
        .arrow-btn { padding: 10px; text-align: center; font-size: 18px; }
        .empty-cell { background-color: transparent; }
        .ap-info { background-color: #e2e3e5; padding: 10px; border-radius: 5px; margin-bottom: 20px; text-align: center; }
    </style>
</head>
<body>
    <div class="ap-info">
            <h3>Connected to PV Tracker Access Point</h3>
            <p>SSID: PV_Tracker_AP | Password: 12345678</p>
        </div>
        
        <div class="header">
            <h1>PV Tracker Dashboard</h1>
            <p>Real-time monitoring of solar panel tracking system</p>
        </div>


        <div id="connectionStatus" class="connection-status connected">
            Connected to ESP32
        </div>

        <div class="sensor-container">
            <div class="sensor-card">
                <h3>Voltage</h3>
                <div class="sensor-value" id="voltage">--</div>
                <div class="sensor-unit">Volts</div>
            </div>
            <div class="sensor-card">
                <h3>Current</h3>
                <div class="sensor-value" id="current">--</div>
                <div class="sensor-unit">mA</div>
            </div>
            <div class="sensor-card">
                <h3>Power</h3>
                <div class="sensor-value" id="power">--</div>
                <div class="sensor-unit">mW</div>
            </div>
        </div>

        <h3>Light Sensors (LDRs)</h3>
        <div class="ldr-grid">
            <div class="ldr-sensor">
                <div>Top Left</div>
                <div class="ldr-value" id="ldr1">--</div>
            </div>
            <div class="ldr-sensor">
                <div>Top Right</div>
                <div class="ldr-value" id="ldr2">--</div>
            </div>
            <div class="ldr-sensor">
                <div>Bottom Left</div>
                <div class="ldr-value" id="ldr3">--</div>
            </div>
            <div class="ldr-sensor">
                <div>Bottom Right</div>
                <div class="ldr-value" id="ldr4">--</div>
            </div>
        </div>

        <div class="motor-container">
            <div class="motor-card">
                <h3>Motor X Position</h3>
                <div class="sensor-value" id="motorX">--</div>
                <div class="sensor-unit">steps</div>
            </div>
            <div class="motor-card">
                <h3>Motor Y Position</h3>
                <div class="sensor-value" id="motorY">--</div>
                <div class="sensor-unit">steps</div>
            </div>
        </div>

        <h3>Manual Control</h3>
        <div class="arrow-controls">
            <div class="empty-cell"></div>
            <button class="arrow-btn" id="upBtn">&uarr;</button>
            <div class="empty-cell"></div>
            <button class="arrow-btn" id="leftBtn">&larr;</button>
            <div class="empty-cell"></div>
            <button class="arrow-btn" id="rightBtn">&rarr;</button>
            <div class="empty-cell"></div>
            <button class="arrow-btn" id="downBtn">&darr;</button>
            <div class="empty-cell"></div>
        </div>

        <div class="controls">
            <button id="autoTrackBtn">Enable Auto Tracking</button>
            <button id="refreshBtn">Refresh Data</button>
            <button id="resetBtn">Reset Positions</button>
            <button id="testValuesBtn">Use Test Values</button>
        </div>

        <h3>Debug Information</h3>
        <div class="debug-area" id="debugArea">
            Waiting for WebSocket messages...
        </div>

        <div class="last-updated" id="lastUpdated">
            Last updated: --
        </div>
    </div>

    <script>
        // WebSocket connection
        let socket;
        let reconnectAttempts = 0;
        const maxReconnectAttempts = 5;
        let autoTrackingEnabled = false;

        function log(message) {
            const debugArea = document.getElementById('debugArea');
            const timestamp = new Date().toLocaleTimeString();
            debugArea.innerHTML += `<div>[${timestamp}] ${message}</div>`;
            debugArea.scrollTop = debugArea.scrollHeight;
        }

        function connectWebSocket() {
            log("Attempting to connect to WebSocket...");
            
            // Connect to WebSocket on the same host
            socket = new WebSocket(`ws://${window.location.host}/ws`);

            socket.onopen = function(e) {
                log("✅ Connected to ESP32 WebSocket");
                updateConnectionStatus(true);
                reconnectAttempts = 0;
                // Request initial data
                socket.send(JSON.stringify({command: "refresh"}));
            };

            socket.onmessage = function(event) {
                try {
                    log(`📥 Received data: ${event.data}`);
                    const data = JSON.parse(event.data);
                    updateDashboard(data);
                } catch (error) {
                    log(`❌ Error parsing message: ${error.message}`);
                    log(`Raw message: ${event.data}`);
                }
            };

            socket.onclose = function(event) {
                log(`❌ WebSocket disconnected, code: ${event.code}`);
                updateConnectionStatus(false);
                
                reconnectAttempts++;
                if (reconnectAttempts < maxReconnectAttempts) {
                    const delay = reconnectAttempts * 1000;
                    log(`Reconnecting in ${delay/1000} seconds... (Attempt ${reconnectAttempts}/${maxReconnectAttempts})`);
                    setTimeout(connectWebSocket, delay);
                } else {
                    log("Maximum reconnection attempts reached. Please refresh the page.");
                }
            };

            socket.onerror = function(error) {
                log(`⚠️ WebSocket Error`);
                updateConnectionStatus(false);
            };
        }

        function updateDashboard(data) {
            if (data.voltage !== undefined) {
                document.getElementById('voltage').textContent = data.voltage.toFixed(2);
            }
            if (data.current !== undefined) {
                document.getElementById('current').textContent = data.current.toFixed(2);
            }
            if (data.power !== undefined) {
                document.getElementById('power').textContent = data.power.toFixed(2);
            }
            if (data.motorX !== undefined) {
                document.getElementById('motorX').textContent = data.motorX;
            }
            if (data.motorY !== undefined) {
                document.getElementById('motorY').textContent = data.motorY;
            }
            if (data.ldr1 !== undefined) {
                document.getElementById('ldr1').textContent = data.ldr1;
            }
            if (data.ldr2 !== undefined) {
                document.getElementById('ldr2').textContent = data.ldr2;
            }
            if (data.ldr3 !== undefined) {
                document.getElementById('ldr3').textContent = data.ldr3;
            }
            if (data.ldr4 !== undefined) {
                document.getElementById('ldr4').textContent = data.ldr4;
            }
            if (data.autoTracking !== undefined) {
                autoTrackingEnabled = data.autoTracking;
                updateAutoTrackButton();
            }

            document.getElementById('lastUpdated').textContent = `Last updated: ${new Date().toLocaleTimeString()}`;
        }

        function updateConnectionStatus(connected) {
            const statusElement = document.getElementById('connectionStatus');
            statusElement.textContent = connected ? "Connected to ESP32" : "Disconnected - Reconnecting...";
            statusElement.className = `connection-status ${connected ? 'connected' : 'disconnected'}`;
        }

        function updateAutoTrackButton() {
            const btn = document.getElementById('autoTrackBtn');
            if (autoTrackingEnabled) {
                btn.textContent = "Disable Auto Tracking";
                btn.classList.add('active');
            } else {
                btn.textContent = "Enable Auto Tracking";
                btn.classList.remove('active');
            }
        }

        document.getElementById('refreshBtn').addEventListener('click', () => {
            if (socket?.readyState === WebSocket.OPEN) {
                log("🔄 Sending refresh command");
                socket.send(JSON.stringify({command: "refresh"}));
            } else {
                log("❌ Cannot send refresh - WebSocket not connected");
            }
        });

        document.getElementById('testValuesBtn').addEventListener('click', () => {
            if (socket?.readyState === WebSocket.OPEN) {
                log("🧪 Requesting test values");
                socket.send(JSON.stringify({command: "test_values"}));
            } else {
                log("❌ Cannot send test command - WebSocket not connected");
            }
        });

        document.getElementById('resetBtn').addEventListener('click', () => {
            if (socket?.readyState === WebSocket.OPEN) {
                log("🔄 Sending reset command");
                socket.send(JSON.stringify({command: "reset"}));
                // Request updated data after reset
                setTimeout(() => {
                    socket.send(JSON.stringify({command: "refresh"}));
                }, 500);
            } else {
                log("❌ Cannot send reset - WebSocket not connected");
            }
        });

        document.getElementById('autoTrackBtn').addEventListener('click', () => {
            if (socket?.readyState === WebSocket.OPEN) {
                const newState = !autoTrackingEnabled;
                log(`${newState ? "🔄 Enabling" : "⏹️ Disabling"} auto tracking`);
                socket.send(JSON.stringify({
                    command: "set_auto_tracking",
                    enabled: newState
                }));
            } else {
                log("❌ Cannot toggle auto tracking - WebSocket not connected");
            }
        });

        // Manual control buttons
        document.getElementById('upBtn').addEventListener('click', () => {
            if (socket?.readyState === WebSocket.OPEN) {
                log("⬆️ Moving up");
                socket.send(JSON.stringify({command: "move", direction: "up", steps: 10}));
            }
        });

        document.getElementById('downBtn').addEventListener('click', () => {
            if (socket?.readyState === WebSocket.OPEN) {
                log("⬇️ Moving down");
                socket.send(JSON.stringify({command: "move", direction: "down", steps: 10}));
            }
        });

        document.getElementById('leftBtn').addEventListener('click', () => {
            if (socket?.readyState === WebSocket.OPEN) {
                log("⬅️ Moving left");
                socket.send(JSON.stringify({command: "move", direction: "left", steps: 10}));
            }
        });

        document.getElementById('rightBtn').addEventListener('click', () => {
            if (socket?.readyState === WebSocket.OPEN) {
                log("➡️ Moving right");
                socket.send(JSON.stringify({command: "move", direction: "right", steps: 10}));
            }
        });

        // Initialize connection when page loads
        window.addEventListener('load', () => {
            log("Page loaded, connecting to WebSocket");
            connectWebSocket();
        });
        
        // Setup periodic refresh
        setInterval(() => {
            if (socket?.readyState === WebSocket.OPEN) {
                log("🔄 Auto-refresh");
                socket.send(JSON.stringify({command: "refresh"}));
            }
        }, 5000);
    </script>
</body>
</html>)rawliteral";

void notifyClients() {
    // Create a JSON document
    StaticJsonDocument<512> jsonDoc;
    
    // Add sensor data
    jsonDoc["voltage"] = loadvoltage;
    jsonDoc["current"] = current_mA;
    jsonDoc["power"] = power_mW;
    jsonDoc["motorX"] = motorXPosition;
    jsonDoc["motorY"] = motorYPosition;
    
    // Add LDR values
    jsonDoc["ldr1"] = ldrValues[0];
    jsonDoc["ldr2"] = ldrValues[1];
    jsonDoc["ldr3"] = ldrValues[2];
    jsonDoc["ldr4"] = ldrValues[3];
    
    // Add auto tracking status
    jsonDoc["autoTracking"] = autoTrackingEnabled;

    // Serialize to JSON string
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    
    // Print the JSON string to Serial for debugging
    Serial.print("Sending JSON: ");
    Serial.println(jsonString);
    
    // Send to all WebSocket clients
    ws.textAll(jsonString);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        // Null-terminate the received data
        data[len] = 0;
        String message = String((char*)data);
        Serial.print("Received WebSocket message: ");
        Serial.println(message);
        
        // Parse JSON message
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, message);
        
        if (error) {
            Serial.print("JSON parse error: ");
            Serial.println(error.c_str());
            return;
        }
        
        // Handle commands
        if (doc.containsKey("command")) {
            String command = doc["command"];
            Serial.print("Processing command: ");
            Serial.println(command);
            
            if (command == "refresh") {
                readSensorData();
                readLDRs();
                notifyClients();
            } 
            else if (command == "test_values") {
                // Set test values
                loadvoltage = 12.34;
                current_mA = 567.89;
                power_mW = 1234.56;
                ldrValues[0] = 800;
                ldrValues[1] = 850;
                ldrValues[2] = 820;
                ldrValues[3] = 830;
                Serial.println("Using test values");
                notifyClients();
            }
            else if (command == "reset") {
                motorXPosition = 0;
                motorYPosition = 0;
                preferences.putInt("motorX", motorXPosition);
                preferences.putInt("motorY", motorYPosition);
                Serial.println("Motors reset to position 0");
                notifyClients();
            }
            else if (command == "set_auto_tracking") {
                autoTrackingEnabled = doc["enabled"];
                Serial.print("Auto tracking set to: ");
                Serial.println(autoTrackingEnabled ? "ENABLED" : "DISABLED");
                notifyClients();
            }
            else if (command == "move") {
                String direction = doc["direction"];
                int steps = doc["steps"];
                
                if (direction == "up") {
                    forwardy(10*steps);
                }
                else if (direction == "down") {
                    backwardy(10*steps);
                }
                else if (direction == "left") {
                    backwardx(10*steps);
                }
                else if (direction == "right") {
                    forwardx(10*steps);
                }
                
                Serial.print("Moved ");
                Serial.print(direction);
                Serial.print(" by ");
                Serial.print(steps);
                Serial.println(" steps");
                
                notifyClients();
            }
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            // Send initial data when client connects
            readSensorData();
            readLDRs();
            notifyClients();
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void initWebSocket() {
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}

void readSensorData() {
    // Try to read from INA219 sensor
    if (sensorInitialized) {
        try {
            float shuntvoltage = ina219.getShuntVoltage_mV();
            float busvoltage = ina219.getBusVoltage_V();
            current_mA = ina219.getCurrent_mA();
            power_mW = ina219.getPower_mW();
            loadvoltage = busvoltage + (shuntvoltage / 1000);
            
            Serial.println("Sensor readings:");
            Serial.print("  Voltage: "); Serial.println(loadvoltage);
            Serial.print("  Current: "); Serial.println(current_mA);
            Serial.print("  Power: "); Serial.println(power_mW);
        }
        catch (...) {
            Serial.println("Error reading from INA219 sensor");
            // Keep using previous or default values
        }
    }
    else {
        Serial.println("INA219 not initialized, using default values");
    }
}

void readLDRs() {
    // Read values from all four LDRs
    ldrValues[0] = analogRead(LDR1);  // Top-left
    ldrValues[1] = analogRead(LDR2);  // Top-right
    ldrValues[2] = analogRead(LDR3);  // Bottom-left
    ldrValues[3] = analogRead(LDR4);  // Bottom-right
    
    Serial.println("LDR readings:");
    Serial.print("  Top-left (LDR1): "); Serial.println(ldrValues[0]);
    Serial.print("  Top-right (LDR2): "); Serial.println(ldrValues[1]);
    Serial.print("  Bottom-left (LDR3): "); Serial.println(ldrValues[2]);
    Serial.print("  Bottom-right (LDR4): "); Serial.println(ldrValues[3]);
}

void performAutoTracking() {
    // If auto tracking is not enabled, return
    if (!autoTrackingEnabled) {
        return;
    }
    
    // Read current LDR values
    readLDRs();
    
    // Calculate average values for different directions
    int topAvg = (ldrValues[0] + ldrValues[1]) / 2;     // Top (LDR1 + LDR2)
    int bottomAvg = (ldrValues[2] + ldrValues[3]) / 2;  // Bottom (LDR3 + LDR4)
    int leftAvg = (ldrValues[0] + ldrValues[2]) / 2;    // Left (LDR1 + LDR3)
    int rightAvg = (ldrValues[1] + ldrValues[3]) / 2;   // Right (LDR2 + LDR4)
    
    // Determine vertical movement (Y-axis)
    int verticalDiff = topAvg - bottomAvg;
    if (abs(verticalDiff) > LDR_THRESHOLD) {
        if (verticalDiff > 0) {
            // Top is brighter than bottom, move up
            Serial.println("Auto-tracking: Moving up");
            forwardy(TRACKING_STEPS);
        } else {
            // Bottom is brighter than top, move down
            Serial.println("Auto-tracking: Moving down");
            backwardy(TRACKING_STEPS);
        }
    }
    
    // Determine horizontal movement (X-axis)
    int horizontalDiff = leftAvg - rightAvg;
    if (abs(horizontalDiff) > LDR_THRESHOLD) {
        if (horizontalDiff > 0) {
            // Left is brighter than right, move left
            Serial.println("Auto-tracking: Moving left");
            backwardx(TRACKING_STEPS);
        } else {
            // Right is brighter than left, move right
            Serial.println("Auto-tracking: Moving right");
            forwardx(TRACKING_STEPS);
        }
    }
    
    // If any movement was made, update clients
    if (abs(verticalDiff) > LDR_THRESHOLD || abs(horizontalDiff) > LDR_THRESHOLD) {
        notifyClients();
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000); // Wait for serial for up to 5 seconds
    
    Serial.println("\n\n=== PV Tracker with Auto Tracking ===");
    
    // Initialize INA219 sensor
    Serial.println("Initializing INA219 sensor...");
    if (ina219.begin()) {
        Serial.println("INA219 sensor initialized successfully");
        sensorInitialized = true;
    } else {
        Serial.println("Failed to initialize INA219 sensor - will use default values");
        sensorInitialized = false;
    }
    
    // Initialize LDR pins
    pinMode(LDR1, INPUT);
    pinMode(LDR2, INPUT);
    pinMode(LDR3, INPUT);
    pinMode(LDR4, INPUT);
    Serial.println("LDR pins initialized");

    // Initialize motor control pins
    pinMode(steppinh, OUTPUT);
    pinMode(dirpinh, OUTPUT);
    pinMode(steppinv, OUTPUT);
    pinMode(dirpinv, OUTPUT);
    Serial.println("Motor control pins initialized");

    // Initialize preferences
    preferences.begin("motor-positions", false);
    motorXPosition = preferences.getInt("motorX", 0);
    motorYPosition = preferences.getInt("motorY", 0);
    Serial.println("Preferences initialized");
    Serial.print("Loaded X position: ");
    Serial.println(motorXPosition);
    Serial.print("Loaded Y position: ");
    Serial.println(motorYPosition);

    // Create Access Point
    Serial.println("Creating WiFi Access Point...");
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("Access Point created with IP: ");
    Serial.println(WiFi.softAPIP());

    // Initialize WebSocket
    initWebSocket();
    Serial.println("WebSocket initialized");

    // Serve HTML page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Serving HTML page");
        request->send_P(200, "text/html", index_html);
    });

    // Start server
    server.begin();
    Serial.println("HTTP server started");
    Serial.println("System ready!");
    
    // Initial LDR reading
    readLDRs();
}

void loop() {
    ws.cleanupClients();
    
    // Handle auto tracking if enabled
    unsigned long currentMillis = millis();
    if (autoTrackingEnabled && (currentMillis - lastTrackingTime >= TRACKING_INTERVAL)) {
        lastTrackingTime = currentMillis;
        performAutoTracking();
    }
    
    delay(100);
}

void forwardx(int steps) {
    digitalWrite(dirpinh, HIGH);
    for (int x = 0; x < steps; x++) {
        digitalWrite(steppinh, HIGH);
        delayMicroseconds(2000);
        digitalWrite(steppinh, LOW);
        delayMicroseconds(2000);
    }
    motorXPosition += steps;
    preferences.putInt("motorX", motorXPosition);
}

void forwardy(int steps) {
    digitalWrite(dirpinv, HIGH);
    for (int x = 0; x < steps; x++) {
        digitalWrite(steppinv, HIGH);
        delayMicroseconds(2000);
        digitalWrite(steppinv, LOW);
        delayMicroseconds(2000);
    }
    motorYPosition += steps;
    preferences.putInt("motorY", motorYPosition);
}

void backwardx(int steps) {
    digitalWrite(dirpinh, LOW);
    for (int x = 0; x < steps; x++) {
        digitalWrite(steppinh, HIGH);
        delayMicroseconds(2000);
        digitalWrite(steppinh, LOW);
        delayMicroseconds(2000);
    }
    motorXPosition -= steps;
    preferences.putInt("motorX", motorXPosition);
}

void backwardy(int steps) {
    digitalWrite(dirpinv, LOW);
    for (int x = 0; x < steps; x++) {
        digitalWrite(steppinv, HIGH);
        delayMicroseconds(2000);
        digitalWrite(steppinv, LOW);
        delayMicroseconds(2000);
    }
    motorYPosition -= steps;
    preferences.putInt("motorY", motorYPosition);
}