#include "stm32f4xx.h"
#include <stdint.h>

/* ================= CONFIGURATION ================= */

#define LCD_ADDR 0x27
#define INA_ADDR 0x40

#define LCD_RS 0x01
#define LCD_EN 0x04
#define LCD_BL 0x08

#define SW_START   0
#define SW_MEASURE 1
#define SW_STOP    2

#define LED_GREEN  8
#define LED_YELLOW 9
#define LED_RED    10

#define CURRENT_THRESHOLD_MA 2




/* ================= Note =================
 * Fault distance calculation requires cable parameters:
 * - Cable length, material, cross-section, insulation type
 * - Or TDR (Time Domain Reflectometry) equipment
 * This system detects fault presence, not exact location.
 * For exact fault location, professional fault locators
 * with TDR capability are required.
 * ================= SETTINGS STRUCT ================= */

typedef struct {
    uint16_t current_threshold_ma;
    uint8_t lcd_addr;
    uint8_t ina_addr;
} CableFaultSettings;

/* ================= STATE STRUCT ================= */

typedef enum {
    STATE_IDLE,
    STATE_READY,
    STATE_MEASURING,
    STATE_FAULT,
    STATE_STOPPED
} CableFaultStateType;

typedef struct {
    uint8_t system_started;
    uint8_t reading_active;
    uint8_t fault_detected;
    uint8_t red_led_state;
    uint32_t blink_elapsed;
    uint32_t display_elapsed;
    CableFaultStateType state;
} CableFaultState;

/* ================= GLOBAL OBJECTS ================= */

static CableFaultSettings settings = {
    CURRENT_THRESHOLD_MA,
    LCD_ADDR,
    INA_ADDR
};

static CableFaultState state = {
    0, 0, 0, 0, 0, 0, STATE_IDLE
};

/* ================= DELAY ================= */

void delay_ms(uint32_t ms)
{
    SysTick->CTRL = 0;

    while(ms--)
    {
        SysTick->LOAD = 16000U - 1U;
        SysTick->VAL = 0;
        SysTick->CTRL = 5U;

        while(!(SysTick->CTRL & 0x10000U))
        {
        }
    }

    SysTick->CTRL = 0;
}

/* ================= GPIO ================= */

void GPIO_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN |
                    RCC_AHB1ENR_GPIOBEN;

    /* Buttons PA0, PA1, PA2: input with pull-ups */

    GPIOA->MODER &= ~0x3FU;
    GPIOA->PUPDR &= ~0x3FU;
    GPIOA->PUPDR |= 0x15U;

    /* LEDs PB8, PB9, PB10: output */

    GPIOB->MODER &= ~((3U << 16) |
                      (3U << 18) |
                      (3U << 20));

    GPIOB->MODER |= (1U << 16) |
                    (1U << 18) |
                    (1U << 20);

    GPIOB->OTYPER &= ~((1U << LED_GREEN) |
                       (1U << LED_YELLOW) |
                       (1U << LED_RED));

    GPIOB->BSRR = (1U << (LED_GREEN + 16U)) |
                  (1U << (LED_YELLOW + 16U)) |
                  (1U << (LED_RED + 16U));
}

void LED_On(uint8_t pin)
{
    GPIOB->BSRR = 1U << pin;
}

void LED_Off(uint8_t pin)
{
    GPIOB->BSRR = 1U << (pin + 16U);
}

void LEDs_Off(void)
{
    LED_Off(LED_GREEN);
    LED_Off(LED_YELLOW);
    LED_Off(LED_RED);
}

void Test_LEDs(void)
{
    LED_On(LED_GREEN);
    delay_ms(400);
    LED_Off(LED_GREEN);

    LED_On(LED_YELLOW);
    delay_ms(400);
    LED_Off(LED_YELLOW);

    LED_On(LED_RED);
    delay_ms(400);
    LED_Off(LED_RED);
}

/* ================= BUTTONS ================= */

