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

uint8_t gps_parse_buf[];

unsigned parse_gps(uint8_t c)
{
	static uint16_t gps_word_index = 0;
	static uint16_t gps_word_mask = 0;
	static uint8_t *gps_char;

	if(c == '$') {						// - Begining of sentence
		gps_word_index = 1;				// Setup word index
		gps_word_mask = 1;				// Parse first word
		gps_char = gps_parse_buf;		// Setup parse buffer
	} else if(c == ',') {				// - Word seperator
		if(gps_word_index & gps_word_mask)
			*gps_char++ = 0;			// Null terminate string
		if(gps_word_index == 1) {
			if(gps_parse_buf[3] == 'G') { /// hack
				gps_word_mask = 0x003E;
			} else {
				gps_word_mask = 0;
			}
		}
		gps_word_index <<= 1;			// Increment word index
	} else if(c >= ' ' && c <= '~') {	// - Printable char
		if(gps_word_index & gps_word_mask) { // Check if this word should be parsed
			*gps_char++ = c;			// Save char in parse buffer
		}								//
	} else if(c == 13 || c == 10) {		// - End of sentence
		if(gps_word_index && gps_word_mask) {
			*gps_char = 0;				// Null terminate string
			return 1;					//
		}
		gps_word_index = 0;				// Clear word index, disable parsing
	} else {							// - Ignore all invalid chars
	}									//
	return 0;
}
			
/*
	--- GPGGA
	 0 GPGGA
	 1 Time HHMMSS
	 2 Latitude DDMM.MMMM
	 3 Latitude N/S
	 4 Longitude DDDMM.MMMM
	 5 Longitude E/W
	 6 Fix  0 = Invalid  1 = GPS  2 = DGPS
	 7 Number of sats in view NN
	 8 Horizonal dilution of precision N.N
	 9 Altitude NNN.N
	10 Altitude units M/F
	11 Height of geoid NN.N
	12 Height of geoid units M/F
	13 Time since last DGPS update
	14 DGPS reference station ID
	15 Checksum
*/
