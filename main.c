#include "LPC17xx.h"
#include "lpc_types.h"
#include "uIP/ethernet.h"
#include "uart.h"
#include "opentherm.h"
#include "timer.h"
#include "GPIO.h"
#include "mqtt.h"
#include "leds.h"

// MAC: uipopt.h
// IPs: ethernet.h

uint32_t toggle = 0;
static volatile int seccount=0;

int main(void)
{
	char incoming;
	OT_init();
	EthernetInit();
	UART0_Init(9600);

	TimerInit(0, 1000);
	TimerInit(3, 120000000);
	//delayMs(0,1000);
	//enable_timer(3);

	while(1)
	{
		EthernetHandle();

		//UARTHandle();
		if(UART0_Available())
		{
			UARTLED_ON;
			incoming = UART0_Getchar();
			gw_char(incoming);
			UARTLED_OFF;
		}
	}
}
