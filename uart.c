/*
 * File:   uart.c
 * Author: David
 *
 * Created on November 7, 2024, 12:26 PM
 */


#include <xc.h>
#include "uart.h"

void init_uart(void)
{
	/* Serial initialization */
	RX_PIN = 1;
	TX_PIN = 0;

	/* TXSTA:- Transmitor Status and control Register */
	/* 9bit TX enable or disable bit */ 
	TX9 = 0;        //if tx9 is zero it is set to 8 bit transmission    []]
	/* UART Tarsmition enable bit */
	TXEN = 1;
	/* Synchronous or Asynchronous mode selection */
	/* Asynchronous */
	SYNC = 0;
	/* Send the Break character bit */
	SENDB = 0;
	/* Low or High baud rate selection bit */
	/* High Baud Rate */
	BRGH = 1;

	/* RCSTA :- Recepition Status and control Register */
	/* TX/RC7 and RX/RC6 act as serial port */ 
	SPEN = 1;
	/* 9bit RX enable or disable bit */
	RX9 = 0;
	/* Continous reception enable or disable */ 
	CREN = 1;

	/* BAUDCTL:- Baud rate control register */
    
    /*ABDOVF = 0;         //auto boaud rate over flow flag
    WUE = 0;
    ABDEN = 0;*/
    
	/* 16bit baud generate bit */ 
	BRG16 = 0;
	
	/* Baud Rate Setting Register */
	/* Set to 10 for 115200, 64 for 19200 and 129 for 9600 */
	SPBRG = 129;


	/* TX interrupt flag bit */
	TXIF = 0;

	/* RX interrupt enable bit */
	RCIF = 0;
}

void putch(unsigned char byte)          //This function handles transmitting (TX).
{
	/* Output one byte */
	/* Set when register is empty */
	while(!TXIF)
	{
		continue;
	}
	TXIF = 0;
	TXREG = byte;
} 

int puts(const char *s)
{
	while(*s)		
	{
		putch(*s++);	
	}
	return 0;
}

unsigned char getch(void)           //This function handles receiving (RX).
{
	/* Retrieve one byte */
	/* Set when register is not empty */
	while(!RCIF)
	{
		continue;
	}
	RCIF = 0;
	return RCREG;
}

unsigned char getche(void)          //This function does both receiving and transmitting.
{
	unsigned char c;

	putch(c = getch());

	return (c);
}