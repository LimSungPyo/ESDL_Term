/*수정된부분 
GPIO_Configure USART2 부분 추가
USART_Configure -> USART1_Configure (이름변경)
USAART2_Configure 추가(블루투스 연결)
SendData, SendString 함수 블루투스 출력 추가가
*/


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
uint32_t THRESHOLD = 3600; // 기준치

/* 
   PA5, PA6, PA7, PB0, PB1 5개의 채널 이용
   PA5(ADC_Value[0]) -> Sensor1
   PA6(ADC_Value[1]) -> Sensor2
   PA7(ADC_Value[2]) -> Sensor3
   PB0(ADC_Value[3]) -> Sensor4
   PB1(ADC_Value[4]) -> Sensor5
   초기에는 Sensor값이 모두 0으로 초기화된 상태
   각각의 조도센서가 레이저를 인식하면 인식한 조도센서의 값이 1로 업데이트됨 
   만약 레이저가 조도센서에 적중한다면 Currentstate가 1 증가 
*/
int Sensor1 = 0, Sensor2 = 0, Sensor3 = 0, Sensor4 = 0, Sensor5 = 0;
int PreviousState = 0, CurrentState = 0;

void RCC_Configure(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // GPIOA 클럭 활성화 : PIEZO, Start Button
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // GPIOB 클럭 활성화 : Analog Pin
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // GPIOC 클럭 활성화 : Fire Button
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE); // USART1 클럭 활성화

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE); // USART2 클럭 활성화(추가)
    
}

void GPIO_Configure(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // KEY 1 Fire Button
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;             // KEY 1 Fire Button (PC4)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    
    // KEY 4 game start Button 
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;             // KEY 4 game start Button (PA0)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 조도 센서 핀 설정 (PA5, PA6, PA7, PB0, PB1) Analog 핀
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7; // PA5 ~ PA7
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; // 아날로그 입력
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1; // PB0 ~ PB1
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; // 아날로그 입력
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // PIEZO (TIM2, 채널 2)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;       // 출력, 대체 기능, 푸시풀
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 레이저 (TIM3, 채널 1) - PB4 사용
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;              // 레이저 핀 (PB4)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;         // 대체 기능, 푸시풀
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;       // 속도 설정
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    /*    // TX (PA9) 설정
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 대체 기능 푸시풀
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // RX (PA10) 설정
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // 입력 모드
    GPIO_Init(GPIOA, &GPIO_InitStructure);*/

     // USART1 (TX: PA9, RX: PA10) 설정
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 대체 기능 푸시풀
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // 입력 모드
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // USART2 (TX: PA2, RX: PA3) 설정
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;  // TX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  // 대체 기능 푸시풀
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  // 속도 설정
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;  // RX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 입력 모드 (풀업/풀다운 없이)
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

// 레이저용 PWM 초기화 (TIM3, 채널 1)
void LaserShoot_Init(void) {
    // 레이저를 발사하는 초기화 함수 (예: PWM 신호 설정)
    // 타이머와 PWM을 설정하는 코드가 여기에 포함됩니다.
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);  // TIM3 클럭 활성화

    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // 기본 타이머 설정
    TIM_TimeBaseStructure.TIM_Period = 999;       // 자동 리로드 값 (1kHz 주파수)
    TIM_TimeBaseStructure.TIM_Prescaler = 71;     // 분주비 (1MHz 클럭)
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    // PWM 모드 설정 (채널 1, 50% 듀티)
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 500;          // 듀티 사이클 50% (레이저 발사)
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);

    // 타이머 활성화
    TIM_Cmd(TIM3, ENABLE);
}

// PWM 초기화 함수 (TIM2, 채널 2)
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
    TIM_OCInitStructure.TIM_Pulse = 500;          // 50% 듀티 사이클
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

/*void USART_Configure(void) {
    USART_InitTypeDef USART_InitStructure;

    USART_InitStructure.USART_BaudRate = 9600; // 보드레이트: 9600
    USART_InitStructure.USART_WordLength = USART_WordLength_8b; // 데이터 비트: 8비트
    USART_InitStructure.USART_StopBits = USART_StopBits_1; // 스탑 비트: 1비트
    USART_InitStructure.USART_Parity = USART_Parity_No; // 패리티 없음
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // 송수신 모드 활성화
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 플로우 제어 없음

    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE); // USART 활성화
}*/

