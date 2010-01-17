//
//	canping168: main.c
//
//	Listens for incoming CAN packets and replies to them.
//
//	Uses the MCP2515 SPI-based CAN controller chip to facilitate CAN
//	communications with the outside world.
//
//	Upon reception of a new CAN message, this code will reply back to a fixed
//	address (SID 0x07). It will increment the 1st data byte received and use it
//	as the reply data.
//
//	Bit timings are as follows:
//
//	0.625 	us / TQ
//	16 		TQ / bit
//	7		TQ / PSEG
//	4		TQ / PHSEG1
//	4		TQ / PHSEG2
//	1		TQ / SJW
//
//	Targets the ATMEGA168 running at 16 MHz.
//
//	Michael Jean <michael.jean@shaw.ca>
//

#include <stdio.h>
#include <stdlib.h>

#include <avr/interrupt.h>
#include <avr/io.h>

#include <util/delay.h>

#include "spi.h"

static int usart_putchar (char c, FILE *stream);
static FILE mystdout = FDEV_SETUP_STREAM (usart_putchar, NULL, _FDEV_SETUP_WRITE);

//
//	The MCP2515 triggers this interrupt whenever a new CAN message arrives.
//

ISR (INT0_vect)
{
	uint8_t data;

	printf ("INT0: Entering ISR.\n");

	spi_slave_select (0);		/* pull first data byte from received packet */
	spi_putch (0x92);
	data = spi_getch ();
	spi_slave_deselect (0);

	printf ("INT0: Received data value 0x%02X.\n", data);

	spi_slave_select (0);		/* load transmit buffer */
	spi_putch (0x02);
	spi_putch (0x31);
	spi_putch (0x00);
	spi_putch (0xE0);			/* SID address 0x07 */
	spi_putch (0x00);
	spi_putch (0x00);
	spi_putch (0x01);
	spi_putch (++data);			/* payload is (received data) + 1 */
	spi_slave_deselect (0);

	spi_slave_select (0);		/* flag as ready to send */
	spi_putch (0x81);
	spi_slave_deselect (0);

	printf ("INT0: Replied with 0x%02X.\n", data);
	printf ("INT0: Leaving ISR.\n");
}

//
//	Write a character to the USART. Used by printf and friends.
//

static int
usart_putchar
(
		char 	c,
		FILE 	*stream
)
{
	if (c == '\n')
		usart_putchar ('\r', stream);

	loop_until_bit_is_set (UCSR0A, UDRE0);
	UDR0 = c;

	return 0;
}

//
//	Program execution starts here.
//

int
main (void)
{
	spi_slave_desc_t mcp2515_desc;

	// Configure the USART for debug output

	UBRR0 = 16; 							/* 57600 baud at 16 MHz clock */
	UCSR0B = _BV (RXEN0) | _BV (TXEN0);		/* Enable receiver and transmitter */
	stdout = &mystdout;						/* tie printf to the usart */

	printf ("Main: Hello, world!\n");

	// Configure the MCU subsystems i.e. io, interrupts, and spi.

	printf ("Main: Configuring MCU... ");

	DDRB |= _BV (PB2);						/* slave select pin is output */
	EIMSK |= _BV (INT0);					/* listen for the mcp2515 interrupt */

	mcp2515_desc.port 			= &PORTB;
	mcp2515_desc.pin			= PB2;
	mcp2515_desc.select_delay	= 500.0;
	mcp2515_desc.deselect_delay = 500.0;

	spi_init ();
	spi_setup_slave (0, &mcp2515_desc);

	printf ("done.\n");

	// Configure the MCP2515 to accept new packets from any source and interrupt.

	printf ("Main: Configuring MCP2515... ");

	spi_slave_select (0);		/* reset */
	spi_putch (0xC0);
	spi_slave_deselect (0);

	spi_slave_select (0);		/* set bit timing */
	spi_putch (0x02);
	spi_putch (0x28);
	spi_putch (0x03);
	spi_putch (0x9E);
	spi_putch (0x08);
	spi_slave_deselect (0);

	spi_slave_select (0);		/* set interrupts */
	spi_putch (0x02);
	spi_putch (0x2B);
	spi_putch (0x01);
	spi_slave_deselect (0);

	spi_slave_select (0);		/* accept data from any sid */
	spi_putch (0x02);
	spi_putch (0x60);
	spi_putch (0x60);
	spi_slave_deselect (0);

	spi_slave_select (0);		/* enter normal operating mode */
	spi_putch (0x02);
	spi_putch (0x0F);
	spi_putch (0x00);
	spi_slave_deselect (0);

	printf ("done.\n");

	// Wait for packets to arrive.

	sei ();

	printf ("Main: Entering main wait loop.\n");

	for (;;)
		;

	return 0;
}