uint8_t ButtonPressed(uint8_t pin)
{
    if((GPIOA->IDR & (1U << pin)) == 0U)
    {
        delay_ms(25);

        if((GPIOA->IDR & (1U << pin)) == 0U)
        {
            while((GPIOA->IDR & (1U << pin)) == 0U)
            {
            }

            delay_ms(25);
            return 1U;
        }
    }

    return 0U;
}

/* ================= I2C1 ================= */

void I2C1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    GPIOB->MODER &= ~((3U << 12) | (3U << 14));
    GPIOB->MODER |= ((2U << 12) | (2U << 14));

    GPIOB->OTYPER |= (1U << 6) | (1U << 7);

    GPIOB->OSPEEDR |= (3U << 12) | (3U << 14);

    GPIOB->PUPDR &= ~((3U << 12) | (3U << 14));
    GPIOB->PUPDR |= ((1U << 12) | (1U << 14));

    GPIOB->AFR[0] &= ~((0xFU << 24) | (0xFU << 28));
    GPIOB->AFR[0] |= ((4U << 24) | (4U << 28));

    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0;

    I2C1->CR2 = 16;
    I2C1->CCR = 80;
    I2C1->TRISE = 17;

    I2C1->CR1 |= I2C_CR1_PE;
}

/* ================= I2C WRITE 8 BIT ================= */