void USART1_Configure(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // PA9 (TX)와 PA10 (RX) 설정
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;  // TX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  // 대체 기능 푸시풀
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;  // RX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 입력 모드
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // USART1 설정
    USART_InitStructure.USART_BaudRate = 9600;  // PUTTY와 같은 보드레이트
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

void USART2_Configure(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // PA2 (TX)와 PA3 (RX) 설정
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;  // TX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  // 대체 기능 푸시풀
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;  // RX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 입력 모드
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // USART2 설정
    USART_InitStructure.USART_BaudRate = 9600;  // 블루투스 모듈과 같은 보드레이트
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART2, &USART_InitStructure);
    USART_Cmd(USART2, ENABLE);
}


/* 각각 조도센서의 아날로그 값이 처음으로 임계점 아래로 떨어질때만 Sensor 값을 1로 업데이트하고 CurrentState를 1 증가시킴
   이를 통해 이미 적중시킨 조도센서를 다시 맞춰서 점수를 얻는것을 방지함 */
void UpdateSensorStates(void) 
{
    if (ADC_Value[0] < THRESHOLD && Sensor1 == 0)// PA5
    {
        Sensor1 = 1;
        CurrentState++;
    }
    if (ADC_Value[1] < THRESHOLD && Sensor2 == 0)// PA6
    {
        Sensor2 = 1;
        CurrentState++;
    }
    if (ADC_Value[2] < THRESHOLD && Sensor3 == 0)// PA7
    {
        Sensor3 = 1;
        CurrentState++;
    }
    if (ADC_Value[3] < THRESHOLD && Sensor4 == 0)// PB0
    {
        Sensor4 = 1;
        CurrentState++;
    }
    if (ADC_Value[4] < THRESHOLD && Sensor5 == 0)// PB1
    {
        Sensor5 = 1;
        CurrentState++;
    }
}

void delay()
{
    int i = 0;
    for(i=0; i<3000000; i++){}
}

void SendData(uint16_t data) 
{
    // PUTTY에 전송 (USART1)
    USART1->DR = data;
    while ((USART1->SR & USART_SR_TC) == 0);  // 전송 완료 대기

    // 블루투스에 전송 (USART2)
    USART2->DR = data;
    while ((USART2->SR & USART_SR_TC) == 0);  // 전송 완료 대기
}

void SendString(const char *str) {
  while(*str) {
    SendData(USART1, *str); // PuTTY 출력
    SendData(USART2, *str); // 블루투스 출력
    str++;
  }
}

int main() {
    SystemInit();
    RCC_Configure();
    GPIO_Configure();     // GPIO 클럭 초기화
    ADC_Configure();
    DMA_Configure();
    PWM_Init_Config();   // PWM 초기화
    LaserShoot_Init();
    USART1_Configure();
    USART2_Configure(); // 블루투스 USART 초기화

    int bullet =  5;
    int point = 0;
    while(1)
    {
      bullet=5;
      point = 0;

      //start, key4
      while(1) 
      {   
        TIM_SetCompare2(TIM2, 0);   // 듀티 0% (소리 OFF)
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_RESET){
            SendString("===========[Game Start]===========\r\n");
            break;
        }
      }
      //gaming
      while(1)
      {
          //보드의 버튼을 누르면 총알 발사.
          //PreviousState != Currentstate -> 조도센서에 레이저가 적중했다
          //PreviousState == Currentstate -> 조도센서에 레이저가 적중하지 않았다
          //적중했다면 PreviousState를 Currentstate값으로 업데이트, 다시 계속 비교
          if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4) == Bit_RESET) // KEY1을 눌렀을 시
          {
              TIM_SetCompare2(TIM2, 500);
              SendString("===========[Fired]===========\r\n");
              TIM_SetCompare1(TIM3, 500); // 레이저 발사
              UpdateSensorStates();
              if (PreviousState != CurrentState) { // 조도센서에 레이저가 적중했다면,
                  SendString("if case\r\n");
                  point += 20;
                  TIM_SetCompare2(TIM2, 500); // 50% 듀티 (소리 ON)
                  PreviousState = CurrentState;
              } else { // 빗나갔다면
                  
              }
              bullet--;
              delay();
          } else {
              TIM_SetCompare2(TIM2, 0);   // 듀티 0% (소리 OFF)
              TIM_SetCompare1(TIM3, 0); // 레이저 0% (레이저 OFF)
          }
          if(bullet == 0){
              SendString(" ===========[Finish]===========\r\n");
              for (int i = 0; i < sizeof(msg_end); i++) {
                  SendData(msg_end[i]);
              }
              if(point == 0){
                SendString(" 0\r\n");
              } else if (point == 100) {
                SendString(" 100\r\n");
              } else {
                SendString(" ");
                SendData(point/10 + 48);
                SendString("0\r\n");
              }
            break;
          }     
      }
    }
    return 0;
}
