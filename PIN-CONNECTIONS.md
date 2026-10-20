# Cable Fault Locator - Pin Connections & Wiring

## STM32 Pin Assignment Table

| Component | Signal | STM32 Pin | Pin Type | Voltage | Notes |
|-----------|--------|-----------|----------|---------|-------|
| **LCD Display (16x2)** |
| | RS (Register Select) | PA0 | Digital Output | 3.3V | Command/Data selection |
| | EN (Enable) | PA1 | Digital Output | 3.3V | Enable signal |
| | D4 (Data Bit 4) | PB0 | Digital I/O | 3.3V | 4-bit mode |
| | D5 (Data Bit 5) | PB1 | Digital I/O | 3.3V | 4-bit mode |
| | D6 (Data Bit 6) | PB4 | Digital I/O | 3.3V | 4-bit mode |
| | D7 (Data Bit 7) | PB5 | Digital I/O | 3.3V | 4-bit mode |
| | Backlight A | 5V | Power | 3.3V | Backlight anode |
| | Backlight K | GND | Ground | 0V | Backlight cathode |
| **Push Buttons** |
| | SW_START | PA0 | Digital Input | 3.3V | Start button (also LCD RS) |
| | SW_MEASURE | PA1 | Digital Input | 3.3V | Measure button (also LCD EN) |
| | SW_STOP | PA2 | Digital Input | 3.3V | Stop button |
| **LEDs** |
| | LED_GREEN | PB8 | Digital Output | 3.3V | Green status LED |
| | LED_YELLOW | PB9 | Digital Output | 3.3V | Yellow measurement LED |
| | LED_RED | PB10 | Digital Output | 3.3V | Red fault LED |
| **INA219 Current Sensor** |
| | SDA | PB7 | Digital I/O | 3.3V | I2C data line |
| | SCL | PB6 | Digital I/O | 3.3V | I2C clock line |

## Detailed Wiring Connections

### 1. LCD Display (16x2) via I2C

**LCD Pin Layout and Connections:**

```
LCD Pin | Function | STM32 Connection | Notes
--------|----------|-----------------|-------
1 (VSS) | Ground   | GND             | Common ground
2 (VDD) | Power    | 3.3V or 5V      | Main power supply
3 (V0)  | Contrast | Potentiometer   | 10kΩ pot for contrast
4 (RS)  | Register | PA0             | Command/Data select
5 (RW)  | Read/Write| GND            | Always write mode
6 (EN)  | Enable   | PA1             | Enable pulse
7-10    | Data 0-3 | Not Used        | Using 4-bit mode
11 (D4) | Data 4   | PB0             | Data bit 4
12 (D5) | Data 5   | PB1             | Data bit 5
13 (D6) | Data 6   | PB4             | Data bit 6
14 (D7) | Data 7   | PB5             | Data bit 7
15 (A)  | Backlight| 5V via resistor | Backlight anode
16 (K)  | Backlight| GND             | Backlight cathode
```

**Connection Steps:**
1. Connect VSS (pin 1) to STM32 GND
2. Connect VDD (pin 2) to 3.3V or 5V (depending on LCD module)
3. Connect V0 (pin 3) to center pin of 10kΩ potentiometer
4. Connect potentiometer outer pins to 3.3V and GND
5. Connect RS (pin 4) to STM32 PA0
6. Connect RW (pin 5) to STM32 GND
7. Connect EN (pin 6) to STM32 PA1
8. Skip pins 7-10 (not used in 4-bit mode)
9. Connect D4-D7 (pins 11-14) to STM32 PB0, PB1, PB4, PB5 respectively
10. Connect backlight A (pin 15) to 5V through 220Ω resistor
11. Connect backlight K (pin 16) to GND

### 2. INA219 Current Sensor via I2C

**Sensor Pin Configuration:**

```
INA219 Pin | Function | STM32 Connection | Wire Color (Suggested)
-----------|----------|-----------------|---------------------
VCC        | Power    | 3.3V or 5V      | Red
GND        | Ground   | GND             | Black
SDA        | Data     | PB7             | Yellow
SCL        | Clock    | PB6             | Green
```

**I2C Connection Details:**
- Requires stable 3.3V or 5V power supply (minimum 10mA)
- I2C1 peripheral on STM32F4
- Standard I2C fast mode (400kHz)
- Pull-up resistors required on I2C lines (typically 4.7kΩ)
- Default I2C address: 0x48 (with address pins floating) or 0x40/0x41

**INA219 Configuration:**
- Configuration register: 0x00
- Calibration register: 0x05
- Current shunt resistor value must be configured
- Voltage and power monitoring available

