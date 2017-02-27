#include "RawHID.h"
#include "usb_istr.h"
#include "usb_lib.h"
#include "usb_pwr.h"
#include "common.h"

extern uint8_t newData;
extern uint8_t Receive_Buffer[PACK_SIZE];

void dump_hex(uint8_t *buffer, int len);

void RawHID_send(uint8_t *buffer, uint32_t length)
{
    /* Write the descriptor through the endpoint */
    DBG_MSG("length = %d",length);
    dump_hex(buffer,length);
    for (int i=0;i<length;i+=2)
    {
        USB_SIL_Write(EP1_IN, buffer+i, 2);
        SetEPTxValid(ENDP1);
        delayMicroseconds(5000);
    }
}

uint32_t RawHID_recv(uint8_t *buffer, uint32_t length)
{
    if(newData){
        memcpy(buffer, Receive_Buffer, length);
        newData = 0;
        SetEPRxStatus(ENDP1, EP_RX_VALID);
        return length;
    }
    else
    {
        return 0;
    }
}
