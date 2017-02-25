#ifndef _USB_COMMON_H__
#define _USB_COMMON_H__
#include "usb_type.h"

#define USB_Port GPIOA
#define USB_Det  GPIO_Pin_15
#define USB_Conn GPIO_Pin_8

#define USB_DISCONNECT                      USB_Port
#define USB_DISCONNECT_PIN                  USB_Conn
#define RCC_APB2Periph_GPIO_DISCONNECT      RCC_APB2Periph_GPIOA

void Set_USBClock(void);
void Enter_LowPowerMode(void);
void Leave_LowPowerMode(void);
void USB_Interrupts_Config(void);
void USB_Cable_Config (FunctionalState NewState);
void Get_SerialNum(void);
void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len);

void USBCommon_Init(void);
#endif
