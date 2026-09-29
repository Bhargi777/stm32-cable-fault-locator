#include "stm32f4xx.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define LCD_ADDR_1        0x27
#define LCD_ADDR_2        0x3F
#define INA219_ADDR       0x40

#define START_PIN         0
#define SELECT_PIN        1
#define RESET_PIN         2

#define GREEN_PIN         8
#define YELLOW_PIN        9
#define RED_PIN           10
#define BUZZER_PIN        8

#define LCD_RS            0x01
#define LCD_RW            0x02
#define LCD_EN            0x04
#define LCD_BL            0x08

#define LCD_D4            0x10
#define LCD_D5            0x20
#define LCD_D6            0x40
#define LCD_D7            0x80

#define SHUNT_RESISTOR_OHM    0.1f

#define SERIES_RESISTOR_OHM   100.0f

/*
   CHANGE THIS AFTER CABLE CALIBRATION.

   Example:
   If one conductor of your actual cable measures
   0.05 ohm per metre:

   CABLE_OHM_PER_M = 0.05
*/
#define CABLE_OHM_PER_M       0.05f

#define MIN_FAULT_DISTANCE    0.05f
#define MAX_FAULT_DISTANCE    100.0f

volatile uint32_t ms_ticks = 0;

uint8_t lcd_address = LCD_ADDR_1;

void SysTick_Handler(void)
{
    ms_ticks++;
}

void delay_ms(uint32_t ms)
{
    uint32_t start = ms_ticks;

    while ((ms_ticks - start) < ms);
}

void GPIO_Init(void)
{
    RCC->AHB1ENR |= (1 << 0);
    RCC->AHB1ENR |= (1 << 1);

    /* PA0 PA1 PA2 input */
    GPIOA->MODER &= ~(
        (3 << (START_PIN * 2)) |
        (3 << (SELECT_PIN * 2)) |
        (3 << (RESET_PIN * 2))
    );

    /* Internal pull-up */
    GPIOA->PUPDR &= ~(
        (3 << (START_PIN * 2)) |
        (3 << (SELECT_PIN * 2)) |
        (3 << (RESET_PIN * 2))
    );

    GPIOA->PUPDR |= (
        (1 << (START_PIN * 2)) |
        (1 << (SELECT_PIN * 2)) |
        (1 << (RESET_PIN * 2))
    );

    /* PB8 PB9 PB10 outputs */
    GPIOB->MODER &= ~(
        (3 << (GREEN_PIN * 2)) |
        (3 << (YELLOW_PIN * 2)) |
        (3 << (RED_PIN * 2))
    );

    GPIOB->MODER |= (
        (1 << (GREEN_PIN * 2)) |
        (1 << (YELLOW_PIN * 2)) |
        (1 << (RED_PIN * 2))
    );

    GPIOB->OTYPER &= ~(
        (1 << GREEN_PIN) |
        (1 << YELLOW_PIN) |
        (1 << RED_PIN)
    );

    GPIOB->ODR &= ~(
        (1 << GREEN_PIN) |
        (1 << YELLOW_PIN) |
        (1 << RED_PIN)
    );

    /* PA8 buzzer control */
    GPIOA->MODER &= ~(3 << (BUZZER_PIN * 2));
    GPIOA->MODER |= (1 << (BUZZER_PIN * 2));

    GPIOA->OTYPER &= ~(1 << BUZZER_PIN);
    GPIOA->ODR &= ~(1 << BUZZER_PIN);
}

