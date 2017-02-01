fm-multimix
===========

A multiple channel FM downmixer.

This program takes a 8 bit unsigned IQ pair stream as input and downmixes selected narrow band FM channels to DC. If sufficient power is transmitted on a channel it spawns a rtl-fm process to demodulate this channel and saves it to a file. This program is intended for use with rtl-sdr but not limited to it.


Example use
-----------
	rtl_sdr -f xxx500000 -s 1014300 - | ./fm_multimix -l 150 -f xxx500000 xxx600000 xxx700000

This will record anything at xxx,6MHz and xxx,7MHz. Any amount of channels can be specified as long as they are within 1014300Hz centered at xxx,5MHz and the computer you are using it on has sufficient processing power. 

To use this on very slow systems it helps to increase the priority of rtl_sdr and give it a large buffer to work with:

	sudo nice -n-10 rtl_sdr -f xxx500000 -s 1014300 - | pv -q -B 50m | ./fm_multimix -l 150 -f xxx500000 xxx600000 xxx700000

This approach requires ```pv``` to also be installed. 
With the priority increase and buffering the Raspberry PI for example can demodulate 2 to 3 channels at the same time as long as they do not transmit for many minutes at a time.

### Custom Decoder

Instead of saving each channel to a file, a custom decoder can be specified. The command receives two arguments: The frequency and the time elapsed since the start. The following example shows a simple wrapper script to decode AFSK1200 on the fly:

	$ cat decode.sh
	#!/bin/sh
	rtl_fm -f $1 -s 22050 -P -C -i 46 -t 5 - | multimon-ng -t raw -c -a AFSK1200 -A >$1-$2.log
	$ rtl_sdr -f ... | fm_multimix -f ... -c ./decode.sh


Warning
-------
Please check with local regulations before recording arbitrary frequencies. Many if not most will probably require a license to receive.


Dependencies
----------
FFTW3 development libraries:
libfftw3-dev on Ubuntu and Raspbian:

	sudo apt-get install libfftw3-dev

cmake:

	sudo apt-get install cmake

rtl-sdr: This program requires a modified version of rtl-fm that can be found at https://github.com/TonberryKing/rtlsdr

Warning: This will install over all your rtl-sdr programs and not just rtl-fm


Building
----------
	mkdir build
	cd build
	cmake ../
	make


Todo
----------
-Compress saved files.

-Add options for output files.

-Add ability to install.

-Better rtl-fm integration.
