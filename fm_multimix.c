/*
    fm-multimix  A multiple channel fm downmixer.
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
 *This is the main program. A loop with a call to select() reads data 
 *from stdin, compares possible transmissions to a list of frequencies 
 *that should be recorded and spawns rtl-fm if required.
 *Mixing is also done here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include "fft.h"
#include "sinegen.h"
#include "demod_proc.h"
#include "circ_buffer.h"
#include "globals.h"

fd_set readdesc;
fd_set writedesc;
uint8_t inbuf [READ_SIZE];
uint8_t outbuf [READ_SIZE];
fft_obj* chanfinder_obj;
int fft_has_results;
double* results;
int skip=0;
int i;
int readcount;
int highdesc = 0;
int descready;
demodproc** demod_processes;
int center_freq=0;
long long int samples_read=0;
int freqs[FFT_LEN];
int freq_count=0;
int verbosity=0;
float detection_threshold=4;
int detection_misses=10;
int filter_sub=0;
struct circ_buf circ_buffer;
int fast_mode=0;
int squelch=0;

void mix_signal(demodproc* proc, uint8_t* inbuf, uint8_t* outbuf)
{
	int j;
	int32_t tmpreal;
	int32_t tmpimag;
	int32_t tmpinreal;
	int32_t tmpinimag;
	
	for(j=0;j<READ_SIZE;j+=2)
	{
		tmpreal = sine_next_val(proc->real);
		tmpimag = sine_next_val(proc->imag);
		tmpinreal = inbuf[j]-128;
		tmpinimag = inbuf[j+1]-128;
		//fprintf(stderr, "%f %f \n", tmpreal, tmpimag);
		
		outbuf[j]=(uint8_t)((((tmpinreal*tmpreal) - (tmpinimag*tmpimag))>>16)+ 128);
		outbuf[j+1]=(uint8_t)((((tmpinimag*tmpreal) + (tmpinreal*tmpimag))>>16)+ 128);
	}
}

void set_file_descriptors()
{
	FD_ZERO(&writedesc);
	FD_ZERO(&readdesc);

	if(circSpaceLeft(&circ_buffer)>=4096)
	{
		FD_SET(STDIN_FILENO, &readdesc);
		if(STDIN_FILENO > highdesc)
		{
			highdesc = STDIN_FILENO;
		}
	}
	if(circSpaceLeft(&circ_buffer)<4096)
	{
		for(i=0; i<get_process_count(); i++)
		{
			if(demod_processes[i]->serviced == 0)
			{
				FD_SET(demod_processes[i]->outpipe, &writedesc);
				if(demod_processes[i]->outpipe > highdesc)
				{
					highdesc = demod_processes[i]->outpipe;
				}
			}
		}
	}
}

int read_data()
{
	if(FD_ISSET(STDIN_FILENO, &readdesc))
	{
		//readcount = fread(inbuf, sizeof(uint8_t), READ_SIZE, stdin);  
		readcount = read(STDIN_FILENO, inbuf, READ_SIZE);
		if(readcount == 0)
		{
			fprintf(stderr,"End of file reached, exiting.\n");
			return 0;
		}
		if(readcount == -1)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{
				fprintf(stderr,"again or wouldblock stdin read\n");
			}
			else
			{
				fprintf(stderr,"error reading from stdin, exiting \n");
				return 0;
			}
		}
		for(i=0; i<get_process_count(); i++)
		{
			demod_processes[i]->serviced=0;
		}
		samples_read+=readcount;
		circWrite(&circ_buffer, inbuf, readcount);

		
		if(skip == 0)//no need to do an FFT every time
		{
			fft_has_results = do_fft(chanfinder_obj, inbuf, readcount);

			if(fft_has_results)
			{
				results= get_fft_results(chanfinder_obj);
				int i;
				for(i=0; i<FFT_LEN; i++)
				{
					if(results[i] > 0)
					{
						if(verbosity > 0)
						{
							fprintf(stderr,"[%lld] Possible transmission at %d Hz with %f\n",
									samples_read/SAMP_RATE/2,
								 	(i-(FFT_LEN/2))*SAMP_RATE/FFT_LEN+center_freq, results[i]);
						}
					}
				}
				check_processes(results, freqs, freq_count, samples_read,
						detection_misses, center_freq, filter_sub, fast_mode, squelch);
			}
			skip = 120;
		}
		else
		{
			skip--;
		}
	}
	return 1;
}

int write_to_demod()
{
	for(i=0; i<get_process_count(); i++)
	{
		if(FD_ISSET(demod_processes[i]->outpipe, &writedesc))
		{
			int writecount;
			if(demod_processes[i]->outbuf_written == 0)
			{
				circPeek(&circ_buffer,inbuf,READ_SIZE);
				mix_signal(demod_processes[i],inbuf,demod_processes[i]->outbuf);
			}
			writecount = write(demod_processes[i]->outpipe,
				 	demod_processes[i]->outbuf + demod_processes[i]->outbuf_written,
				 	READ_SIZE-demod_processes[i]->outbuf_written);
			if(writecount == 0)
			{
				close(demod_processes[i]->outpipe);
			}
			if(writecount == -1)
			{
				if(errno == EAGAIN || errno == EWOULDBLOCK)
				{
					fprintf(stderr,"again or wouldblock pipe write\n");
				}
				else
				{
					fprintf(stderr,"error writing to pipe, exiting \n");
					return 0;
				}
			}
			else
			{
				demod_processes[i]->outbuf_written += writecount;
			}
			if(demod_processes[i]->outbuf_written == READ_SIZE)
			{
				demod_processes[i]->outbuf_written=0;
				demod_processes[i]->serviced=1;
			}

		}
	}

	//check if all processes have been serviced

	int serviced=1;
	for(i=0; i<get_process_count(); i++)
	{
		if(!demod_processes[i]->serviced)
		{
			serviced = 0;
			break;
		}
	}
	if(serviced)
	{
		circReadAdvance(&circ_buffer, READ_SIZE);
	}
	return 1;
}

void arg_msg()
{
	fprintf(stderr, "\n\tfm_multimix -f center frequency [-options] frequency1 frequency2 ...\n"
									"\t-f Center frequency of the signal on stdin\n"	
									"\t[-v Increases level of debug output/verbosity]\n"	
									"\t[-t The threshold at which the system will start recording.\n"
									"\tMeasured in channel level/average signal level (default 4)]\n"	
									"\t[-m The amount of times a channel has to be under the threshold\n"
									"\tbefore recording will stop (default 10, approx 10 seconds)]\n"
									"\t[-s Filter out sub audible (<300 Hz) tones.]\n"	
									"\t[-S Speed boost. Turns of nice features like output filtering\n"
									"\tin an attempt to improve speed. (defualt off)]\n"	
			);
	exit(1);
}

int main(int argc, char *argv[])
{
	struct timeval timeout;
	int running =1;
	int opt;

	while ((opt = getopt(argc, argv, "f:t:m:vsSl:")) != -1)
	{
		switch (opt) 
		{
			case 'f':
				center_freq = (int)atof(optarg);
				break;
			case 't':
				detection_threshold = (int)atof(optarg);
				break;
			case 'm':
				detection_misses = (int)atof(optarg);
				break;
			case 'v':
				verbosity++;
				break;
			case 's':
				filter_sub=1;
				break;
			case 'S':
				fast_mode=1;
				break;
			case 'l':
				squelch = (int)atoi(optarg);
				break;
			default:
				arg_msg();
				break;
		}
	}
	while(optind < argc)
	{
			freqs[freq_count]=(int)atoi(argv[optind]);
			freq_count++;
			optind++;
	}
	if(freq_count == 0)
	{
		arg_msg();
	}

	int min,max;
	int i;
	min=freqs[0];
	max=freqs[0];
	for(i=1; i<freq_count; i++)
	{
		if(min>freqs[i])
		{
			min = freqs[i];
		}	
		if(max<freqs[i])
		{
			max=freqs[i];
		}
	}
	if(max-min > 1000000)//slightly less than actual sampled bandwidth to avoid issues with sampling at the center frequency.
	{
		fprintf(stderr, "Difference between maximum and minimum freqencies exceed sampled spectrum\n");
		exit(1);
	}
	/*if(!center_freq)
	{
		center_freq= (min+max)/2;

		//do not use frequencies around center_freq.
		//combination of dc from ADC and artifacts from splitting spectrum.
		for(i=0;i<freq_count;i++)
		{
			if(freqs[i]< center_freq+BANDWIDTH && freqs[i]> center_freq-BANDWIDTH)
			{
				center_freq+=BANDWIDTH;
				break;
			}
		}
	}*/
	//	for now this has to be handled externally
	if(!center_freq)
	{
		arg_msg();
	}
		

	for(i=0;i<freq_count;i++)
	{
		freqs[i]=((freqs[i]-center_freq) * FFT_LEN / SAMP_RATE)+(FFT_LEN/2);
		if(freqs[i]>FFT_LEN || freqs[i]<0)
		{
			fprintf(stderr, "Frequencies too far away from center frequency.\n");
			arg_msg();
		}
	}

	demod_processes	= get_process_list();
	chanfinder_obj = fft_init(detection_threshold);
	circInit(&circ_buffer, SAMP_RATE*4);//should give about two seconds of buffering

	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK ); 
	fcntl(STDOUT_FILENO, F_SETFL, fcntl(STDOUT_FILENO, F_GETFL) | O_NONBLOCK);
	while (running)
	{
		set_file_descriptors();

		timeout.tv_sec=1;
		timeout.tv_usec=0;

		descready = select( highdesc+1, &readdesc, &writedesc, NULL, &timeout);

		if(descready != -1)
		{
			if(!read_data())
			{
				running =0;
				continue;
			}

			if(!write_to_demod())
			{
				running = 0;
				continue;
			}
		}
	}
	return 0;
}
