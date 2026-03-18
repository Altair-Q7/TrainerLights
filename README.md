```markdown
# TrainerLights

**A WiFi-based reflex training system for athletes**  
Measure and improve reaction time with wireless sensor nodes.

---

## 📖 Overview

TrainerLights is an open-source, low-cost training tool that helps athletes enhance their reflexes. The system consists of one **server** (ESP8266) that creates a WiFi access point and multiple **client lights** (ESP8266 + ultrasonic sensor) that athletes interact with. When a client light turns on, the athlete must move into its detection zone as quickly as possible. The system measures reaction time (ms) and distance (cm), and sends real‑time statistics to a coach’s phone via a built‑in web interface.

No smartphone app is required – just a browser.

---

## ✨ Features

- **Wireless control** – server acts as WiFi access point (`TrainerLights`).
- **Web interface** – accessible from any device with a browser (mobile friendly).
- **Real‑time statistics** – score, errors, average/min/max reaction times and distances.
- **Configurable parameters** – delay before stimulus, timeout, detection range, random/sequential mode.
- **Multiple sensor nodes** – up to 32 clients can connect simultaneously.
- **Sensor health management** – timeouts are tracked; a sensor is removed only after 5 consecutive failures.
- **Persistent configuration** – settings saved in EEPROM (survives power cycle).
- **On‑board button** on each client with three functions (short/medium/long press).
- **Moving average + hysteresis** on distance readings to prevent false triggers.

---

## 🧰 Hardware Requirements

### Server (one unit)
- NodeMCU ESP8266 (or any ESP8266 board)
- Micro‑USB cable
- (Optional) built‑in LED for status

### Each Client (up to 32)
- NodeMCU ESP8266
- HC‑SR04 Ultrasonic Sensor
- 3× LEDs (stimulus, success, error) + appropriate resistors (220–330Ω)
- 1× Tactile push button (pull‑up, connects to GND when pressed)
- Jumper wires
- Breadboard or PCB for assembly

> **Note:** Pin assignments are defined in the code – double‑check before wiring.

---

## 📦 Software Requirements

### Arduino IDE / PlatformIO
Install the following libraries via Arduino Library Manager:

- `ESP8266WiFi` (built‑in with ESP8266 board package)
- `WebSockets` by Markus Sattler
- `ArduinoJson` (version 6)
- `TaskScheduler` by Anatoli Arkhipenko
- `LinkedList` by Ivan Seidel
- `EEPROM` (built‑in)

### ESP8266 Board Package
In Arduino IDE:  
`File` → `Preferences` → Additional Board Manager URLs:  
`http://arduino.esp8266.com/stable/package_esp8266com_index.json`  
Then `Tools` → `Board` → `Board Manager` → search `esp8266` and install.

---

## 🔧 Wiring Diagrams

### Client Node (per light)

| Component      | ESP8266 Pin |
|----------------|-------------|
| HC‑SR04 VCC    | 5V (or 3.3V) |
| HC‑SR04 GND    | GND         |
| HC‑SR04 TRIG   | D1          |
| HC‑SR04 ECHO   | D2          |
| Stimulus LED   | D6 (through resistor) |
| Success LED    | D0 (through resistor) |
| Error LED      | D7 (through resistor) |
| Button (one side to GND) | D5 (INPUT_PULLUP) |
| Built‑in LED   | D4 (on‑board) |

> Use current‑limiting resistors (220Ω) in series with each LED.

### Server Node
No external components required (only built‑in LED on D4 for status).  
The server code does not use the ultrasonic sensor pins; they can be left unconnected.

---

## 🚀 Getting Started

### 1. Prepare the Server
1. Open `TrainerLights_server.ino` in Arduino IDE.
2. Select the correct board (`NodeMCU 1.0 (ESP-12E)`) and port.
3. Upload the sketch to your server NodeMCU.
4. Open Serial Monitor (115200 baud) to see the IP address (usually `192.168.4.1`).

### 2. Prepare Each Client
1. Open `TrainerLights_client.ino`.
2. (Optional) Adjust pin definitions if your wiring differs.
3. Upload the sketch to each client NodeMCU.
4. Clients will automatically attempt to connect to the `TrainerLights` WiFi.

### 3. Power Up
- Power the server via USB.
- Power each client (USB or battery). The built‑in LED on each client will blink while connecting, then stay solid when connected.

### 4. Connect Your Phone/Tablet
- Go to WiFi settings and connect to **`TrainerLights`** (password: `1234567890`).
- Open a browser and navigate to **`http://192.168.4.1`** (or `http://trainerlights.local` if mDNS works).
- The TrainerLights control panel will load.

