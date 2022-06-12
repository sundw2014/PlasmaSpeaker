#include "timer.h"
#include "misc.h"
void TIM1_Config()
{
	NVIC_InitTypeDef NVIC_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);          
  TIM_TimeBaseStructure.TIM_Prescaler = (7200-1);                 //1 Mhz                        
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;    
  TIM_TimeBaseStructure.TIM_Period = 1000;                                     
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;                                       
  TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
  TIM_TimeBaseInit(TIM1,&TIM_TimeBaseStructure);
	
	NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn; 
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; 
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0; 
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; 
  NVIC_Init(&NVIC_InitStructure); 
	
	
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM1, ENABLE);	
}
