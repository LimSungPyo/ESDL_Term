#include "stm32f10x.h"
#include "core_cm3.h"
#include "misc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_adc.h"
#include "lcd.h"
#include "touch.h"
#include "stm32f10x_dma.h"

uint32_t flag = 0;
uint32_t light;
int color[12] = {WHITE,CYAN,BLUE,RED,MAGENTA,LGRAY,GREEN,YELLOW,BROWN,BRRED,GRAY};
uint16_t value = 0;
volatile uint32_t ADC_Value[1];
uint32_t THRESHOLD = 3800;

void RCC_Configure(void)
{
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
}

void GPIO_Configure(void)
{  
    // PA5
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void ADC_Configure(void)
{
    ADC_InitTypeDef ADC_InitStruct;

    ADC_InitStruct.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStruct.ADC_ScanConvMode = DISABLE;
    ADC_InitStruct.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStruct.ADC_NbrOfChannel = 1;

    ADC_Init(ADC1, &ADC_InitStruct);

    //chanel 5 setting
    ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 1, ADC_SampleTime_239Cycles5);
    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1)){}

    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1)){}

    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

void DMA_Configure(void)
{
    DMA_InitTypeDef DMA_Instructure;
   
    DMA_Instructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_Instructure.DMA_MemoryBaseAddr = (uint32_t)&ADC_Value[0];      
   
    DMA_Instructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_Instructure.DMA_BufferSize = 1;
    DMA_Instructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_Instructure.DMA_MemoryInc = DMA_MemoryInc_Disable;
    DMA_Instructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    DMA_Instructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    DMA_Instructure.DMA_Mode = DMA_Mode_Circular;
    DMA_Instructure.DMA_Priority = DMA_Priority_High;
   
    DMA_Instructure.DMA_M2M = DMA_M2M_Disable;
   
    DMA_Init(DMA1_Channel1, &DMA_Instructure);
   
    DMA_Cmd(DMA1_Channel1, ENABLE);
}

void delay()
{
  int i = 0;
  for(i=0; i<3000000; i++){}
}

int main() {
   
    SystemInit();
    RCC_Configure();
    GPIO_Configure();
    ADC_Configure();
    DMA_Configure();
   
    LCD_Init();
    Touch_Configuration();
    Touch_Adjust();
    LCD_Clear(WHITE);
   
    while(1){
        delay();

        flag = (ADC_Value[0] < THRESHOLD) ? 1: 0;
        if (flag) {
            LCD_Clear(GRAY);
            LCD_ShowNum(50, 150, ADC_Value[0], 10, WHITE, GRAY); 
        } else {  
            LCD_Clear(WHITE);
            LCD_ShowNum(50, 150, ADC_Value[0], 10, GRAY, WHITE); 
        }
    }
    return 0;
}
