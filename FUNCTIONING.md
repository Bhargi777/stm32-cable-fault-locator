# Cable Fault Locator - System Functioning

## Overview

The Cable Fault Locator is an STM32-based embedded system designed to detect and locate faults in underground cables. The system measures current using an INA219 sensor and estimates fault distance based on current threshold analysis. The system operates through a series of defined states with intuitive user interaction.

## Core System Functions

### 1. System Initialization

**Power-On Sequence:**
- STM32 microcontroller reset and peripheral initialization
- GPIO initialization (buttons as inputs, LEDs as outputs)
- I2C1 initialization for LCD and INA219 communication
- LCD initialization with 16x2 character display
- System welcome display ("CABLE FAULT DETECTION")
- Initial delay for sensor stabilization

**Configuration Settings:**
- CURRENT_THRESHOLD_MA: 2 mA (factory default)
- Fault detected when current <= threshold
- Distance estimation: 10-15 cm when fault detected

### 2. User Interface and Button Handling

**Three-Button Interface:**
- **SW_START (PA0)**: Start/stop the system
- **SW_MEASURE (PA1)**: Start current measurement
- **SW_STOP (PA2)**: Emergency stop/return to idle

**Button Debouncing:**
- 25ms delay after initial button press
- Secondary check to confirm press
- Wait for button release before accepting new press

**State Transitions:**
- Idle → Ready (on SW_START press)
- Ready → Measuring (on SW_MEASURE press)
- Measuring → Ready (on SW_STOP press, or after measurement cycle)
- Any state → Idle (on prolonged inactivity or reset)

### 3. I2C Communication

**LCD Display (Addr: 0x27)**
- 4-bit data mode via I2C
- Expander write byte function
- Command and data sending with enable pulse
- Cursor positioning and clearing
- Text printing and number display

**INA219 Current Sensor (Addr: 0x40)**
- 16-bit current register read/write via I2C
- Configuration register setup (calibration, mode)
- Current reading from register 0x04
- Voltage and power monitoring capabilities

**I2C Initialization:**
- GPIOB pins 6,7 configured as I2C1_SCL/SDA
- APB1 peripheral clock enabled
- I2C1 peripheral enabled
- Clock frequency: 16MHz with 80 prescaler
- Rise time: 17 ticks

### 4. Current Measurement

**Measurement Cycle:**
1. System enters measuring mode (yellow LED on)
2. 500ms delay for sensor stabilization
3. INA219_ReadCurrent() called to get raw current
4. Raw value converted to mA (divided by 10)
5. Current compared against CURRENT_THRESHOLD_MA (2 mA)

**Current Calculation:**
```
current_raw = INA219_ReadCurrent()  // 16-bit signed value
if current_raw < 0: current_mA = 0
else: current_mA = current_raw / 10  // Convert to mA
```

**Fault Detection Logic:**
```
if current_mA <= CURRENT_THRESHOLD_MA:
    fault_detected = 1U
    LED_Off(LED_GREEN)
else:
    fault_detected = 0U
    LED_On(LED_GREEN)
```

### 5. Fault Distance Estimation

**When Fault Detected (current <= 2 mA):**
- LCD displays "DIST: 10-15 cm"
- Red LED begins blinking
- Green LED turned off
- Indicates fault is located within 10-15 cm range

**When No Fault (current > 2 mA):**
- LCD displays "DIST: 0 cm"
- Green LED on
- Red LED off
- Indicates no significant fault detected

**Blinking Pattern:**
- Red LED blinks every 300ms
- State toggles every 100ms increment
- Continuous blinking until fault cleared or system stopped

### 6. LCD Display Management

**Display States and Content:**

**DisplayReady():**
- Line 1: "SYSTEM READY"
- Line 2: "SW2: MEASURE"

**DisplayStopped():**
- Line 1: "SYSTEM STOPPED"
- Line 2: "SW1: START"

**DisplayCurrent(uint32_t current_mA):**
- Line 1: "CURRENT: {value} mA"
- Line 2: "DIST: 10-15 cm" (if fault) or "DIST: 0 cm" (if no fault)

**LCD Updates:**
- Display refresh every 500ms (display_elapsed counter)
- Automatic screen clearing before new display
- Cursor positioning at line 1, column 0 and line 2, column 0

### 7. LED Indicator Control

**LED Assignments:**
- LED_GREEN (PB8): System ready/green status
- LED_YELLOW (PB9): Measurement in progress
- LED_RED (PB10): Fault detected

**LED Behaviors:**
- LED_On(pin): Sets bit in BSRR register
- LED_Off(pin): Sets bit+16 in BSRR register
- LEDs_Off(): Turns off all three LEDs
- Test_LEDs(): Sequential test (green → yellow → red) on startup

