# PV-Tracker Project

An ESP32-based solar panel tracking system with web interface that optimizes energy capture by dynamically following the sun's position.

![PV Tracker System - Main Image](/images/PVTrackerSystem.png "PV Tracker System")

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Hardware Components](#hardware-components)
- [Wiring Diagram](#wiring-diagram)
- [Installation](#installation)
- [User Interface](#user-interface)
- [How It Works](#how-it-works)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

## Overview

The PV-Tracker Project is an ESP32-based solar tracking system designed to maximize the efficiency of photovoltaic panels by continuously aligning them with the sun's position. The system uses light-dependent resistors (LDRs) to detect the sun's location and stepper motors to adjust the panel's orientation in real-time. It features a responsive web interface accessible via WiFi, allowing users to monitor system performance and control the tracker manually or automatically.

## Features

- **Dual-axis tracking**: Automatically aligns solar panels in both horizontal and vertical axes
- **Light-dependent resistor (LDR) sensing**: Uses four LDRs to detect optimal sun position
- **Stepper motor control**: Precise movement control for panel positioning
- **Web-based control interface**: Monitor and control your system from any device with a web browser
- **Automatic and manual tracking modes**: Choose between automatic sun-following or manual positioning
- **Real-time power monitoring**: Track voltage, current, and power output via INA219 sensor
- **Energy-efficient**: Optimizes solar energy capture compared to fixed panels
- **Position memory**: Remembers motor positions even after power cycling
- **WiFi access point**: Creates its own network for direct connection without requiring internet

## Hardware Components

- ESP32 Microcontroller
- 2 x Stepper Motors with drivers
- 4 x Light Dependent Resistors (LDRs)
- 4 x Resistors (10kΩ recommended for LDR voltage dividers)
- INA219 Current/Voltage Sensor
- Power supply
- Solar panel mounting hardware
- Connecting wires
- Panel frame and mounting base

## Wiring Diagram

| Component | ESP32 Pin |
|-----------|-----------|
| Horizontal Stepper STEP | GPIO 12 |
| Horizontal Stepper DIR | GPIO 13 |
| Vertical Stepper STEP | GPIO 27 |
| Vertical Stepper DIR | GPIO 14 |
| Top-left LDR | GPIO 32 |
| Top-right LDR | GPIO 33 |
| Bottom-left LDR | GPIO 34 |
| Bottom-right LDR | GPIO 35 |
| INA219 | I2C (default SDA/SCL) |

## Installation

1. Clone this repository:
```
git clone https://github.com/moameneslam/PV-Tracker-Project.git
```

2. Open the `PV_TRACKER_PROJECT.ino` file in the Arduino IDE.

3. Install the required libraries through the Arduino Library Manager:
   - ESP32 Board Support Package
   - AsyncTCP
   - ESPAsyncWebServer
   - Wire
   - Adafruit_INA219
   - ArduinoJson

4. Connect the hardware components according to the wiring diagram.

5. Upload the sketch to your ESP32 board.

6. Connect to the WiFi access point created by the ESP32:
   - SSID: `PV_Tracker_AP`
   - Password: `12345678`

7. Navigate to `192.168.4.1` in your web browser to access the control interface.

## User Interface

The PV-Tracker features a responsive web interface accessible via WiFi. Here's what you'll find:

### Dashboard Overview
![Dashboard Overview](/images/Dashboard.png "Dashboard Overview")

The main dashboard displays real-time system information including:
- Current, voltage, and power readings
- LDR sensor values
- Motor positions
- Connection status

### Control Panel
![Control Panel](/images/Control.png "Control Panel")

The control panel allows you to:
- Enable/disable automatic tracking
- Manually adjust panel position with directional controls
- Reset motor positions
- Refresh sensor data

### Debug Information
![Debug Information](/images/Debug.png "Debug Information")

A debug area shows system logs and WebSocket communication for troubleshooting.

## How It Works

### Automatic Sun Tracking

The system uses four LDRs arranged in a grid pattern to detect the direction of maximum sunlight:

1. The system reads values from all four LDRs
2. It compares the average readings from different directions:
   - Top vs Bottom for vertical tracking
   - Left vs Right for horizontal tracking
3. If the difference exceeds the threshold (`LDR_THRESHOLD`), the system moves the panel toward the brighter direction
4. This process repeats at regular intervals (defined by `TRACKING_INTERVAL`)

### Motor Control

The system uses stepper motors for precise positioning:
- Each axis (horizontal and vertical) has its own stepper motor
- Motor positions are tracked and stored in non-volatile memory
- Movement can be triggered automatically or manually via the web interface

### Power Monitoring

An INA219 sensor monitors the electrical output of the solar panel:
- Bus voltage
- Current draw
- Power output

This data is displayed in real-time on the web interface.

## Configuration

You can adjust several parameters in the code to fine-tune the system:

```cpp
// Auto tracking parameters
#define LDR_THRESHOLD 30        // Threshold for LDR difference to trigger movement
#define TRACKING_STEPS 50       // Steps to move each tracking adjustment
#define TRACKING_INTERVAL 1000  // Auto tracking interval in milliseconds
```

Other configurable settings:
- WiFi access point SSID and password
- Motor speed (by adjusting delayMicroseconds values)
- Web interface refresh rate

## Troubleshooting

### Connection Issues
- Ensure you're connected to the correct WiFi network (`PV_Tracker_AP`)
- Try refreshing the page
- Check the ESP32's power supply

### Tracking Issues
- Verify all LDRs are working correctly (check values in web interface)
- Adjust `LDR_THRESHOLD` if the system is too sensitive or not sensitive enough
- Ensure stepper motors are properly wired and powered

### Sensor Issues
- If the INA219 sensor isn't detected, verify I2C connections
- The system will use default values if the sensor isn't available

## Contributing

Contributions to improve the PV-Tracker Project are welcome:

1. Fork the repository
2. Create a new branch (`git checkout -b feature/your-feature`)
3. Make your changes
4. Commit your changes (`git commit -m 'Add some feature'`)
5. Push to the branch (`git push origin feature/your-feature`)
6. Open a Pull Request


## Author

Moamen Eslam - [GitHub Profile](https://github.com/moameneslam)

## Acknowledgments

- Special thanks to all contributors
- Inspired by various solar tracking designs from the Arduino community