void I2C1_Init(void)
{
    RCC->AHB1ENR |= (1 << 1);
    RCC->APB1ENR |= (1 << 21);

    /* PB6 = I2C1_SCL, PB7 = I2C1_SDA */

    GPIOB->MODER &= ~(
        (3 << (6 * 2)) |
        (3 << (7 * 2))
    );

    GPIOB->MODER |= (
        (2 << (6 * 2)) |
        (2 << (7 * 2))
    );

    GPIOB->OTYPER |= (
        (1 << 6) |
        (1 << 7)
    );

    GPIOB->OSPEEDR |= (
        (3 << (6 * 2)) |
        (3 << (7 * 2))
    );

    GPIOB->PUPDR &= ~(
        (3 << (6 * 2)) |
        (3 << (7 * 2))
    );

    GPIOB->AFR[0] &= ~(
        (0xF << (6 * 4)) |
        (0xF << (7 * 4))
    );

    GPIOB->AFR[0] |= (
        (4 << (6 * 4)) |
        (4 << (7 * 4))
    );

    I2C1->CR1 = 0;
    I2C1->CR2 = 16;

    I2C1->CCR = 80;

    I2C1->TRISE = 17;

    I2C1->CR1 |= (1 << 0);
}

void I2C_Start(void)
{
    I2C1->CR1 |= (1 << 8);

    while (!(I2C1->SR1 & (1 << 0)));
}

void I2C_Stop(void)
{
    I2C1->CR1 |= (1 << 9);
}

uint8_t I2C_Write(uint8_t data)
{
    while (!(I2C1->SR1 & (1 << 7)));

    I2C1->DR = data;

    while (!(I2C1->SR1 & (1 << 2)));

    if (I2C1->SR1 & (1 << 10))
    {
        I2C1->SR1 &= ~(1 << 10);
        return 0;
    }

    return 1;
}

uint8_t I2C_Address(uint8_t address, uint8_t read)
{
    uint8_t addr = (address << 1) | read;

    I2C1->DR = addr;

    while (!(I2C1->SR1 & ((1 << 1) | (1 << 10))));

    if (I2C1->SR1 & (1 << 10))
    {
        I2C1->SR1 &= ~(1 << 10);
        return 0;
    }

    return 1;
}

uint8_t I2C_Read(uint8_t ack)
{
    if (ack)
        I2C1->CR1 |= (1 << 10);
    else
        I2C1->CR1 &= ~(1 << 10);

    while (!(I2C1->SR1 & (1 << 6)));

    return I2C1->DR;
}

uint8_t I2C_DevicePresent(uint8_t address)
{
    I2C_Start();

    if (!I2C_Address(address, 0))
    {
        I2C_Stop();
        return 0;
    }

    I2C_Stop();

    return 1;
}

void I2C_WriteRegister(uint8_t address, uint8_t reg, uint16_t value)
{
    I2C_Start();

    I2C_Address(address, 0);

    I2C_Write(reg);

    I2C_Write((value >> 8) & 0xFF);

    I2C_Write(value & 0xFF);

    I2C_Stop();
}

uint16_t I2C_ReadRegister(uint8_t address, uint8_t reg)
{
    uint8_t high;
    uint8_t low;

    I2C_Start();

    I2C_Address(address, 0);

    I2C_Write(reg);

    I2C_Start();

    I2C_Address(address, 1);

    high = I2C_Read(1);
    low = I2C_Read(0);

    I2C_Stop();

    return ((uint16_t)high << 8) | low;
}

/* ---------------- LCD ---------------- */

void LCD_Expander_Write(uint8_t data)
{
    I2C_Start();

    if (I2C_Address(lcd_address, 0))
    {
        I2C_Write(data | LCD_BL);
    }

    I2C_Stop();
}

void LCD_Pulse(uint8_t data)
{
    LCD_Expander_Write(data | LCD_EN);

    delay_ms(1);

    LCD_Expander_Write(data & ~LCD_EN);

    delay_ms(1);
}

void LCD_Send4Bits(uint8_t data)
{
    LCD_Expander_Write(data);
    LCD_Pulse(data);
}

void LCD_SendCommand(uint8_t command)
{
    uint8_t high;
    uint8_t low;

    high = command & 0xF0;
    low = (command << 4) & 0xF0;

    LCD_Send4Bits(high);
    LCD_Send4Bits(low);

    delay_ms(2);
}

