/*
    This file is part of fm-multimix, A multiple channel fm downmixer.
    Copyright (C) 2013  Hendrik van Wyk

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

/*
 * This is the header file for a process spawner that creates rtl-fm processes. 
 */
#ifndef DEMOD_PROC_H
#define DEMOD_PROC_H

#include <sys/types.h>
#include <unistd.h>
#include "sinegen.h"
#include "globals.h"

typedef struct demodproc_struct
{
	int inpipe;
	int outpipe;
	int detection_misses;
	int bin;
	int serviced; //1 if it has received the latest data
	uint8_t outbuf[READ_SIZE];
	int outbuf_written;
	sine_gen* real;
	sine_gen* imag;
	int pid;

}
demodproc;

int create_process(int bin, long long int totalread, int filter_sub, int center_freq, int fast, int squelch);
void end_process(demodproc* proc);

demodproc** get_process_list();
int get_process_count();

//checks if process exists and creates if not, 
//should handle small frequency instabilities
void check_processes(double* bins, int* freqs, int freqcount, 
		long long int total_read, int misses, int center_freq, int filter_sub, int fast, int squelch);

#endif
