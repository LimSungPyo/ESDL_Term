#include "stm32f10x.h"
#include "core_cm3.h"
#include "misc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_exti.h"

uint32_t flag = 0;
uint32_t light;
uint16_t value = 0;
//5개의 조도센서 별 빛이 입력된 기준 값
volatile uint32_t ADC_Value[5];
uint32_t THRESHOLD1 = 4050; // 기준치
uint32_t THRESHOLD2 = 4050; // 기준치
uint32_t THRESHOLD3 = 4050; // 기준치
uint32_t THRESHOLD4 = 4050; // 기준치
uint32_t THRESHOLD5 = 4050; // 기준치


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
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // LED
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE); // USART1 클럭 활성화
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE); // USART2 클럭 활성화
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE); //AFIO기능 사용 -> GPIO를 외부 인터럽트로 사용
}

/*
관련된 핀 설정
KEY1과 KEY4에 발사버튼과 게임 시작버튼 매칭 -> BUTTON을 따로 두어서 보드에 버튼을 누르지 않아도 작동하도록 설정
PA5-7, PB0-1에 조도센서 핀 설정
PA1에 PIEZO출력 데이터 핀 설정
PA9-10에 USART1데이터 전달 핀 설정
PA2-3에 USART2데이터_Bluetooth 데이터 전달 핀 설정
PD8-12에 LED출력 핀 설정
*/
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
    
    // TX (PA9) 설정
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 대체 기능 푸시풀
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // RX (PA10) 설정
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 입력 모드
    //MODE_IN_FLOATING -> 외부 회로에 의존된 상태
    //외부 회로로 신호 안정성이 보장되는 경우 사용한다고 함.
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    /* USART2 pin setting */
    //TX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    //RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    //LED
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12; // PD8 ~ PD12
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_SetBits(GPIOD, GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12);
}

//ADC를 통해서 조도센서의 값을 디지털로 변환
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

