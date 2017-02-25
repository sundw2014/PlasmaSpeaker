#include "RawHID.h"
#include "usb_istr.h"
#include "usb_lib.h"
#include "usb_pwr.h"

extern uint8_t newData;
extern uint8_t Receive_Buffer[PACK_SIZE];

void RawHID_send(uint8_t *buffer, uint32_t length)
{
    /* Write the descriptor through the endpoint */
    USB_SIL_Write(EP1_IN, buffer, length);

    SetEPTxValid(ENDP1);
}

uint32_t RawHID_recv(uint8_t *buffer, uint32_t length)
{
    if(newData){
        memcpy(buffer, Receive_Buffer, length);
        return length;
    }
    else
    {
        return 0;
    }
}
