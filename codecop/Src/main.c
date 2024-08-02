#include <stdint.h>
#include "stm32f446xx.h"
#include <math.h>

void generate_sine_wave(void);
void generate_square_wave(void);
void generate_triangle_wave(void);
void generate_rising_sawtooth(void);
void generate_falling_sawtooth(void);
void generate_rising_ramp(void);
void generate_falling_ramp(void);

volatile int waveform_index = 0;
volatile int analog = 0;
volatile uint32_t frequency = 100000;                     // Initial frequency
volatile int external_led_state = 0;                      // State of the external LED

void EXTI15_10_IRQHandler(void)
{
    if (EXTI->PR & (1 << 13))                             // Check if EXTI line 13 is pending
    {
        EXTI->PR |= (1 << 13);                           // Clear the pending bit
        waveform_index = (waveform_index + 1) % 7;        // Cycle through waveforms
        GPIOA->ODR |= (1 << 5);                          // Turn on onboard LED (LD2)
        delay(100000);                                  // Short delay
        GPIOA->ODR &= ~(1 << 5);                        // Turn off onboard LED (LD2)
    }
}

void EXTI0_IRQHandler(void)
{
    if (EXTI->PR & (1 << 0))                  // Check if EXTI line 0 is pending
    {
        EXTI->PR |= (1 << 0);                  // Clear the pending bit
        frequency = (frequency == 100000) ? 50000 : 100000;  // Toggle frequency between 100000 and 50000
        external_led_state = !external_led_state;        // Toggle external LED state
        if (external_led_state)
        {
            GPIOA->ODR |= (1 << 1);                   // Turn on external LED
        }
        else
        {
            GPIOA->ODR &= ~(1 << 1); // Turn off external LED
        }
    }
}

void init_button(void)
{
    RCC->AHB1ENR |= (1 << 2);                         // Enable GPIOC clock
    GPIOC->MODER &= ~(3 << 26);                    // Set PC13 to input mode
    GPIOC->PUPDR |= (1 << 26);                      // Enable pull-up resistor

    RCC->APB2ENR |= (1 << 14);
    SYSCFG->EXTICR[3] |= (2 << 4);                           // Connect EXTI line 13 to PC13

    EXTI->IMR |= (1 << 13);                                // Unmask EXTI line 13
    EXTI->FTSR |= (1 << 13);                          // Trigger on falling edge

    NVIC_EnableIRQ(EXTI15_10_IRQn);                    // Enable EXTI line 15-10 interrupt
}

void init_frequency_button(void)
{
    RCC->AHB1ENR |= (1 << 0);                                    // Enable GPIOA clock
    GPIOA->MODER &= ~(3 << 0);                                  // Set PA0 to input mode
    GPIOA->PUPDR |= (1 << 0);                                   // Enable pull-up resistor

    RCC->APB2ENR |= (1 << 14);                                  // Enable SYSCFG clock
    SYSCFG->EXTICR[0] |= (0 << 0);                             // Connect EXTI line 0 to PA0

    EXTI->IMR |= (1 << 0);                                       // Unmask EXTI line 0
    EXTI->FTSR |= (1 << 0);                                     // Trigger on falling edge

    NVIC_EnableIRQ(EXTI0_IRQn);                                    // Enable EXTI line 0 interrupt
}

void init_led(void)
{
    RCC->AHB1ENR |= (1 << 0);                                          // Enable GPIOA clock
    GPIOA->MODER |= (1 << 10);                                       // Set PA5 to output mode (onboard LED)
    GPIOA->MODER |= (1 << 2);                                        // Set PA1 to output mode (external LED)
}

void delay(volatile uint32_t count)
{
    while (count--) {}
}

int main(void) {   uint32_t dac_val = 0x0;

    // Enable GPIOA clock, bit 0 on AHB1ENR
    RCC->AHB1ENR |= (1 << 0);

    GPIOA->MODER &= ~(3 << 8);                                         // Reset bits 8-9 to clear old values
    GPIOA->MODER |= (3 << 8);                                          // Set pin 4 to analog mode (0b11)

                                                                             // Enable DAC clock, bit 29 on APB1ENR
    RCC->APB1ENR |= (1 << 29);

    DAC->CR |= (1 << 0);                                                       // Enable DAC channel 1
    DAC->CR &= ~(1 << 1);                                                  // Enable DAC ch1 output buffer
    DAC->CR |= (1 << 2);                                                 // Enable trigger
    DAC->CR |= (7 << 3);                                                 // Choose sw trigger as source (0b111)

    // Set output to Vref * (dac_value/0xFFF)
    DAC->DHR12R1 = dac_val;
    DAC->SWTRIGR |= (1 << 0); // Trigger ch

    init_button();
    init_frequency_button();
    init_led();

    while (1)
    {
        switch (waveform_index)
        {
            case 0: generate_sine_wave(); break;
            case 1: generate_square_wave(); break;
            case 2: generate_triangle_wave(); break;
            case 3: generate_rising_sawtooth(); break;
            case 4: generate_falling_sawtooth(); break;
            case 5: generate_rising_ramp(); break;
            case 6: generate_falling_ramp(); break;
        }
        delay(frequency); // Use the frequency variable for delay
    }
    return 0;
}

void generate_sine_wave(void)
{
    static float angle = 0.0;
    uint32_t dac_val = (uint32_t)((sin(angle) + 1) * 2047.5); // Scale to 0-4095
    DAC->DHR12R1 = dac_val;
    DAC->SWTRIGR |= (1 << 0); // Trigger DAC
    analog = dac_val;
    angle += 0.1;
    if (angle > 2 * M_PI) angle = 0;
}

void generate_square_wave(void)
{
    static int state = 0;
    uint32_t dac_val = state ? 4095 : 0;
    DAC->DHR12R1 = dac_val;
    DAC->SWTRIGR |= (1 << 0); // Trigger DAC
    analog = dac_val;
    state = !state;
}

void generate_triangle_wave(void)
{
    static int direction = 1;
    static uint32_t dac_val = 0;
    dac_val += direction * 100;
    if (dac_val >= 4095 || dac_val <= 0)
    	direction = -direction;
    DAC->DHR12R1 = dac_val;
    DAC->SWTRIGR |= (1 << 0); // Trigger DAC
    analog = dac_val;
}

void generate_rising_sawtooth(void)
{
    static uint32_t dac_val = 0;
    dac_val += 100;
    if (dac_val >= 4095) dac_val = 0;
    DAC->DHR12R1 = dac_val;
    DAC->SWTRIGR |= (1 << 0); // Trigger DAC
    analog = dac_val;
}

void generate_falling_sawtooth(void)
{
    static uint32_t dac_val = 4095;
    if (dac_val >= 100) {
        dac_val -= 100;
    } else {
        dac_val = 4095;
    }
    DAC->DHR12R1 = dac_val;
    DAC->SWTRIGR |= (1 << 0); // Trigger DAC
    analog = dac_val;
}

void generate_rising_ramp(void)
{
    static uint32_t dac_val = 0;
    dac_val += 50;
    if (dac_val >= 4095)
    	dac_val = 0;
    DAC->DHR12R1 = dac_val;
    DAC->SWTRIGR |= (1 << 0); // Trigger DAC
    analog = dac_val;
}

void generate_falling_ramp(void)
{
    static uint32_t dac_val = 4095;
    dac_val -= 50;
    if (dac_val <= 0)
    	dac_val = 4095;
    DAC->DHR12R1 = dac_val;
    DAC->SWTRIGR |= (1 << 0); // Trigger DAC
    analog = dac_val;
}