//USART의 인터럽트 관련 우선순위 설정
//Group2로 Preemption과 sub에 각각 2비트씩 할당
//USART1에 0, 0으로 인터럽트 우선순위 설정
//USART2에 1, 1으로 인터럽트 우선순위 설정
void NVIC_Configure(void) {

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    // USART1, PC
    // 'NVIC_EnableIRQ' is only required for USART setting
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // USART2, Bluetooth
    // 'NVIC_EnableIRQ' is only required for USART setting
    NVIC_EnableIRQ(USART2_IRQn);
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

// PWM 초기화 함수 (TIM2, 채널 2)
// 펄스 폭 변조를 통해서 PIEZO의 sound를 출력
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

//DMA를 통해서 직접 메모리 접근을 수행
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

//USART1, 2에 대한 값 설정
void USART_Configure(void) {
    USART_InitTypeDef USART1_InitStructure;
    USART_Cmd(USART1, ENABLE);
    USART1_InitStructure.USART_BaudRate = 9600; // 보드레이트: 9600
    USART1_InitStructure.USART_WordLength = USART_WordLength_8b; // 데이터 비트: 8비트
    USART1_InitStructure.USART_StopBits = USART_StopBits_1; // 스탑 비트: 1비트
    USART1_InitStructure.USART_Parity = USART_Parity_No; // 패리티 없음
    USART1_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // 송수신 모드 활성화
    USART1_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 플로우 제어 없음
    USART_Init(USART1, &USART1_InitStructure);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    USART_InitTypeDef USART2_InitStructure;
    USART_Cmd(USART2, ENABLE);
    USART2_InitStructure.USART_BaudRate = 9600;
    USART2_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART2_InitStructure.USART_StopBits =USART_StopBits_1;
    USART2_InitStructure.USART_Parity =USART_Parity_No;
    USART2_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART2_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART2, &USART2_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
}

//기본적인 LED핀 설정 -> 전부 불이 켜지도록
//Sensor의 값들을 조정 -> 만약 조도센서에 불이 비춰진 경우, Sensor의 값을 조정
void LEDInit()
{
    GPIO_SetBits(GPIOD, GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12);
    PreviousState = 0, CurrentState = 0;
    Sensor1 = 0;
    Sensor2 = 0;
    Sensor3 = 0;
    Sensor4 = 0;
    Sensor5 = 0;
}

//Bluetooth에서 PC의 Putty로 입력한 데이터 전달
void USART1_IRQHandler() {
  uint16_t word;
  if(USART_GetITStatus(USART1,USART_IT_RXNE)!=RESET){
    word = USART_ReceiveData(USART1);

    USART_SendData(USART2,word);
    USART_ClearITPendingBit(USART1,USART_IT_RXNE);
  }
}
//PC의 Putty로 입력한 데이터를 Bluetooth로 전달
void USART2_IRQHandler() {
  uint16_t word;
  if(USART_GetITStatus(USART2,USART_IT_RXNE)!=RESET){
    word = USART_ReceiveData(USART2);

    USART_SendData(USART1,word);
    USART_ClearITPendingBit(USART2,USART_IT_RXNE);
  }
}

void delay()
{
    int i = 0;
    for(i=0; i<3000000; i++){}
}

void SendData(uint16_t data) 
{
    /* USART1에 데이터 전송 */
    USART1->DR = data;

    /* 전송이 완료되면 해제 */
    while ((USART1->SR & USART_SR_TC) == 0);

    /* USART2에 데이터 전송 (블루투스 모듈) */
    USART2->DR = data;

    /* 전송이 완료되면 해제 */
    while ((USART2->SR & USART_SR_TC) == 0);
}

//SendData가 16비트데이터를 전송하므로, 전체 string을 출력하도록 수행
void SendString(const char *str) {
  while(*str) {
    if(*str != '\0'){
      SendData(*str++);
    }
  }
}

//조도센서에 들어온 값을 확인하기 위해서, 정수 값을 출력하도록 설정
void SendInt(int tmp_val){
        int charDigit[4];
         for (int i = 0; i < 4; i++) {
             charDigit[3 - i] = tmp_val % 10;
             tmp_val = tmp_val / 10;
         }
         
         // 저장된 자릿수를 올바른 순서로 출력
         SendString(" ");
         for (int i = 0; i < 4; i++) {
             SendData(charDigit[i] + 48);
         }
         SendString("\r\n");
}

/* 각각 조도센서의 아날로그 값이 처음으로 임계점 아래로 떨어질때만 Sensor 값을 1로 업데이트하고 CurrentState를 1 증가시킴
   이를 통해 이미 적중시킨 조도센서를 다시 맞춰서 점수를 얻는것을 방지함 */
void UpdateSensorStates(void) 
{
    // PA5 - Sensor1
    if (ADC_Value[0] < THRESHOLD1 && Sensor1 == 0) 
    {
        SendInt(ADC_Value[0]);
        SendInt(ADC_Value[1]);
        SendInt(ADC_Value[2]);
        SendInt(ADC_Value[3]);
        SendInt(ADC_Value[4]);
        Sensor1 = 1;
        CurrentState++;
        GPIO_ResetBits(GPIOD, GPIO_Pin_8);
        return;
    }

    // PA6 - Sensor2
    if (ADC_Value[1] < THRESHOLD2 && Sensor2 == 0) 
    {
        SendInt(ADC_Value[0]);
        SendInt(ADC_Value[1]);
        SendInt(ADC_Value[2]);
        SendInt(ADC_Value[3]);
        SendInt(ADC_Value[4]);

        Sensor2 = 1;
        CurrentState++;
        GPIO_ResetBits(GPIOD, GPIO_Pin_9);
        return;
    }

    // PA7 - Sensor3
    if (ADC_Value[2] < THRESHOLD3 && Sensor3 == 0) 
    {
        SendInt(ADC_Value[0]);
        SendInt(ADC_Value[1]);
        SendInt(ADC_Value[2]);
        SendInt(ADC_Value[3]);
        SendInt(ADC_Value[4]);
        Sensor3 = 1;
        CurrentState++;
        GPIO_ResetBits(GPIOD, GPIO_Pin_10);
        return;
    }

    // PB0 - Sensor4
    if (ADC_Value[3] < THRESHOLD4 && Sensor4 == 0) 
    {
        SendInt(ADC_Value[0]);
        SendInt(ADC_Value[1]);
        SendInt(ADC_Value[2]);
        SendInt(ADC_Value[3]);
        SendInt(ADC_Value[4]);
        Sensor4 = 1;
        CurrentState++;
        GPIO_ResetBits(GPIOD, GPIO_Pin_11);
        return;
    }

    // PB1 - Sensor5
    if (ADC_Value[4] < THRESHOLD5 && Sensor5 == 0) 
    {
        SendInt(ADC_Value[0]);
        SendInt(ADC_Value[1]);
        SendInt(ADC_Value[2]);
        SendInt(ADC_Value[3]);
        SendInt(ADC_Value[4]);
        Sensor5 = 1;
        CurrentState++;
        GPIO_ResetBits(GPIOD, GPIO_Pin_12);
        return;
    }
}

int main() {
    SystemInit();
    RCC_Configure();
    GPIO_Configure();     // GPIO 클럭 초기화
    ADC_Configure();
    DMA_Configure();
    PWM_Init_Config();   // PWM 초기화
    USART_Configure();
    NVIC_Configure();
    GPIO_SetBits(GPIOB, GPIO_Pin_4); // PB4를 Low로 설정하여 레이저 끄기 (ResetBits)
    
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
            char msg_start[] = "===========[Game Start]===========\r\n";
            for (int i = 0; i < sizeof(msg_start)-1; i++) {
                SendData(msg_start[i]);
            } 
            break;
        }
      }
      //gaming
      LEDInit();
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
              UpdateSensorStates();
              if (PreviousState != CurrentState) { // 조도센서에 레이저가 적중했다면,
                  SendString("hit!\r\n");
                  point += 20;
                  TIM_SetCompare2(TIM2, 500); // 50% 듀티 (소리 ON)
                  PreviousState = CurrentState;
              } 
              bullet--;
              delay();
          } else {
              TIM_SetCompare2(TIM2, 0);   // 듀티 0% (소리 OFF)
          }
          if(bullet == 0){
              char msg_end[] = " ===========[Finish]===========\r\n";
              for (int i = 0; i < sizeof(msg_end)-1; i++) {
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
