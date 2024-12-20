# Udo's Arduino Shenanigans 2025+
## ESP 8266 Wifi-enabled Switch Monitor

Implements an HTTP-accessible system for monitoring the state of GPIO pins on an ESP8266. It supports OTA firmware updates, EEPROM-based configuration storage, and HTTP notifications.

### Features

- **GPIO Monitoring and Control**:
  - Detects state changes on specific GPIO pins.
  - Sends notifications to a configured server when a pin state changes.

- **Wi-Fi Configuration**:
  - Attempts to connect to stored credentials on startup.
  - Provides a Soft AP mode with a captive portal for configuration if no credentials are available.

- **HTTP Server**:
  - Serves GPIO states and other system information.
  - Accepts commands for resetting the device, setting a notification server, or viewing data in JSON format.

- **OTA Updates**:
  - Allows over-the-air updates for firmware.

- **mDNS Support**:
  - Enables easy access via `http://<hostname>.local` on the local network.

- **EEPROM Storage**:
  - Saves Wi-Fi credentials and notification server URL persistently.

- **Ping Monitoring**:
  - Monitors gateway connectivity using ICMP pings.

- **Non-Blocking HTTP Client**:
  - Sends queued HTTP requests without blocking execution.

### Usage

#### Endpoints

1. `/`:
   - Displays system and GPIO states.

2. `/notify?n=[URL]`:
   - Sets the notification server URL.
   - Example: `http://<device_ip>/notify?n=http://example.com/notify`

3. `/json`:
   - Returns system and GPIO states in JSON format.

4. `/reset`:
   - Reboots the device.

#### Configuration

- When no stored Wi-Fi credentials are available:
  1. The ESP8266 creates a Soft AP (`ESP-SETUP` with password `ESP12345`).
  2. Connect to the AP and navigate to `http://192.168.4.1/` to configure credentials.

### Setup

#### Prerequisites

- **Hardware**:
  - ESP8266 microcontroller (e.g., Wemos D1 Mini).

- **Software**:
  - Arduino IDE with ESP8266 board support installed.
  - ESP 8266 libraries

### Example JSON Response

An example response from the `/json` endpoint:

```json
{
  "NODE": {
    "HOST": "ESP-MT-123456",
    "RC": 42,
    "Load": 3.14,
    "Uptime": 86400
  },
  "SWITCHES": {
    "GPIO2": "OPEN",
    "GPIO4": "CLOSED"
  },
  "SINCE": {
    "GPIO2": 12.3,
    "GPIO4": 0.5
  },
  "WIFI": {
    "AP": "AA:BB:CC:DD:EE:FF",
    "MAC": "FF:EE:DD:CC:BB:AA",
    "RSSI": "-45",
    "Signal": "90%"
  }
}
```

### Customization

#### GPIO Pins

Modify the `safePins` array to define which GPIO pins are monitored:

```cpp
const int safePins[] = {0, 2, 4, 5, 12, 13, 14};
```

#### Notification Server

Set a default notification server URL in the code or through the `/notify` endpoint.

### Troubleshooting

- **Device does not connect to Wi-Fi**:
  - Ensure the stored credentials are correct.
  - Reset the device and re-enter credentials using the captive portal.

- **OTA Updates Fail**:
  - Verify that the device and the computer are on the same network.
  - Ensure no firewall is blocking the OTA process.

- **No mDNS Resolution**:
  - Install an mDNS resolver on your computer (e.g., `avahi-daemon` on Linux).