void LCD_SendData(uint8_t data)
{
    uint8_t high;
    uint8_t low;

    high = data & 0xF0;
    low = (data << 4) & 0xF0;

    LCD_Send4Bits(high | LCD_RS);
    LCD_Send4Bits(low | LCD_RS);
}

void LCD_Init(void)
{
    delay_ms(50);

    LCD_Send4Bits(0x30);
    delay_ms(5);

    LCD_Send4Bits(0x30);
    delay_ms(1);

    LCD_Send4Bits(0x30);
    delay_ms(1);

    LCD_Send4Bits(0x20);

    LCD_SendCommand(0x28);
    LCD_SendCommand(0x08);
    LCD_SendCommand(0x01);

    delay_ms(2);

    LCD_SendCommand(0x06);
    LCD_SendCommand(0x0C);
}

void LCD_Clear(void)
{
    LCD_SendCommand(0x01);
    delay_ms(2);
}

void LCD_SetCursor(uint8_t row, uint8_t column)
{
    uint8_t address;

    if (row == 0)
        address = 0x00 + column;
    else
        address = 0x40 + column;

    LCD_SendCommand(0x80 | address);
}

void LCD_Print(char *text)
{
    while (*text)
    {
        LCD_SendData(*text++);
    }
}

/* ---------------- INA219 ---------------- */

void INA219_Init(void)
{
    /*
       Configuration:

       32 V bus range
       ±320 mV shunt range
       12-bit bus ADC
       12-bit shunt ADC
       Continuous shunt + bus measurement
    */

    I2C_WriteRegister(INA219_ADDR, 0x00, 0x399F);

    delay_ms(10);
}

int16_t INA219_ReadShuntRaw(void)
{
    return (int16_t)I2C_ReadRegister(INA219_ADDR, 0x01);
}

uint16_t INA219_ReadBusRaw(void)
{
    return I2C_ReadRegister(INA219_ADDR, 0x02);
}

float INA219_GetCurrent(void)
{
    int16_t raw;
    float shunt_voltage;
    float current;

    raw = INA219_ReadShuntRaw();

    shunt_voltage = raw * 0.00001f;

    current = shunt_voltage / SHUNT_RESISTOR_OHM;

    if (current < 0)
        current = -current;

    return current;
}

float INA219_GetBusVoltage(void)
{
    uint16_t raw;
    uint16_t voltage_bits;
    float voltage;

    raw = INA219_ReadBusRaw();

    voltage_bits = raw >> 3;

    voltage = voltage_bits * 0.004f;

    return voltage;
}

/* ---------------- LEDs ---------------- */

void LED_All_Off(void)
{
    GPIOB->ODR &= ~(
        (1 << GREEN_PIN) |
        (1 << YELLOW_PIN) |
        (1 << RED_PIN)
    );
}

void LED_Green(void)
{
    LED_All_Off();

    GPIOB->ODR |= (1 << GREEN_PIN);
}

void LED_Yellow(void)
{
    LED_All_Off();

    GPIOB->ODR |= (1 << YELLOW_PIN);
}

void LED_Red(void)
{
    LED_All_Off();

    GPIOB->ODR |= (1 << RED_PIN);
}

/* ---------------- Buzzer ---------------- */

void Buzzer_On(void)
{
    GPIOA->ODR |= (1 << BUZZER_PIN);
}

void Buzzer_Off(void)
{
    GPIOA->ODR &= ~(1 << BUZZER_PIN);
}

/* ---------------- Buttons ---------------- */

uint8_t Start_Pressed(void)
{
    return !(GPIOA->IDR & (1 << START_PIN));
}

uint8_t Select_Pressed(void)
{
    return !(GPIOA->IDR & (1 << SELECT_PIN));
}

uint8_t Reset_Pressed(void)
{
    return !(GPIOA->IDR & (1 << RESET_PIN));
}

void Wait_Button_Release(void)
{
    while (
        Start_Pressed() ||
        Select_Pressed() ||
        Reset_Pressed()
    )
    {
        delay_ms(10);
    }
}

