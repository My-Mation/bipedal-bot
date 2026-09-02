# Mobile App Developer Prompt: ESP32 Biped Robot Dashboard

> **HOW TO USE THIS FILE**: Copy this entire document and paste it directly into the AI assistant (ChatGPT, Claude, Gemini, etc.) that will build your mobile app (Flutter, React Native, iOS, or Android).

---

## 🤖 System Context & Architecture

You are an expert mobile app developer. I am building a mobile control dashboard app (in Flutter / React Native / Native) for a 6-DOF Biped Robot powered by an **ESP32 DevKit V1**. 

The robot acts as an Access Point (Hotspot) and streams live telemetry via **WebSockets** at ~30 Hz while listening for real-time motion commands, stance presets, light toggles, and audio chirps.

---

## 🌐 Network & Connection Specification

- **Wi-Fi Access Point SSID**: `BipedBot`
- **Wi-Fi Password**: `12345678`
- **ESP32 Gateway / Server IP**: `192.168.4.1`
- **WebSocket Endpoint**: `ws://192.168.4.1/ws`
- **Port**: `80`

### Connection Lifecycle & Audio Triggers
1. **Wi-Fi Ready**: ESP32 emits 1 short beep when the Wi-Fi AP starts.
2. **Phone Connect**: When the app opens a WebSocket connection to `ws://192.168.4.1/ws`, the ESP32 emits **2 quick high-pitched chirps**.
3. **Phone Disconnect**: When the app closes or loses connection, the ESP32 emits **1 lower warning beep** and initiates safety procedures.
4. **Heartbeat Requirement**: The app **MUST** send `{"type": "heartbeat"}` every **1000 ms (1 second)**. If the ESP32 receives no heartbeat for >1500 ms, it triggers an Emergency Stop (`ESTOP`) for safety.

---

## 📡 Live Telemetry JSON Schema (Incoming from ESP32 @ ~30 Hz)

The ESP32 broadcasts real-time telemetry over WebSockets in the following JSON format:

```json
{
  "type": "telemetry",
  "batteryPercent": 100,
  "batteryVoltage": 7.40,
  "light": false,
  "roll": -1.2,
  "pitch": 3.4,
  "yaw": null,
  "yawSupported": false,
  "accelX": 0.12,
  "accelY": -0.05,
  "accelZ": 9.81,
  "gyroX": 0.01,
  "gyroY": 0.00,
  "gyroZ": 0.02,
  "imu": {
    "ok": true,
    "pitch": 3.4,
    "roll": -1.2,
    "accelX": 0.12,
    "accelY": -0.05,
    "accelZ": 9.81,
    "gyroX": 0.01,
    "gyroY": 0.00,
    "gyroZ": 0.02,
    "temp": 31.5
  },
  "gps": {
    "valid": true,
    "lat": 22.572645,
    "lng": 88.363892,
    "alt": 18.5,
    "speed": 0.2,
    "sats": 8,
    "hdop": 1.2
  },
  "queue": 0,
  "moving": false,
  "wifiRSSI": -42,
  "servos": [2350, 650, 1950, 2350, 2500, 2500]
}
```

### Telemetry Field Breakdown

| JSON Field | Type | Description |
| :--- | :--- | :--- |
| `batteryPercent` | `int` | Battery capacity percentage (0–100%) |
| `batteryVoltage` | `float` | Voltage reading in Volts (e.g. 7.40 V) |
| `light` | `bool` | Headlight (GPIO 32) state (`true` = ON, `false` = OFF) |
| `imu.ok` | `bool` | MPU6050 sensor initialization status |
| `imu.pitch` | `float` | Forward/backward tilt angle in degrees (-180° to +180°) |
| `imu.roll` | `float` | Side-to-side tilt angle in degrees (-180° to +180°) |
| `imu.accelX/Y/Z` | `float` | 3-axis acceleration in m/s² |
| `imu.gyroX/Y/Z` | `float` | 3-axis angular speed in rad/s |
| `imu.temp` | `float` | MPU6050 die temperature in °C |
| `gps.valid` | `bool` | `true` when GPS satellite fix is valid |
| `gps.lat` | `double` | Latitude in decimal degrees (e.g. 22.572645) |
| `gps.lng` | `double` | Longitude in decimal degrees (e.g. 88.363892) |
| `gps.alt` | `float` | Altitude above sea level in meters |
| `gps.speed` | `float` | Speed over ground in km/h |
| `gps.sats` | `int` | Number of connected GPS satellites |
| `gps.hdop` | `float` | Horizontal Dilution of Precision (lower is better) |
| `moving` | `bool` | `true` if any servo is currently executing a motion |
| `queue` | `int` | Number of motion commands currently queued |
| `wifiRSSI` | `int` | Wi-Fi Signal Strength in dBm (e.g. -42) |
| `servos` | `Array[6]` | Live positions of S1–S6 (S1,S2,S4,S5,S6 in µs; S3 in ADC 0–4095) |

---

## 🕹️ Outgoing Commands JSON Schema (App -> ESP32)

Send raw text JSON frames to `ws://192.168.4.1/ws`:

### 1. Heartbeat (Mandatory every 1s)
```json
{"type": "heartbeat"}
```