---

## 📱 Using the Web Interface

### Main Sections
- **Timer** – start/stop/reset the training session.
- **Score & Errors** – successful hits vs. timeouts.
- **Reaction Times** – average, minimum, maximum (ms).
- **Distances** – average, minimum, maximum (cm).
- **Training Modes** – choose `Random` or `Sequential`.
- **Delay Range** – random wait before light turns on (ms).
- **Timeout Range** – random time the light stays on (ms).
- **Detection Range** – min/max distance (cm) for a valid hit.
- **Configure** – send settings to the server (saved in EEPROM).
- **List Sensors** – show all connected client lights.

### Workflow
1. Set your desired parameters.
2. Click **Configure**.
3. Click **Start** (timer begins, stimuli are sent).
4. Athletes react when their light turns on.
5. Watch statistics update in real time.
6. Click **Stop** to end the session.

---

## 🎮 Client Button Functions

Each client board has a tactile button (connected to D5). Its behaviour depends on how long you press it:

| Press Duration | Function               | Visual Feedback                     | Use Case                         |
|----------------|------------------------|-------------------------------------|----------------------------------|
| **< 1 sec**    | Toggle status LED      | Built‑in LED on/off                 | Identify a specific light        |
| **1–3 sec**    | Manual stimulus        | Stimulus LED turns on (default timeout) | Test light without server       |
| **> 3 sec**    | Calibration            | Both red and green LEDs on (yellow) | Check sensor alignment           |

During calibration, the sensor takes 10 readings and reports the average distance via Serial. If the average is >100 cm, a warning is shown (rapid red flashes).

---

## ⚙️ Configuration Storage

The server saves your settings to EEPROM. On next power‑up, it restores:
- Mode (random/sequential)
- Min/max delay
- Min/max timeout
- Min/max detection range

No need to reconfigure every time!

---

## 📊 Statistics Explained

- **Score** – number of successful hits (object detected within range and before timeout).
- **Errors** – timeouts (no detection) or false triggers (reaction < 50 ms).
- **Reaction time** – time from LED turning on until detection.
- **Distance** – measured distance when detection occurred.

All statistics are reset when you click **Start**.

---

## 🛠 Troubleshooting

### Clients don't connect to WiFi
- Ensure the server is powered and the `TrainerLights` network is visible.
- Check that the password is correct (`1234567890`).
- Serial monitor on client shows connection attempts.

### No web interface
- Verify your phone is connected to the `TrainerLights` WiFi.
- Try `http://192.168.4.1` (hard‑coded IP).
- Check server Serial monitor for errors.

### Sensors not detecting
- Verify HC‑SR04 wiring (VCC to 5V, GND, TRIG/ECHO to correct pins).
- Run calibration (long press) to see average distance.
- Adjust detection range in the web interface.

### False triggers or missed hits
- The code implements a moving average filter and requires two consecutive valid readings. This reduces noise but may slightly increase reaction time. Adjust in code if needed.

---

## 📁 Repository Structure

```
TrainerLights/
├── TrainerLights_server.ino      # Server code
├── TrainerLights_client.ino      # Client code
├── README.md                     # This file
└── images/                       # (optional) wiring diagrams, screenshots
```

---

## 📚 Libraries Used

| Library               | Author                 | Purpose                          |
|-----------------------|------------------------|----------------------------------|
| `ESP8266WiFi`         | ESP8266 Community      | WiFi connection                  |
| `WebSockets`          | Markus Sattler         | WebSocket server/client          |
| `ArduinoJson`         | Benoit Blanchon        | JSON parsing / generation        |
| `TaskScheduler`       | Anatoli Arkhipenko     | Cooperative multitasking         |
| `LinkedList`          | Ivan Seidel            | Dynamic list of sensors          |
| `EEPROM`              | Arduino                | Persistent storage               |

Install them via Arduino Library Manager (Sketch → Include Library → Manage Libraries...).

---

## 👥 Credits

**Author:** Ricardo Lerch ([@RickLerch](https://github.com/...))  
**Contact:** ricardo.lerch@gmail.com  

Special thanks to the open‑source community for the incredible libraries that made this project possible.

---

## 📄 License

This project is open source under the MIT License – feel free to use, modify, and distribute.

---

## 🌟 Support & Contributions

Found a bug? Want a new feature? Open an issue or submit a pull request. Feedback is always welcome!

---

**Happy training!** 💪
```