void I2C_WriteByte(uint8_t address, uint8_t data)
{
    while(I2C1->SR2 & I2C_SR2_BUSY);

    I2C1->CR1 |= I2C_CR1_START;

    while(!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = address << 1;

    while(!(I2C1->SR1 & I2C_SR1_ADDR));

    (void)I2C1->SR2;

    while(!(I2C1->SR1 & I2C_SR1_TXE));

    I2C1->DR = data;

    while(!(I2C1->SR1 & I2C_SR1_BTF));

    I2C1->CR1 |= I2C_CR1_STOP;
}

/* ================= I2C WRITE 16 BIT ================= */

void I2C_Write16(uint8_t address, uint8_t reg, uint16_t data)
{
    while(I2C1->SR2 & I2C_SR2_BUSY);

    I2C1->CR1 |= I2C_CR1_START;

    while(!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = address << 1;

    while(!(I2C1->SR1 & I2C_SR1_ADDR));

    (void)I2C1->SR2;

    while(!(I2C1->SR1 & I2C_SR1_TXE));
    I2C1->DR = reg;

    while(!(I2C1->SR1 & I2C_SR1_TXE));
    I2C1->DR = (uint8_t)(data >> 8);

    while(!(I2C1->SR1 & I2C_SR1_TXE));
    I2C1->DR = (uint8_t)data;

    while(!(I2C1->SR1 & I2C_SR1_BTF));

    I2C1->CR1 |= I2C_CR1_STOP;
}

/* ================= I2C READ 16 BIT ================= */

uint16_t I2C_Read16(uint8_t address, uint8_t reg)
{
    uint16_t data;

    while(I2C1->SR2 & I2C_SR2_BUSY);

    I2C1->CR1 |= I2C_CR1_START;

    while(!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = address << 1;

    while(!(I2C1->SR1 & I2C_SR1_ADDR));

    (void)I2C1->SR2;

    while(!(I2C1->SR1 & I2C_SR1_TXE));

    I2C1->DR = reg;

    while(!(I2C1->SR1 & I2C_SR1_BTF));

    I2C1->CR1 |= I2C_CR1_START;

    while(!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = (address << 1) | 1U;

    I2C1->CR1 |= I2C_CR1_POS;
    I2C1->CR1 &= ~I2C_CR1_ACK;

    while(!(I2C1->SR1 & I2C_SR1_ADDR));

    (void)I2C1->SR2;

    while(!(I2C1->SR1 & I2C_SR1_BTF));

    I2C1->CR1 |= I2C_CR1_STOP;

    data = ((uint16_t)I2C1->DR << 8);
    data |= I2C1->DR;

    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_ACK;

    return data;
}

/* ================= LCD ================= */

void LCD_Expander_Write(uint8_t data)
{
    I2C_WriteByte(LCD_ADDR, data | LCD_BL);
}

void LCD_Pulse_Enable(uint8_t data)
{
    LCD_Expander_Write(data | LCD_EN);
    delay_ms(1);

    LCD_Expander_Write(data & (uint8_t)~LCD_EN);
    delay_ms(1);
}

void LCD_Send4Bits(uint8_t data)
{
    LCD_Expander_Write(data);
    LCD_Pulse_Enable(data);
}

void LCD_Command(uint8_t command)
{
    uint8_t high = command & 0xF0U;
    uint8_t low = (command << 4) & 0xF0U;

    LCD_Send4Bits(high);
    LCD_Send4Bits(low);
}

void LCD_Data(uint8_t data)
{
    uint8_t high = (data & 0xF0U) | LCD_RS;
    uint8_t low = ((data << 4) & 0xF0U) | LCD_RS;

    LCD_Send4Bits(high);
    LCD_Send4Bits(low);
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
    delay_ms(1);

    LCD_Command(0x28);
    LCD_Command(0x0C);
    LCD_Command(0x06);
    LCD_Command(0x01);

    delay_ms(2);
}

void LCD_SetCursor(uint8_t row, uint8_t column)
{
    if(row == 0U)
        LCD_Command(0x80U + column);
    else
        LCD_Command(0xC0U + column);
}

void LCD_Print(char *text)
{
    while(*text)
        LCD_Data((uint8_t)*text++);
}

void LCD_PrintNumber(uint32_t number)
{
    char buffer[10];
    int i = 0;

    if(number == 0U)
    {
        LCD_Data('0');
        return;
    }

    while(number > 0U && i < 10)
    {
        buffer[i++] = (char)((number % 10U) + '0');
        number /= 10U;
    }

    while(i > 0)
        LCD_Data((uint8_t)buffer[--i]);
}

void LCD_Clear(void)
{
    LCD_Command(0x01);
    delay_ms(2);
}

/* ================= INA219 ================= */

void INA219_Init(void)
{
    I2C_Write16(INA_ADDR, 0x00, 0x399F);
    I2C_Write16(INA_ADDR, 0x05, 4096);
}

int16_t INA219_ReadCurrent(void)
{
    uint16_t raw = I2C_Read16(INA_ADDR, 0x04);
    return (int16_t)raw;
}

/* ================= DISPLAY FUNCTIONS ================= */

void DisplayIdle(void)
{
    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print("SYSTEM IDLE");

    LCD_SetCursor(1, 0);
    LCD_Print("SW1: START");
}

void DisplayReady(void)
{
    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print("SYSTEM READY");

    LCD_SetCursor(1, 0);
    LCD_Print("SW2: MEASURE");
}

void DisplayStopped(void)
{
    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print("SYSTEM STOPPED");

    LCD_SetCursor(1, 0);
    LCD_Print("SW1: START");
}

void DisplayCurrent(uint32_t current_mA)
{
    LCD_Clear();

    LCD_SetCursor(0, 0);
    LCD_Print("CURRENT:");
    LCD_PrintNumber(current_mA);
    LCD_Print("mA");

    LCD_SetCursor(1, 0);

    if(state.fault_detected)
    {
        /* Estimate distance: linearly from MAX_DISTANCE_CM at 0mA to 0cm at threshold */
        uint16_t distance_cm = (uint16_t)(
            (15U * (settings.current_threshold_ma - current_mA)) /
            settings.current_threshold_ma
        );

        LCD_Print("DIST: ");
        LCD_PrintNumber(distance_cm);
        LCD_Print(" cm");

        LED_Off(LED_GREEN);
    }
    else
    {
        LCD_Print("NO FAULT      ");
        LED_On(LED_GREEN);
    }
}

/* ================= FAULT LED ================= */

void UpdateFaultLED(void)
{
    if(state.fault_detected)
    {
        state.blink_elapsed += 100U;

        if(state.blink_elapsed >= 300U)
        {
            state.blink_elapsed = 0U;
            state.red_led_state = (uint8_t)!state.red_led_state;

            if(state.red_led_state)
                LED_On(LED_RED);
            else
                LED_Off(LED_RED);
        }
    }
    else
    {
        state.blink_elapsed = 0U;
        state.red_led_state = 0U;
        LED_Off(LED_RED);
    }
}

/* ================= STATE TRANSITION HELPERS ================= */

void TransitionToIdle(void)
{
    state.system_started = 0;
    state.reading_active = 0;
    state.fault_detected = 0;
    state.red_led_state = 0;
    state.blink_elapsed = 0;
    state.display_elapsed = 0;
    state.state = STATE_IDLE;

    LEDs_Off();
    DisplayStopped();
}

void TransitionToReady(void)
{
    state.system_started = 1;
    state.reading_active = 0;
    state.fault_detected = 0;
    state.red_led_state = 0;
    state.blink_elapsed = 0;
    state.display_elapsed = 0;
    state.state = STATE_READY;

    LEDs_Off();
    LED_On(LED_GREEN);
    DisplayReady();
}

void TransitionToMeasuring(void)
{
    LED_Off(LED_GREEN);
    LED_Off(LED_RED);
    LED_On(LED_YELLOW);

    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("MEASURING...");
    LCD_SetCursor(1, 0);
    LCD_Print("Please wait");

    delay_ms(500);

    LED_Off(LED_YELLOW);
    state.reading_active = 1;
    state.display_elapsed = 0;
    state.state = STATE_MEASURING;
}

void TransitionToStopped(void)
{
    state.system_started = 0;
    state.reading_active = 0;
    state.fault_detected = 0;
    state.red_led_state = 0;
    state.blink_elapsed = 0;
    state.display_elapsed = 0;
    state.state = STATE_STOPPED;

    LEDs_Off();
    DisplayStopped();
}

/* ================= MAIN ================= */

int main(void)
{
    int16_t current_raw;
    uint32_t current_mA;

    GPIO_Init();
    Test_LEDs();

    I2C1_Init();
    delay_ms(100);

    LCD_Init();
    delay_ms(100);

    INA219_Init();
    delay_ms(100);

    LCD_SetCursor(0, 0);
    LCD_Print("CABLE FAULT");
    LCD_SetCursor(1, 0);
    LCD_Print("DETECTION");
    delay_ms(1500);

    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("Press SW1 START");
    LCD_SetCursor(1, 0);
    LCD_Print("System OFF");

    for(;;)
    {
        /* START BUTTON */

        if(!state.system_started && ButtonPressed(SW_START))
        {
            TransitionToReady();
        }

        /* MEASURE BUTTON */

        if(state.system_started &&
           !state.reading_active &&
           ButtonPressed(SW_MEASURE))
        {
            TransitionToMeasuring();
        }

        /* STOP BUTTON */

        if(state.system_started && ButtonPressed(SW_STOP))
        {
            TransitionToIdle();
        }

        /* CURRENT MEASUREMENT */

        if(state.system_started && state.reading_active)
        {
            current_raw = INA219_ReadCurrent();

            if(current_raw < 0)
                current_mA = 0U;
            else
                current_mA = (uint32_t)current_raw / 10U;

            /*
               FAULT DETECTION:
               Current <= threshold: fault detected
               Current > threshold: no fault
            */

            if(current_mA <= settings.current_threshold_ma)
            {
                state.fault_detected = 1U;
                LED_Off(LED_GREEN);
            }
            else
            {
                state.fault_detected = 0U;
                state.red_led_state = 0U;
                state.blink_elapsed = 0U;

                LED_Off(LED_RED);
                LED_On(LED_GREEN);
            }

            UpdateFaultLED();

            state.display_elapsed += 100U;

            if(state.display_elapsed >= 500U)
            {
                state.display_elapsed = 0U;
                DisplayCurrent(current_mA);
            }

            delay_ms(100);
        }
    }
}