/* ---------------- Fault calculation ---------------- */

float Calculate_Cable_Resistance(float bus_voltage, float current)
{
    if (current < 0.0001f)
        return 0.0f;

    return bus_voltage / current;
}

float Calculate_Distance(float cable_resistance)
{
    float distance;

    if (CABLE_OHM_PER_M <= 0.0f)
        return 0.0f;

    distance = cable_resistance /
               (2.0f * CABLE_OHM_PER_M);

    return distance;
}

/* ---------------- Display ---------------- */

void Display_Main(void)
{
    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print("CABLE FAULT");

    LCD_SetCursor(1, 0);
    LCD_Print("PRESS START");
}

void Display_Testing(void)
{
    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print("MEASURING...");

    LCD_SetCursor(1, 0);
    LCD_Print("PLEASE WAIT");
}

void Display_Open(void)
{
    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print("NO FAULT");

    LCD_SetCursor(1, 0);
    LCD_Print("CABLE OPEN");
}

void Display_Distance(float distance)
{
    char line[17];

    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print("FAULT DISTANCE");

    snprintf(line, sizeof(line), "%6.2f m", distance);

    LCD_SetCursor(1, 0);
    LCD_Print(line);
}

void Display_Current(float current)
{
    char line[17];

    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print("CURRENT:");

    snprintf(line, sizeof(line), "%7.2f mA", current * 1000.0f);

    LCD_SetCursor(1, 0);
    LCD_Print(line);
}

/* ---------------- Measurement ---------------- */

void Perform_Measurement(void)
{
    float current;
    float bus_voltage;
    float cable_resistance;
    float distance;

    Display_Testing();

    LED_Yellow();

    delay_ms(500);

    current = INA219_GetCurrent();

    if (current < 0.001f)
    {
        LED_Green();

        Display_Open();

        Buzzer_Off();

        return;
    }

    bus_voltage = INA219_GetBusVoltage();

    cable_resistance =
        Calculate_Cable_Resistance(
            bus_voltage,
            current
        );

    distance =
        Calculate_Distance(
            cable_resistance
        );

    if (
        distance < MIN_FAULT_DISTANCE ||
        distance > MAX_FAULT_DISTANCE
    )
    {
        LED_Red();

        Display_Current(current);

        Buzzer_On();

        delay_ms(500);

        Buzzer_Off();

        return;
    }

    LED_Red();

    Display_Distance(distance);

    Buzzer_On();

    delay_ms(300);

    Buzzer_Off();
}

/* ---------------- Main ---------------- */

int main(void)
{
    SystemCoreClockUpdate();

    SysTick_Config(SystemCoreClock / 1000);

    GPIO_Init();

    I2C1_Init();

    delay_ms(100);

    /*
       Find LCD address.
       Most modules use 0x27 or 0x3F.
    */

    if (I2C_DevicePresent(LCD_ADDR_1))
    {
        lcd_address = LCD_ADDR_1;
    }
    else if (I2C_DevicePresent(LCD_ADDR_2))
    {
        lcd_address = LCD_ADDR_2;
    }

    LCD_Init();

    INA219_Init();

    LED_All_Off();

    Buzzer_Off();

    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print("FAULT LOCATOR");

    LCD_SetCursor(1, 0);
    LCD_Print("INITIALIZING");

    delay_ms(1500);

    Display_Main();

    while (1)
    {
        if (Reset_Pressed())
        {
            LED_All_Off();

            Buzzer_Off();

            Display_Main();

            Wait_Button_Release();
        }

        if (Select_Pressed())
        {
            Display_Current(
                INA219_GetCurrent()
            );

            Wait_Button_Release();

            delay_ms(1000);

            Display_Main();
        }

        if (Start_Pressed())
        {
            Perform_Measurement();

            Wait_Button_Release();

            delay_ms(1500);

            Display_Main();
        }
    }
}
