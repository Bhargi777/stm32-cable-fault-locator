# Cable Fault Locator

A STM32-based cable fault detection system that locates faults in underground cables using current measurement and fault distance estimation.

## Features

- **Automatic Fault Detection**: Detects cable faults based on current threshold
- **Distance Estimation**: Estimates fault distance (10-15 cm when fault detected)
- **LED Indicator**: Red LED blinks when fault detected, green when system ready
- **LCD Display**: Real-time current reading and fault distance
- **User Interface**: Start/Measure/Stop buttons for intuitive operation
- **Power Saving**: System enters stopped state when not in use

## Hardware Requirements

| Component | Quantity | Purpose |
|-----------|----------|---------|
| STM32 Microcontroller | 1 | Main controller |
| 16x2 LCD Display | 1 | Status display |
| INA219 Current Sensor | 1 | Current measurement |
| Push Buttons | 3 | Start, Measure, Stop |
| LEDs (Green, Yellow, Red) | 3 | Status indicators |
| Resistors & Wires | Various | Circuit connections |

## Quick Start

### Prerequisites

- STM32 development environment (CubeIDE, Keil, or VSCode)
- ST-Link programmer for firmware upload
- Basic electronics knowledge

### Hardware Setup

1. Follow the pin connections in [PIN-CONNECTIONS.md](PIN-CONNECTIONS.md)
2. Connect all components according to the pin assignment table
3. Double-check all connections before powering on

### Software Setup

1. Open the project in your STM32 IDE
2. Ensure proper board selection (STM32F4xx)
3. Build and upload the firmware
4. The system will display "CABLE FAULT DETECTION" on startup

### Configuration

Modify settings in main.c if needed:

```c
#define CURRENT_THRESHOLD_MA 2  // Current threshold in mA for fault detection
```

## Documentation

| Document | Description |
|----------|-------------|
| [PROJECT-OVERVIEW.md](PROJECT-OVERVIEW.md) | Complete project background, goals, and vision |
| [FUNCTIONING.md](FUNCTIONING.md) | Detailed explanation of how the system works |
| [PIN-CONNECTIONS.md](PIN-CONNECTIONS.md) | Comprehensive wiring guide and circuit diagrams |

## Usage

### System States

**Idle Mode**
- System is off, waiting for start button
- LCD shows "System OFF"

**Ready Mode**
- System started, waiting for measurement
- LCD shows "SYSTEM READY" and "SW2: MEASURE"
- Green LED on

**Measuring Mode**
- Current being measured
- Yellow LED on during measurement
- LCD shows current and distance

**Fault Mode**
- Fault detected (current <= threshold)
- Red LED blinks, distance shows "10-15 cm"

**Stopped Mode**
- System stopped
- LCD shows "SYSTEM STOPPED" and "SW1: START"

### Quick Controls

- **Power On**: Connect to power
- **Start (SW1)**: Start the system
- **Measure (SW2)**: Start current measurement
- **Stop (SW3)**: Stop the system

### Customization

Adjust these values in main.c:

```c
#define CURRENT_THRESHOLD_MA 2  // Current threshold in mA
```

### Troubleshooting

**LCD not displaying:**
- Check all I2C connections
- Verify LCD address (default 0x27)
- Ensure proper power supply

**Fault detection not working:**
- Verify INA219 I2C address (default 0x40)
- Check current sensor connections
- Adjust CURRENT_THRESHOLD_MA

**LED not responding:**
- Check LED connections to PB8, PB9, PB10
- Verify GPIO initialization