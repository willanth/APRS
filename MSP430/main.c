/*
    Copyright (C) 2012  Kevin Timmerman

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <msp430.h>
#include <stdint.h>

/*
  1	Vcc		 -
  2	P1.0	Out	--	IO  LED1
  3	P1.1	In	--	IO	Rxd
  4	P1.2	Out	--	IO	Txd
  5	P1.3	In	PU	IO	Switch
  6	P1.4	Out	--	Alt	SMCLK out
  7	P1.5	Out	--	IO	Not Used
  8 P2.0
  9 P2.1
 10 P2.2
 11 P2.3
 12 P2.4
 13 P2.5
 14	P1.6	Out	--	Alt	PWM Audio Out
 15	P1.7	Out	--	IO	Not Used
 16	RST		---	--	--	SBW
 17	TEST	---	--	--	SBW
 18	XOUT	 -			32768 Hz xtal
 19	XIN		 -			32768 Hz xtal
 20	Vss		 -
*/

uint8_t gps_buf[32];
uint8_t const * gps_end = gps_buf + sizeof(gps_buf);
uint8_t *gps_head = gps_buf;
uint8_t *gps_tail = gps_buf;

extern unsigned sec_timer;

static const unsigned long smclk_freq = 16000000;					// SMCLK frequency in hertz
static const unsigned bps = 4800;									// Async serial bit rate

void main(void)
{
	const unsigned long brd = (smclk_freq + (bps >> 1)) / bps;	// Bit rate divisor
	
	unsigned send_pos = 0;
	
	WDTCTL = WDTPW | WDTHOLD; 					// Disable watchdog
	
	DCOCTL = 0;									// Run DCO at 16 MHz
	BCSCTL1 = CALBC1_16MHZ;						//
	DCOCTL  = CALDCO_16MHZ;						//
	 
	P1DIR = 0xF5;								// I/O assignment
	P1REN = 0x08;								// 
	P1OUT = 0x04;								//
	P1SEL = 0x52;								// Enable Timer A output, SMCLK output, UART rx
	P2SEL |= 0x02;								// UART rx

	UCA0CTL1 = UCSWRST;							// Hold USCI in reset to allow configuration
	UCA0CTL0 = 0;								// No parity, LSB first, 8 bits, one stop bit, UART (async)
	UCA0BR1 = (brd >> 12) & 0xFF;				// High byte of whole divisor
	UCA0BR0 = (brd >> 4) & 0xFF;				// Low byte of whole divisor
	UCA0MCTL = ((brd << 4) & 0xF0) | UCOS16;	// Fractional divisor, oversampling mode				
	UCA0CTL1 = UCSSEL_2;						// Use SMCLK for bit rate generator, release reset												

	TACCR0 = 399;								// Setup Timer A period for under 32768 kHz
	TACCR1  = TACCR0 / 2;						// Setup Timer A compare to midpoint
	TACCTL1 = OUTMOD_7;							// Setup Timer A reset/set output mode
	TAR = 0xFF00;								// Force interrupt soon
	TACTL = TASSEL_1 | MC_1 | TAIE; 			// Timer A config: ACLK, count up, overflow int enabled
	TACCTL0 |= CCIE;							// Enable period interrupt
	
	_EINT();									// Enable interrupts
	
	
	for(;;) {
		if(gps_head != gps_tail) {
			uint8_t c = *gps_tail++;
			if(gps_tail >= gps_end) gps_tail = gps_buf;
			if(parse_gps(c)) {
				if(send_pos) {
					txpos();
					send_pos = 0;
				}
			}
		}
		if(!sec_timer) {
			if(!send_pos) {
				send_pos = 1;
				sec_timer = 10;
			}
		}			
	}
}