### 3. Push Buttons

**Button Pin Configuration (with internal pull-ups):**

```
Button | STM32 Pin | Configuration | Pull-up/Pull-down
-------|-----------|---------------|------------------
SW_START | PA0 | Input | Internal pull-up enabled
SW_MEASURE | PA1 | Input | Internal pull-up enabled
SW_STOP | PA2 | Input | Internal pull-up enabled
```

**Button Wiring Details:**
- PA0, PA1, PA2 configured as input with pull-ups
- Buttons connect pin to GND when pressed
- Active-low detection (read 0 when pressed)
- Debouncing handled in software (25ms delay + double-check)
- All buttons share GPIOA port

### 4. LEDs (Green, Yellow, Red)

**LED Pin Configuration (Push-Pull Output):**

```
LED | STM32 Pin | Type | Current | Series Resistor
----|-----------|------|---------|----------------
GREEN | PB8 | Output | ~20mA | 330Ω or internal LED
YELLOW | PB9 | Output | ~20mA | 330Ω or internal LED
RED | PB10 | Output | ~20mA | 330Ω or internal LED
```

**LED Wiring Details:**
- PB8, PB9, PB10 configured as output mode (general purpose push-pull)
- LEDs connected between pin and GND (cathode to GND, anode to pin via resistor)
- Or LEDs with internal resistors can be connected directly
- GPIOB->OTYPER cleared to push-pull mode
- Initial state: all LEDs on (BSRR set in GPIO_Init)
- LED_On(pin): Sets bit in BSRR (turns LED on)
- LED_Off(pin): Sets bit+16 in BSRR (turns LED off)

### 5. STM32 I2C1 Configuration

**I2C1 Pin Assignment:**

```
Pin | Function | Port | Mode
----|----------|------|------
PB6 | I2C1_SCL | GPIOB | Alternate Function 4
PB7 | I2C1_SDA | GPIOB | Alternate Function 4
```

**I2C1 Initialization Details:**
- RCC->AHB1ENR: Enable GPIOB clock
- RCC->APB1ENR: Enable I2C1 clock
- GPIOB->MODER: Set PB6, PB7 to alternate function mode (01)
- GPIOB->OTYPER: Set as open-drain for I2C
- GPIOB->OSPEEDR: Set high speed for PB6, PB7
- GPIOB->PUPDR: Set pull-up for PB6, PB7
- GPIOB->AFR[0]: Set AF4 (0100) for PB6, PB7
- I2C1->CR1: Software reset, then clear
- I2C1->CR2: Set peripheral clock (16 for 16MHz)
- I2C1->CCR: Set clock divider (80 for 16MHz / 100kHz)
- I2C1->TRISE: Set rise time (17 for 16MHz / 100kHz)
- I2C1->CR1: Enable I2C peripheral (PE bit)

## Power Supply Requirements ⚡

### Total Power Consumption Analysis

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| STM32F4 | 3.3V | ~50mA | ~165mW |
| LCD Display | 5V | ~20mA | ~100mW |
| INA219 Current Sensor | 3.3V | ~15mA | ~50mW |
| STM32 GPIO (LEDs) | 3.3V | ~15mA | ~50mW |
| Buttons | 3.3V | ~1mA | ~3mW |
| **Total** | **~4.5V** | **~101mA** | **~~468mW** |

### Recommended Power Supply
- **USB Power**: Adequate for development and testing (5V @ 100mA)
- **External Power**: 3.3V - 5V supply with minimum 200mA capacity
- **Battery Operation**: 3.7V LiPo with 3.3V LDO regulator

### Power Supply Notes
- STM32F4 requires stable 3.3V supply
- LCD may require 5V (check module specifications)
- INA219 can operate on 3.3V or 5V
- All GPIO pins are 3.3V tolerant (do not apply 5V directly)
- Use level shifters if connecting 5V components directly

## PCB Layout Considerations

### Component Placement Tips
1. **Keep I2C lines short**: PB6 (SCL) and PB7 (SDA) traces should be minimized
2. **Place bypass capacitors near power pins**: 100nF ceramic capacitors for STM32 and LCD
3. **Separate digital and analog grounds**: Keep I2C signals away from current-sensing traces
4. **Place pull-up resistors close to I2C pins**: 4.7kΩ for SCL and SDA lines
5. **Keep LED traces short**: PB8, PB9, PB10 switching traces should be minimized

### Signal Integrity
- I2C lines require pull-up resistors (4.7kΩ typical)
- Avoid crossing I2C lines with high-current traces
- Keep LCD data lines (PB0-PB5) separate from noisy signals
- Use ground plane for stable reference

