/**
 *  Copyright (C) 2014, David Everlöf
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef DAWGMINIFY_H
#define DAWGMINIFY_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define BITS_IN_BYTE		(8)
#define BITS_PER_CHAR		(8)

/* FOR COMPRESSED DAWG */
#define WORD_MASK 			(0x00000001)
#define CHAR_MASK 			(0x0000FF00)
#define END_MASK  			(0x00000002)

#define WORD_MASK_LENGTH 	(0x00000001)
#define END_MASK_LENGTH 	(0x00000001)
#define CHAR_MASK_LENGTH 	(0x00000008)

/* FOR BLITZKREIG */
#define BYTES_PER_NODE		(4)
#define KChildBitShift		(10)
#define KChildIndexMask		(0xFFFFFF00)
#define KLetterMask			(0x000000FF)
#define KEndOfWordFlag		(0x00000200)
#define KEndOfListFlag		(0x00000100)

/* FOR NON-COMPRESSED DAWG */
/*
#define BYTES_PER_NODE		(4)
#define KChildBitShift		(8)
#define KChildIndexMask		(0x0FFFFF00)
#define KLetterMask			(0x000000FF)
#define KEndOfWordFlag		(0x20000000)
#define KEndOfListFlag		(0x10000000)
*/

char* encode(char* in, size_t in_size, size_t* out_size);
char* decode(char* in, size_t in_size, size_t* out_size);

char* read_file(const char* filename, long* size);
void trim(char *line);

#endif