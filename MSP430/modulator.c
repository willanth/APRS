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

extern uint8_t *tx_buf;
extern unsigned tx_len;

extern uint8_t gps_buf[];
extern uint8_t const * gps_end;
extern uint8_t *gps_head;

// Quarter sine wave - 0 to 191
static const uint8_t sine[256] = {
	1,		2,		3,		4,		5,		6,		8,		9,		
	10,		11,		12,		13,		15,		16,		17,		18,		
	19,		20,		22,		23,		24,		25,		26,		27,		
	29,		30,		31,		32,		33,		34,		36,		37,		
	38,		39,		40,		41,		42,		44,		45,		46,		
	47,		48,		49,		50,		52,		53,		54,		55,		
	56,		57,		58,		59,		60,		62,		63,		64,		
	65,		66,		67,		68,		69,		70,		71,		73,		
	74,		75,		76,		77,		78,		79,		80,		81,		
	82,		83,		84,		85,		86,		87,		88,		90,		
	91,		92,		93,		94,		95,		96,		97,		98,		
	99,		100,	101,	102,	103,	104,	105,	106,	
	107,	108,	109,	109,	110,	111,	112,	113,	
	114,	115,	116,	117,	118,	119,	120,	121,	
	122,	123,	123,	124,	125,	126,	127,	128,	
	129,	130,	130,	131,	132,	133,	134,	135,	
	135,	136,	137,	138,	139,	140,	140,	141,	
	142,	143,	143,	144,	145,	146,	147,	147,	
	148,	149,	149,	150,	151,	152,	152,	153,	
	154,	154,	155,	156,	156,	157,	158,	158,	
	159,	160,	160,	161,	162,	162,	163,	164,	
	164,	165,	165,	166,	166,	167,	168,	168,	
	169,	169,	170,	170,	171,	171,	172,	172,	
	173,	173,	174,	174,	175,	175,	176,	176,	
	177,	177,	178,	178,	178,	179,	179,	180,	
	180,	180,	181,	181,	182,	182,	182,	183,	
	183,	183,	184,	184,	184,	185,	185,	185,	
	185,	186,	186,	186,	186,	187,	187,	187,	
	187,	188,	188,	188,	188,	188,	189,	189,	
	189,	189,	189,	189,	190,	190,	190,	190,	
	190,	190,	190,	190,	191,	191,	191,	191,	
	191,	191,	191,	191,	191,	191,	191,	191,	
};

static unsigned pi = 0;
static unsigned pd = 0;
static unsigned bpi = 0;

unsigned ms_timer = 0;
unsigned sec_timer = 0;

void configure_modulator(unsigned n)
{
										// Warning: Always change phase delta (pd) before phase increment (pi)
										//  because an interrupt could occur between these operations
	switch(n) {
		case 0:							// 1200/2200 Hz 1200 bps VHF
			pd = (2200 *2) ^ (1200 * 2);//
			pi = 1200 * 2;				//
			bpi = 1200 * 2;				//
			break;						//
		case 1:							// 1600/1800 Hz 300 bps HF
			pd = (1800 *2) ^ (1600 * 2);//
			pi = 1600 *2;				//
			bpi = 300 * 2;				// 
			break;						//
		case 2:							// 1200 Hz test
			pd = 0;						//
			pi = 1200 * 2;				//
			break;						//	 
		case 3:							// 2200 Hz test
			pd = 0;						//
			pi = 2200 * 2;				//
			break;						//	 
		case 4:							// 1600 Hz test
			pd = 0;						//
			pi = 1600 * 2;				//
			break;						//	 
		case 5:							// 1800 Hz test
			pd = 0;						//
			pi = 1800 * 2;				//
			break;						//	 
	}
}
	
#pragma vector = TIMER0_A0_VECTOR		// Timer A CCR0 interrupt
__interrupt void timer_a0_isr(void)		// This interrupt will occur when the PWM period is near complete
										//   when TAR == CCR0 - 1
{										//
	TACTL = TASSEL_1 | MC_2 | TAIE; 	// Timer A config: ACLK, continuous, overflow interrupt
	TAR = 0xFFFF;						// Interrupt at next clock edge
}										//

#pragma vector = TIMER0_A1_VECTOR		// Timer A CCR1 & overflow interrupt
__interrupt void timer_a1_isr(void)		// The overflow interrupt will occur when the 32 kHz xtal clock
										//   (ACLK) causes Timer A to overflow from 65535 to 0
										// The CCR1 interupt (not used) will occur when the PWM on
										//   duration is complete
{										//
										// - Tone generator
	static unsigned sample = 208;		// PWM sample
	static unsigned pa = 0;				// Phase accumulator
										//
										// - NRZI encoder
	static unsigned bitphase = 0;		// tx bit timer										
	static uint8_t tx_data = 0x7E;		// AX.25 data to send
	static uint8_t bitmask = 1;			// tx bitmask
	static unsigned bsl = 0;			// bitsuff length
	static unsigned bitstuff = 0;		// bitsuff bit count
										//
										// - Millisecond and second timers
	static unsigned mspa = 0;			// ms phase accumulator
	static unsigned secpa = 0;			// sec phase accumulator										
										//	
										// - Tone generator
	TACCR1 = sample;					// Output previous PWM sample
										//
	TACTL = TASSEL_2 | MC_1;		 	// Timer A config: SMCLK, count up
										//
	//volatile unsigned z = TAIV;		// Clear interrupt flag
	//if(z == 10) {						// Make sure this is the overflow interrupt
										//
	const unsigned n = pa >> 6;			// Get sample from sine table
	sample = (n & 0x0200) ? 208 - ((n & 0x0100) ? sine[n ^ 0x03FF] : sine[n ^ 0x0200])
						  : 208 + ((n & 0x0100) ? sine[n ^ 0x01FF] : sine[n]);
										//
	pa += pi;							// Update phase accumulator
										//
										// - NRZI encoder											
	if(bitphase < bpi) {				// Time for next bit?										
		if(tx_data & bitmask) {			// Sending a one
			if(--bitstuff) {			// Decrement bitstuff count, check if not zero yet
				bitmask <<= 1;			// Next bit
			} else {					// Time to stuff a bit
				tx_data &= ~bitmask;	// Send a zero next time, bitstuff count will be reset
			}							//  when the zero is sent
		} else {						// Sending a zero
			pi ^= pd;					// Change frequency
			bitstuff = bsl;				// Reset bitsuff count
			bitmask <<= 1;				// Next bit
		}								//
										//
		if(!bitmask) {					// Time for next byte?
			bitmask = 1;				// Reset bitmask
			if(tx_len) {				// Check if more data in buffer
				--tx_len;				// Decrement tx length
				tx_data = *tx_buf++;	// Get octet from tx buffer
				bsl = 5;				// Do bit stuffing at 5 bits
			} else {					// No data in buffer - send sync
				tx_data = 0x7E;			// Sync octet
				bsl = 0;				// No bit stuffing
			}							//
		}								// endif next byte
	} else {							// endif next bit
		if(IFG2 & UCA0RXIFG) {			//
			*gps_head++ = UCA0RXBUF;	//
			if(gps_head >= gps_end)		//
				gps_head = gps_buf;		//
			IFG2 &= ~UCA0RXIFG;			//
			P1OUT ^= BIT0;				//
		}								//
	}									//
	bitphase += bpi;					// Update bit timer
										//
	mspa += 2000;						//
	if(mspa < 2000 && ms_timer) --ms_timer; //
	secpa += 2;							//
	if(secpa < 2 && sec_timer) --sec_timer; //	
}										//
