#include "myGPIO.h"
void GPIO_Config()
{
			GPIO_InitTypeDef GPIO_InitStructure;
	    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);         
      GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5|GPIO_Pin_6; 
      GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;  
      GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  
      GPIO_Init(GPIOB, &GPIO_InitStructure);
}