### Decoupling Capacitors
- 100nF ceramic capacitor between VDD and VSS for STM32
- 100nF capacitor between VCC and GND for INA219
- 10µF electrolytic capacitor for LCD backlight voltage stability

## Troubleshooting Connection Issues

### Common Wiring Problems

**LCD Not Working:**
- Check all I2C connections (PB4-PB7, PB0-PB1)
- Verify LCD I2C address (try 0x27 or 0x3F)
- Ensure proper power supply to LCD (5V or 3.3V)
- Test with I2C scanner sketch
- Verify contrast potentiometer setting

**INA219 Not Responding:**
- Check I2C bus wiring (PB6 SCL, PB7 SDA)
- Verify I2C address (0x40 or 0x48)
- Check pull-up resistor values (should be 4.7kΩ)
- Ensure proper VCC and GND connections to INA219
- Re-initialize I2C peripheral after connection changes

**Buttons Not Working:**
- Check GPIOA pin configuration (PA0, PA1, PA2)
- Verify internal pull-up resistors enabled
- Check for short circuits on button lines
- Test with GPIO read in debugger
- Ensure buttons connect to GND when pressed

**LEDs Not Responding:**
- Check GPIOB pin configuration (PB8, PB9, PB10)
- Verify LEDs are not reversed (anode/cathode)
- Check series resistor values
- Test with GPIO toggle in debugger
- Ensure PB8-PB10 are in push-pull output mode

**I2C Communication Errors:**
- Check I2C1 peripheral clock enabled
- Verify GPIO alternate function setup
- Ensure no other I2C devices sharing same bus address
- Check for bus contention or stuck conditions
- Reset I2C peripheral if lines stuck low

### Testing Individual Components

**Step-by-Step Testing:**
1. **Power Test**: Verify 3.3V and GND on all components
2. **I2C Scan**: Run I2C scanner to detect LCD and INA219 addresses
3. **GPIO Test**: Test LED on/off and button press detection
4. **I2C Read/Write**: Test INA219 register read/write operations
5. **Integration**: Combine tested components gradually in main loop

**Debug Code Examples:**

```c
// Test I2C scan for devices
for(address = 1; address < 127; address++) {
    I2C1->CR1 |= I2C_CR1_START;
    while(!(I2C1->SR1 & I2C_SR1_SB));
    I2C1->DR = (address << 1);
    while(!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;
    // Check if device acknowledged
}

// Test LED
GPIOB->BSRR = 1U << 8;  // Turn on green LED
delay_ms(500);
GPIOB->BSRR = 1U << (8 + 16U);  // Turn off green LED

// Test Button
if((GPIOA->IDR & 1U) == 0U) {  // SW_START pressed
    // Button pressed
}
```

## Pin Conflict Resolution

### LCD RS/EN Pin Sharing with Buttons
The smart workspace project uses LCD RS and EN on different pins. This project uses:
- **PA0**: Shared between SW_START and LCD RS (different functions at different times)
- **PA1**: Shared between SW_MEASURE and LCD EN (different functions at different times)

**Usage Pattern:**
- During idle/ready state: PA0/PA1 read as buttons
- During LCD operations: PA0/PA1 used for LCD command/data
- Button presses checked in main loop when not actively LCD operations

### Mitigation
- Button presses checked only in main loop state transitions
- LCD operations occur after button checks are complete
- No simultaneous button press and LCD operation in same cycle
- GPIO pin modes switched as needed (input with pull-up for buttons, output for LCD control)

## Summary

| Component | Port | Pin | Function |
|-----------|------|-----|----------|
| LCD RS | GPIOA | 0 | Register Select |
| LCD EN | GPIOA | 1 | Enable |
| LCD D4 | GPIOB | 0 | Data Bit 4 |
| LCD D5 | GPIOB | 1 | Data Bit 5 |
| LCD D6 | GPIOB | 4 | Data Bit 6 |
| LCD D7 | GPIOB | 5 | Data Bit 7 |
| I2C1 SCL | GPIOB | 6 | I2C Clock |
| I2C1 SDA | GPIOB | 7 | I2C Data |
| LED GREEN | GPIOB | 8 | Green Status |
| LED YELLOW | GPIOB | 9 | Measurement |
| LED RED | GPIOB | 10 | Fault Indicator |
| SW_START | GPIOA | 0 | Start (shared LCD RS) |
| SW_MEASURE | GPIOA | 1 | Measure (shared LCD EN) |
| SW_STOP | GPIOA | 2 | Stop |