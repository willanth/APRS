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

//uint8_t ax25_buf[(7 * 10) + 2 + 256 + 2];
uint8_t ax25_buf[(7 * 10) + 2 + 64 + 2];
unsigned ax25_len = 0;

uint8_t *tx_buf = 0;
unsigned tx_len = 0;

extern uint8_t gps_parse_buf[64];

void ax25_append_octet(uint8_t x);
void ax25_begin_frame(void);
void ax25_append_address(uint8_t *s, unsigned ssid, unsigned last);
void ax25_begin_info(void);
void ax25_append_info(uint8_t *s);
void ax25_end_frame(void);


void copyfield(uint8_t **f, unsigned len)
{
	uint8_t *s = *f;
	while(len && *s) ax25_append_octet(*s++), --len;
	while(len) ax25_append_octet(' '), --len;
	while(*s) ++s;
	*f = ++s;
}

void txpos(void)
{
	uint8_t *gps = gps_parse_buf;
	
	ax25_begin_frame();
	
	ax25_append_address((unsigned char *)"APZ001", 0, 0);
	ax25_append_address((unsigned char *)"KT8DOG", 15, 0);
	ax25_append_address((unsigned char *)"WIDE1", 2, 0);
	ax25_append_address((unsigned char *)"WIDE2", 2, 1);
	
	ax25_begin_info();
	
	//ax25_append_info((unsigned char *)"Test Message 123");
	
	copyfield(&gps, 0);		// Skip GPGGA
	ax25_append_octet('/');
	copyfield(&gps, 6);
	ax25_append_octet('z');
	copyfield(&gps, 7);
	copyfield(&gps, 1);
	ax25_append_octet('/');
	copyfield(&gps, 8);
	copyfield(&gps, 1);
	ax25_append_octet('-');
							// Check if valid
	copyfield(&gps, 1);
	
	ax25_end_frame();

	tx_buf = ax25_buf; tx_len = ax25_len;
}

#if 0	
void aprs_test(void)
{
	ax25_begin_frame();
	ax25_append_address((unsigned char *)"APZ001", 0, 0);
	ax25_append_address((unsigned char *)"KT8DOG", 15, 0);
	ax25_append_address((unsigned char *)"WIDE1", 2, 0);
	ax25_append_address((unsigned char *)"WIDE2", 2, 1);
	ax25_begin_info();
	ax25_append_info((unsigned char *)"Test Message 123");
	ax25_end_frame();

	for(;;) {
		tx_buf = ax25_buf; tx_len = ax25_len;
		__delay_cycles(16000000);
	}
}
#endif
