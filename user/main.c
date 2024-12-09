#include "stm32f10x.h"
#include "core_cm3.h"
#include "misc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_tim.h"

uint32_t flag = 0;
uint32_t light;
uint16_t value = 0;
volatile uint32_t ADC_Value[5];
uint32_t THRESHOLD = 3400; // 기준치

/* PA5, PA6, PA7, PB0, PB1 5개의 채널 이용
   PA5(ADC_Value[0]) -> Sensor1
   PA6(ADC_Value[1]) -> Sensor2
   PA7(ADC_Value[2]) -> Sensor3
   PB0(ADC_Value[3]) -> Sensor4
   PB1(ADC_Value[4]) -> Sensor5
   초기에는 Sensor값이 모두 0으로 초기화된 상태
   각각의 조도센서가 레이저를 인식하면 인식한 조도센서의 값이 1로 업데이트됨 */
int Sensor1 = 0, Sensor2 = 0, Sensor3 = 0, Sensor4 = 0, Sensor5 = 0;

void RCC_Configure(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // GPIOA 클럭 활성화 : PIEZO, Start Button
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // GPIOB 클럭 활성화 : Analog Pin
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // GPIOC 클럭 활성화 : Fire Button
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
}

void GPIO_Configure(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    // KEY 1 Fire Button
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;             // KEY 1 Fire Butto
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    // KEY 4 game start Button 
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;             // KEY 4 game start Button 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    // 조도 센서 핀 설정 (PA5, PA6, PA7, PB0, PB1)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7; // PA5 ~ PA7
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; // 아날로그 입력
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1; // PB0 ~ PB1
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; // 아날로그 입력
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    // PIEZO
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;       // 출력, 대체 기능, 푸시풀
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void ADC_Configure(void) 
{
    ADC_InitTypeDef ADC_InitStruct;

    ADC_InitStruct.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStruct.ADC_ScanConvMode = ENABLE;
    ADC_InitStruct.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStruct.ADC_NbrOfChannel = 5; // 총 5개의 채널 사용

    ADC_Init(ADC1, &ADC_InitStruct);

    // 채널 5~9 설정 (PA5, PA6, PA7, PB0, PB1)
    ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 1, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_6, 2, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_7, 3, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_8, 4, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_9, 5, ADC_SampleTime_239Cycles5);

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1)) {}

    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1)) {}

    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

}

// PIEZO용 PWM 초기화 함수 (TIM2, 채널 2)
void PWM_Init_Config(void) {
    // TIM2 클럭 활성화
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // 기본 타이머 설정
    TIM_TimeBaseStructure.TIM_Period = 999;       // 자동 리로드 값 (1kHz 주파수)
    TIM_TimeBaseStructure.TIM_Prescaler = 71;     // 분주비 (1MHz 클럭)
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // PWM 모드 설정 (채널 2, 50% 듀티)
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 500;          // 100% 듀티 사이클
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);

    // 타이머 활성화
    TIM_Cmd(TIM2, ENABLE);
}

void DMA_Configure(void)
{
    DMA_InitTypeDef DMA_Instructure;

    DMA_Instructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_Instructure.DMA_MemoryBaseAddr = (uint32_t)&ADC_Value[0];

    DMA_Instructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_Instructure.DMA_BufferSize = 5;
    DMA_Instructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_Instructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_Instructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    DMA_Instructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    DMA_Instructure.DMA_Mode = DMA_Mode_Circular;
    DMA_Instructure.DMA_Priority = DMA_Priority_High;
    DMA_Instructure.DMA_M2M = DMA_M2M_Disable;

    DMA_Init(DMA1_Channel1, &DMA_Instructure);

    DMA_Cmd(DMA1_Channel1, ENABLE);
}

void UpdateSensorStates(void) 
{
    if (ADC_Value[0] < THRESHOLD) Sensor1 = 1;// PA5
    if (ADC_Value[1] < THRESHOLD) Sensor2 = 1;// PA6
    if (ADC_Value[2] < THRESHOLD) Sensor3 = 1;// PA7
    if (ADC_Value[3] < THRESHOLD) Sensor4 = 1;// PB0
    if (ADC_Value[4] < THRESHOLD) Sensor5 = 1;// PB1
}

void delay()
{
    int i = 0;
    for(i=0; i<3000000; i++){}
}

int main() {

    SystemInit();
    RCC_Configure();
    GPIO_Configure();     // GPIO 클럭 초기화
    ADC_Configure();
    DMA_Configure();
    PWM_Init_Config();   // PWM 초기화

    int bullet =  10;
    int start = 0;
    while(1) 
    {
        TIM_SetCompare2(TIM2, 0);   // 듀티 0% (소리 OFF)
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_RESET) break;
        UpdateSensorStates();
    }

    while(1){
    //delay();
    //보드의 버튼을 누르면 총알 발사.

    if (
        GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4) == Bit_RESET) { // KEY1을 눌렀을 시,
        flag = (ADC_Value[0] < THRESHOLD) ? 1: 0;
        if (flag) { // 조도센서에 레이저가 적중했다면,
            TIM_SetCompare2(TIM2, 500); // 50% 듀티 (소리 ON)
        } else { // 빗나갔다면
        
        }
    } else {
        TIM_SetCompare2(TIM2, 0);   // 듀티 0% (소리 OFF)
    }
    /*

    */
    }
    return 0;
}