/******************************************************************************
* ? ? ?   : Wifi.c
* ? ? ?   : V1.0
* ????   : wifi????
******************************************************************************/

#include "Wifipro.h"
#include "Control.h"
#include "uart.h"
#include "wifi.h"

void Wifi_Init(void)
{
	UART2_Init();
	wifi_protocol_init();
//	UART2_RxEnable();
}
