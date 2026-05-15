#include <stm32f0xx.h>
#include "clock_.h"
#include <stdio.h>

#define LOG(msg...) printf(msg);

// Select the Baudrate for the UART
#define BAUDRATE 9600

//////////////////////////////////////////////////////////////////////////////
// Predefined Programm functions
//////////////////////////////////////////////////////////////////////////////

/**
 * @brief To support the printf function we override the _write function to redirect the Output to UART
 * @param handle is typically ignored in this context, as we're redirecting all output to USART2
 * @param data is a pointer to the buffer containing the data to be send
 * @param size is the number of the bytes to send
 * @return Returns the total number of bytes send
 */
int _write(int handle, char *data, int size)
{
    int count = size; // Make a copy of the size to use in the loop
    // Loop through each byte in the data buffer
    while (count--)
    {
        // Wait until the transmit data register (TDR) is empty,
        // indicating that USART2 is ready to send a new byte
        while (!(USART2->ISR & USART_ISR_TXE))
        {
            // Wait here (busy wait) until TXE (Transmit Data Register Empty) flag is set
        }
        // Load the next byte of data into the transmit data register (TDR)
        // This sends the byte over UART
        USART2->TDR = *data++;
        // The pointer 'data' is incremented to point to the next byte to send
    }
    return size;
}

//////////////////////////////////////////////////////////////////////////////
// Main Program
//////////////////////////////////////////////////////////////////////////////
int main(void)
{
    // Configure the system clock to 48MHz (defined in a separate function)
    SystemClock_Config();

    // ---------------- Variable definitions ----------------
    // Define constants for the USART2 RX and TX pin numbers on GPIOA
    const uint8_t USART2_RX_PIN = 3;
    const uint8_t USART2_TX_PIN = 2;
    // Define a constant for the LED Output pin number
    const uint8_t LED_Pin = 5;

    // Variable to store received byte
    uint8_t rxb = '\0';

    // ---------------- Clock Peripheral actions ----------------
    // Enable the clock for GPIOA peripheral (used for USART2 pins PA2 and PA3)
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    // Enable the clock for USART2 peripheral
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // ---------------- UART TX Pin Configuration (PA2) ----------------
    // Set PA2 mode to 'Alternate Function' (MODER bits = 10)
    GPIOA->MODER |= 0b10 << (USART2_TX_PIN * 2);

    // Select AF1 (Alternate Function 1) for PA2 (AFR[0] corresponds to pins 0–7)
    GPIOA->AFR[0] |= 0b0001 << (4 * USART2_TX_PIN);

    // ---------------- UART RX Pin Configuration (PA3) ----------------
    // Set PA3 mode to 'Alternate Function' (MODER bits = 10)
    GPIOA->MODER |= 0b10 << (USART2_RX_PIN * 2);

    // Select AF1 (Alternate Function 1) for PA3
    GPIOA->AFR[0] |= 0b0001 << (4 * USART2_RX_PIN);

    // ---------------- USART2 Configuration ----------------

    // Set the baud rate (BRR = APB frequency divided by desired baud rate)
    // APB_FREQ and BAUDRATE are assumed to be defined elsewhere
    USART2->BRR = (APB_FREQ / BAUDRATE);

    // Enable USART2 receiver by setting RE bit in CR1 (bit 2)
    USART2->CR1 |= 0b1 << 2;

    // Enable USART2 transmitter by setting TE bit in CR1 (bit 3)
    USART2->CR1 |= 0b1 << 3;

    // Enable USART2 by setting UE bit in CR1 (bit 0)
    USART2->CR1 |= 0b1 << 0;

    // ---------------- Output LED Pin Configuration ----------------
    // Set the Pin Mode (Input, Output, Analog, Alternate Function) in the Moder Register
    GPIOA->MODER |= 0b01 << (LED_Pin * 2);
    // Set the Output type of the Port in the OTYPER Register
    GPIOA->OTYPER &= ~(0b1 << (LED_Pin));

    // ---------------- Continuous Loop - Receive data and set the LED  ----------------
    for (;;)
    {
        // Wait until the RXNE (Receive Not Empty) flag is set (bit 5 in ISR)
        // This indicates that data has been received and is ready to be read
        while (!(USART2->ISR & (0b1 << 5)))
        {
            // Wait for data to be received
        }
        // Read the received byte from the RDR (Receive Data Register)
        rxb = USART2->RDR;
        // Log the received byte using printf via the LOG macro
        // The printf function is supported through the custom _write() function,
        // which redirects output to UART (USART2)
        if ((rxb == 49))
        {
            GPIOA->BRR &= ~(0b1 << LED_Pin);
            GPIOA->BSRR |= 0b1 << LED_Pin;
            LOG("[DEBUG-LOG]: LED on (%d)\r\n", rxb);
        }
        else
        {
            GPIOA->BRR |= 0b1 << LED_Pin;
            GPIOA->BSRR &= ~(0b1 << LED_Pin);
            LOG("[DEBUG-LOG]: LED off (%d)\r\n", rxb);
        }
    }

}