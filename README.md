fm-multimix
===========

A multiple channel FM downmixer.

This program takes a 8 bit unsigned IQ pair stream as input and downmixes selected narrow band FM channels to DC. If sufficient power is transmitted on a channel it spawns a rtl-fm process to demodulate this channel and saves it to a file. This program is intended for use with rtl-sdr but not limited to it.


Example use
-----------
	rtl_sdr -f xxx500000 -s 1014300 - | ./fm_multimix -f xxx500000 xxx600000 xxx700000

This will record anything at xxx,6MHz and xxx,7MHz. Any amount of channels can be specified as long as they are within 1014300Hz centered at xxx,5MHz and the computer you are using it on has sufficient processing power. The Raspberry PI for example can just about decode 2 channels in real time.


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
