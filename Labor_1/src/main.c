#include "includes.h"
 
#define SPEAKER_PIN 8   // PC8 
#define     Sw1_PIN 9   // PC9  (Sw1 -> Delay)  
#define     Sw2_PIN 6   // PC6  (Sw2 -> Delay)

#define      MARIO_LENGTH  (sizeof(     mario_noten) / sizeof(     mario_noten[0]))    
#define UNDERWORLD_LENGTH  (sizeof(underworld_noten) / sizeof(underworld_noten[0]))

volatile uint8_t melodyChanged = 0; volatile uint8_t currentMelody = 0; // 0 = Mario, 1 = Underground

// Function_Prototypes
void systick_init(void); void delayMicroseconds(uint32_t us); void gpio_init(void); void exti_init(void); void playMelody(const int *melody, const int *tempo, int length); void playTone(uint32_t freq, uint32_t duration_us);


int main(void) {
    systick_init(); gpio_init(); exti_init();
    while (1) {melodyChanged = 0;  // because melodyChanged stops the playMelody-function imediately
        if      (currentMelody == 0)  {playMelody(     mario_noten,      mario_tempo,      MARIO_LENGTH );} 
        else if (currentMelody == 1)  {playMelody(underworld_noten, underworld_tempo, UNDERWORLD_LENGTH );}}   }


void systick_init(void) {
    SysTick->CTRL  = 0;             // deactivate
    SysTick->LOAD  = 0x00FFFFFF;    // 2^24 set to maximum
    SysTick->VAL   = 0;             // current value to zero
    SysTick->CTRL |= (1 << 2);      // CPU clock
    SysTick->CTRL |= (1 << 0);  }   // enable
   

void gpio_init(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    
    GPIOC->MODER |=  (1 << (SPEAKER_PIN * 2));     // PC8 als Output (Speaker)
    GPIOC->MODER &= ~(3 << (    Sw1_PIN * 2));     // PC9 als Input
    GPIOC->MODER &= ~(3 << (    Sw2_PIN * 2));  }  // PC6 als Input


void exti_init(void) {
    RCC->APB2ENR      |= RCC_APB2ENR_SYSCFGCOMPEN;

    SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI6_PC;    // PC6 -> EXTI6
    SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI9_PC;    // PC9 -> EXTI9

    EXTI->IMR  |= (1 << Sw2_PIN) | (1 << Sw1_PIN);
    EXTI->RTSR |= (1 << Sw2_PIN) | (1 << Sw1_PIN);

    NVIC_EnableIRQ(EXTI4_15_IRQn);  }


void delayMicroseconds(uint32_t us) {
    uint32_t ticks = us * 8;  // 8 MHz
    uint32_t start = SysTick->VAL;
    while  ((start - SysTick->VAL) < ticks) {}  }    //  & 0x00FFFFFF)


void EXTI4_15_IRQHandler(void) {
    if (EXTI->PR &  (1 << Sw1_PIN)) {
        EXTI->PR |= (1 << Sw1_PIN);

        currentMelody ^= 1;
        melodyChanged  = 1;  }

    if (EXTI->PR  & (1 << Sw2_PIN)) {
        EXTI->PR |= (1 << Sw2_PIN);
        
        delayMicroseconds(2000000); }      }


void playMelody(const int *melody, const int *tempo, int length) {
    for (int i = 0; i < length; i++) {
        if (melodyChanged) return;                       // back to main imediately
        int noteDuration = (60000000 / 50) / tempo[i];   // tempo =^ Notenwert (je höher das Tempo, desto kürzer die Dauer) !, Notendauer = Tondauer = Basisdauer/Notenwert
        playTone(melody[i], noteDuration);
        int breakTime = 1000000 / tempo[i];
        delayMicroseconds(breakTime); }  }


void playTone(uint32_t freq, uint32_t duration_us) {
    if (freq == 0) {delayMicroseconds(duration_us);return; }
    uint32_t period = 1000000 / freq;                       // Periodendauer
    for (uint32_t i = 0; i < duration_us / period; i++) {   // duration / period = Anzahl Perioden pro Ton
        GPIOC->ODR ^= (1 << SPEAKER_PIN);                   // toggeln
        delayMicroseconds(period / 2); }    }