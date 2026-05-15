#include <stm32f0xx.h>
#include "clock_.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


#define USART2_RX_PIN 3
#define USART2_TX_PIN 2

// LEDs: I/O Board - Shield 2
#define LED_R_PIN 1  // LED_RED    -> PC1
#define LED_G_PIN 6  // LED_GREEN  -> PB6
#define LED_B_PIN 10 // LED_BLUE   -> PA10

// Motor: H-Bridge Board - Shield 1
#define MOTOR_PIN_IN_1 8  // PC8  -> PWM Signal zum steuern der Drehzahl
#define MOTOR_PIN_IN_2 4  // PA4  -> Drehrichtung: 0...Rechtslauf, 1...Linkslauf
#define MOTOR_SLEEP_IN 0  // PB0  -> Motor bei 1 aktiv und 0 deaktiv


#define LOG(msg...) printf(msg);
#define BAUDRATE 9600  // Select the Baudrate for the UART
#define FIFO_SIZE 64

typedef struct {uint8_t data[FIFO_SIZE]; uint8_t head; uint8_t tail;} FIFO_t;

typedef enum {
    WAIT_HEADER,
    READ_ID,
    READ_LEN,
    READ_PAYLOAD,
    READ_CRC,
    WAIT_EOF
} FSM_t;

void GPIO_Init(void);
void UART_Init(void);
void PWM_Init(void);
void FSM_Process(void);

void FIFO_Write(uint8_t d);
uint8_t FIFO_Read(void);
uint8_t FIFO_Empty(void);

void Motor_Control(uint8_t dir, uint16_t speed);
void LED_Control(char c, uint8_t s);

uint8_t CRC_Calc(uint8_t *data, uint8_t len);

FIFO_t fifo = {0};

FSM_t state = WAIT_HEADER;

char id[4];
uint8_t len;
uint8_t payload[16];
uint8_t payload_i;
uint8_t crc_rx;

int main(void) {
    SystemCoreClockUpdate();

    GPIO_Init();
    UART_Init();
    PWM_Init();

    while (1) {
        FSM_Process();
    }
}

void GPIO_Init(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN |
                   RCC_AHBENR_GPIOBEN |
                   RCC_AHBENR_GPIOCEN;

    GPIOA->MODER &= ~(3 << (USART2_TX_PIN * 2));
    GPIOA->MODER |=  (2 << (USART2_TX_PIN * 2));

    GPIOA->MODER &= ~(3 << (USART2_RX_PIN * 2));
    GPIOA->MODER |=  (2 << (USART2_RX_PIN * 2));

    GPIOA->AFR[0] |= (1 << (USART2_TX_PIN * 4)) |
                     (1 << (USART2_RX_PIN * 4));

    GPIOC->MODER |= (1 << (LED_R_PIN * 2));
    GPIOB->MODER |= (1 << (LED_G_PIN * 2));
    GPIOA->MODER |= (1 << (LED_B_PIN * 2));

    GPIOA->MODER |= (1 << (MOTOR_PIN_IN_2 * 2));
    GPIOB->MODER |= (1 << (MOTOR_SLEEP_IN * 2));

    GPIOC->MODER &= ~(3 << (MOTOR_PIN_IN_1 * 2));
    GPIOC->MODER |=  (2 << (MOTOR_PIN_IN_1 * 2));

    GPIOC->AFR[1] |= (0x0 << ((MOTOR_PIN_IN_1 - 8) * 4));
}

void UART_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    USART2->BRR = 48000000 / BAUDRATE;

    USART2->CR1 |= USART_CR1_RE |
                   USART_CR1_TE |
                   USART_CR1_RXNEIE |
                   USART_CR1_UE;

    NVIC_EnableIRQ(USART2_IRQn);
}

void PWM_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    TIM3->PSC = 0;
    TIM3->ARR = 65535;

    TIM3->CCMR2 |= (6 << 4);
    TIM3->CCER  |= TIM_CCER_CC3E;

    TIM3->CCR3 = 0;

    TIM3->CR1 |= TIM_CR1_CEN;
}

void USART2_IRQHandler(void) {
    if (USART2->ISR & USART_ISR_RXNE)
        FIFO_Write((uint8_t)USART2->RDR);
}

void FSM_Process(void)
{
    static char line[64];
    static uint8_t idx = 0;

    if (FIFO_Empty()) return;

    char c = FIFO_Read();

    if (c == '\n' || c == '\r') {

        line[idx] = 0;

        if (strncmp(line, "LEDR=", 5) == 0)
            LED_Control('R', atoi(&line[5]));

        else if (strncmp(line, "LEDG=", 5) == 0)
            LED_Control('G', atoi(&line[5]));

        else if (strncmp(line, "LEDB=", 5) == 0)
            LED_Control('B', atoi(&line[5]));

        else if (strncmp(line, "DIR=", 4) == 0) {

            uint8_t dir = atoi(&line[4]);

            if (dir)
                GPIOA->BSRR = (1 << MOTOR_PIN_IN_2);
            else
                GPIOA->BSRR = (1 << (MOTOR_PIN_IN_2 + 16));
        }

        else if (strncmp(line, "MOTOR=", 6) == 0) {

            uint16_t speed = atoi(&line[6]);

            GPIOB->BSRR = (1 << MOTOR_SLEEP_IN);

            TIM3->CCR3 = speed;

            printf("speed:%u\n", speed);
        }

        idx = 0;
    }
    else {

        if (idx < sizeof(line) - 1)
            line[idx++] = c;
    }
}

void Motor_Control(uint8_t dir, uint16_t speed) {
    GPIOB->BSRR = (1 << MOTOR_SLEEP_IN);

    if (dir == 0x0F)
        GPIOA->BSRR = (1 << MOTOR_PIN_IN_2);
    else
        GPIOA->BSRR = (1 << (MOTOR_PIN_IN_2 + 16));

    TIM3->CCR3 = speed;

    printf("speed:%u\n", speed);
    printf("dir:%u\n", dir);
}

void LED_Control(char c, uint8_t s) {

    if (c == 'R')
        GPIOC->BSRR =
            (s ? (1 << LED_R_PIN) : (1 << (LED_R_PIN + 16)));

    if (c == 'G')
        GPIOB->BSRR =
            (s ? (1 << LED_G_PIN) : (1 << (LED_G_PIN + 16)));

    if (c == 'B')
        GPIOA->BSRR =
            (s ? (1 << LED_B_PIN) : (1 << (LED_B_PIN + 16)));
}

int _write(int file, char *ptr, int len) {
    for (int i = 0; i < len; i++) {
        while (!(USART2->ISR & USART_ISR_TXE));
        USART2->TDR = ptr[i];
    }

    return len;
}

void FIFO_Write(uint8_t d) {
    fifo.data[fifo.head] = d;
    fifo.head = (fifo.head + 1) % FIFO_SIZE;
}

uint8_t FIFO_Read(void) {
    uint8_t d = fifo.data[fifo.tail];
    fifo.tail = (fifo.tail + 1) % FIFO_SIZE;
    return d;
}

uint8_t FIFO_Empty(void) {
    return fifo.head == fifo.tail;
}

uint8_t CRC_Calc(uint8_t *data, uint8_t len) {
    RCC->AHBENR |= RCC_AHBENR_CRCEN;

    CRC->CR = CRC_CR_RESET;

    for (int i = 0; i < len; i++)
        CRC->DR = data[i];

    return (uint8_t)(CRC->DR & 0xFF);
}