### 2. Headlight Toggle (GPIO 32)
```json
{"type": "light", "state": true}
```
*(or send `{"type": "light", "toggle": true}` to flip state)*

### 3. Horn / Buzzer Chirp (GPIO 23 NPN Transistor)
```json
{"type": "beep", "duration": 150}
```

### 4. Stance Controls
- **Home Stance (Standing)**: `{"type": "home"}`
- **Sit Down (Folded Legs)**: `{"type": "sit"}`
- **Emergency Stop (E-STOP)**: `{"type": "estop"}`
- **Clear Motion Queue**: `{"type": "clearQueue"}`

### 5. Custom Servo / Movement Command
```json
{
  "type": "move",
  "id": 101,
  "servos": [
    {"id": 0, "position": 2350, "speed": 100},
    {"id": 1, "position": 650,  "speed": 100},
    {"id": 2, "position": 1950, "speed": 100},
    {"id": 3, "position": 2350, "speed": 100},
    {"id": 4, "position": 1450, "speed": 100},
    {"id": 5, "position": 1700, "speed": 100}
  ]
}
```

---

## 📱 Mobile App UI Dashboard Design Specification

Please design a futuristic, responsive, dark-themed UI dashboard with the following sections:

### 1. Top Bar & Connection Header
- **Status Pill**: `CONNECTED` (Green badge) / `DISCONNECTED` (Red badge)
- **Signal**: Wi-Fi RSSI indicator bar
- **Battery**: Percentage & voltage meter
- **Headlight Switch**: Toggle button for Headlight (GPIO 32) with glowing icon indicator
- **Horn Button**: Instant tap-to-beep button

### 2. MPU6050 Gyro & Orientation Card
- **Visual Artificial Horizon**: 2D gauge or animated tilt card driven by `imu.pitch` and `imu.roll`.
- **Gyro Readings**: Digital pill cards for Accel (X, Y, Z) and Gyro (X, Y, Z) plus Temp (°C).

### 3. GY-GPS6MV2 GPS Location & Mapping Card
- **Live Location Badge**: Displays `Latitude`, `Longitude`, `Altitude`, `Speed (km/h)`, and `Satellites count`.
- **Map View**: Integrated Flutter Map / Google Map pin positioned at `gps.lat`, `gps.lng` with fix quality status (`VALID FIX` / `SEARCHING SATELLITES`).

### 4. Robot Control & Stance Panel
- **Quick Stance Buttons**: Big, clear action buttons:
  - 🧍 **HOME** (Stand up)
  - 🧎 **SIT** (Sit down)
  - 🛑 **EMERGENCY STOP** (Red button)
- **Virtual Joystick / Gait Controls**: Forward, Backward, Turn Left, Turn Right movement trigger buttons.

### 5. 6-DOF Servo Monitor
- Visual position progress bars for:
  - `S1`: Front-Left Leg (1000–2500 µs)
  - `S2`: Back-Right Leg (500–2500 µs)
  - `S3`: Front-Right Smart Servo (50–3950 ADC feedback)
  - `S4`: Back-Left Leg (1170–2500 µs)
  - `S5`: Slider (1450–2500 µs)
  - `S6`: Rotator (1700–2500 µs)

---

## 🚀 Starter Flutter Service Implementation

Here is the Dart WebSocket service class template to connect to the robot:

```dart
import 'dart:async';
import 'dart:convert';
import 'package:web_socket_channel/web_socket_channel.dart';

class RobotService {
  WebSocketChannel? _channel;
  Timer? _heartbeatTimer;
  final StreamController<Map<String, dynamic>> _telemetryController = 
      StreamController<Map<String, dynamic>>.broadcast();

  Stream<Map<String, dynamic>> get telemetryStream => _telemetryController.stream;

  void connect() {
    try {
      _channel = WebSocketChannel.connect(Uri.parse('ws://192.168.4.1/ws'));
      
      _channel!.stream.listen(
        (message) {
          final data = jsonDecode(message);
          if (data['type'] == 'telemetry') {
            _telemetryController.add(data);
          }
        },
        onError: (err) => print('WS Error: $err'),
        onDone: () => _onDisconnected(),
      );

      // Start 1 Hz Heartbeat
      _heartbeatTimer = Timer.periodic(const Duration(seconds: 1), (_) {
        send({'type': 'heartbeat'});
      });
    } catch (e) {
      print('Connection failed: $e');
    }
  }

  void send(Map<String, dynamic> command) {
    if (_channel != null) {
      _channel!.sink.add(jsonEncode(command));
    }
  }

  void toggleLight(bool state) => send({'type': 'light', 'state': state});
  void triggerBeep([int durationMs = 150]) => send({'type': 'beep', 'duration': durationMs});
  void goHome() => send({'type': 'home'});
  void sitDown() => send({'type': 'sit'});
  void emergencyStop() => send({'type': 'estop'});

  void _onDisconnected() {
    _heartbeatTimer?.cancel();
    print('Disconnected from robot');
  }

  void dispose() {
    _heartbeatTimer?.cancel();
    _channel?.sink.close();
    _telemetryController.close();
  }
}
```

---

*Build the mobile app using this specification to seamlessly pair with the ESP32 Walking Bot!*