**Fault LED (UpdateFaultLED):**
- Blink elapsed timer increments by 100ms
- Every 300ms, red LED state toggles
- When fault cleared: blinking stops, red LED off, green LED on
- System reset: all timers and states reset

### 8. System States Overview

**State 1: Idle/Stopped**
- Conditions: System not started
- LCD: "SYSTEM STOPPED" + "SW1: START"
- LEDs: All off
- Buttons: SW1 START active

**State 2: Ready**
- Conditions: system_started = 1, reading_active = 0
- LCD: "SYSTEM READY" + "SW2: MEASURE"
- LEDs: Green on
- Buttons: SW2 MEASURE active

**State 3: Measuring**
- Conditions: system_started = 1, reading_active = 1
- LCD: "CURRENT: {value} mA" + "DIST: 10-15 cm" or "DIST: 0 cm"
- LEDs: Yellow on during measurement
- Buttons: SW3 STOP active, measurement ongoing

**State 4: Fault**
- Conditions: system_started = 1, reading_active = 1, fault_detected = 1
- LCD: "CURRENT: {value} mA" + "DIST: 10-15 cm"
- LEDs: Red blinking, green off
- Buttons: SW3 STOP active

**State 5: Stopped (Reset)**
- Conditions: system_started = 0 (after SW3 press)
- LCD: "SYSTEM STOPPED" + "SW1: START"
- LEDs: All off
- All counters and flags reset

## System Timing and Performance

### Update Cycle
- Main loop executes continuously
- Button polling: each iteration
- Current measurement: every 100ms during active reading
- LCD display update: every 500ms
- Fault LED blinking: every 300ms (state toggle every 100ms)

### Response Times
- Button press detection: <25ms (after debounce)
- State transition (Idle → Ready): immediate on button press
- Current measurement start: 500ms delay after SW_MEASURE
- LCD display update: 500ms interval
- Fault LED start: 300ms after fault detection

### Power Efficiency
- STM32 low-power modes available for deployment
- LCD backlight controllable via LCD_BL define
- LEDs only active during relevant states
- Minimal current draw during idle/stopped states

## Configuration and Customization

### Adjustable Parameters

**CURRENT_THRESHOLD_MA** (main.c:21):
```c
#define CURRENT_THRESHOLD_MA 2  // Current threshold in mA
```
- Lower values: More sensitive fault detection
- Higher values: Less sensitive, ignores small currents
- Typical range: 1-5 mA depending on cable conditions

### Calibration Requirements

**INA219 Initialization** (main.c:379-383):
```c
void INA219_Init(void)
{
    I2C_Write16(INA_ADDR, 0x00, 0x399F);  // Configuration register
    I2C_Write16(INA_ADDR, 0x05, 4096);     // Calibration register
}
```
- Configuration 0x399F: 16mV shunt voltage range, 1A shunt current
- Calibration 4096: Based on 0.1Ω shunt resistor typical values

**LCD Address** (main.c:6):
```c
#define LCD_ADDR 0x27  // Default I2C address
```
- May need adjustment: 0x3F for some LCD modules
- Verify with I2C scanner sketch if display not responding

**INA219 I2C Address** (main.c:7):
```c
#define INA_ADDR 0x40  // Default I2C address
```
- Alternative: 0x41 if ADDR pin connected to VCC

## Error Handling and Reliability

### Sensor Validation
- I2C communication timeout checks
- LCD initialization verification on startup
- INA219 read returns validated values
- Negative current values handled (clamped to 0)

### System Recovery
- Automatic state reset when user starts new cycle
- Timer synchronization prevents stuck states
- All flags and counters reset on system stop
- GPIO defaults to safe state on reset

### Common Error Conditions
- **LCD not responding**: Check I2C connections, verify LCD_ADDR
- **INA219 not responding**: Check I2C connections, verify INA_ADDR, re-initialize
- **Buttons not working**: Check GPIOA pins, verify pull-up configuration
- **LEDs not responding**: Check GPIOB pins, verify LED definitions
- **Fault detection always off**: Adjust CURRENT_THRESHOLD_MA
- **Fault detection always on**: Verify INA219 connections and calibration

## Health and Safety Benefits

### Fault Detection Benefits
- **Early Warning**: Detects cable faults before catastrophic failure
- **Reduced Downtime**: Faster fault location means quicker repairs
- **Safety Improvement**: Prevents electrical hazards from undetected faults
- **Cost Savings**: Reduced need for expensive fault location equipment

### Operational Benefits
- **Standardized Testing**: Consistent fault detection across different technicians
- **Documentation**: LCD display provides record of current measurements
- **Training**: Intuitive interface reduces training time for new operators
- **Portability**: Handheld or bench-mounted operation possible