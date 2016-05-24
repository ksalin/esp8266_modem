Virtual modem for ESP8266
=========================

Copyright (C) 2015 Jussi Salin <salinjus@gmail.com> under GPLv3 license.

Overview
--------

ESP8266 is a tiny MCU module with WIFI. It already contains a virtual modem firmware by factory but I wanted to make one myself to support a wider range of baud rates. For example Commodore 64 requires 2400 or lower. Now it is also possible to add additional features in the future because this is open source. For example, translation tables for different character sets or file transfer protocol conversions on the fly with help of a buffer in MCU memory.

AT command examples
-------------------

* Change baud rate: AT115200
* Connect to WIFI: ATWIFIMyAccessPoint,MyPassword1234
* Connect by TCP: ATDTsome.bbs.com:23
* Disable telnet command handling: ATNET0
* Get my IP: ATIP
* Make a HTTP GET request: ATGEThttp://host:80/path
* Answer a RING: ATA
* Disconnect: +++ (following a delay of a second)

Note that the key and port are optional parameters. Port defaults to 23. All parameters are case sensitive, the command itself not. You must always connect to an access point before dialing, otherwise you get an error. When you connect to WIFI you get either OK or ERROR after a while, depending on if it succeeded. If you get ERROR the connection might still occur by itself some time later, in case you had a slow AP or slow DHCP server in the network. When dialing, you get either CONNECT when successfully connected or ERROR if the connection couldn't be made. Reasons can be that the remote service is down or the host name is mistyped.

Default Baud rate is defined in the code. 2400 is safe for C64 and 19200 for any PC and Amiga. 115200 for PC's with "new" 16550 UART.  You must always have that default rate on the terminal when powering on. After giving a command for a higher rate nothing is replied, just switch to the new baud rate in your terminal as well. Then you can give next command in the new baud rate. Note that the first command after switching baud rate might fail because the serial port hardware is not fully synchronized yet, so it might be good idea to simply give "AT" command and wait for "ERROR" or "OK" before giving an actual command.

Example communication
---------------------

	OK
	ATWIFIMyAccessPoint,MyPassword
	Connecting to MyAccessPoint/MyPassword
	OK
	ATDTbat.org
	Connecting to bat.org:23
	CONNECT


	    __|\/|__             __|\/|__           __|\/|__       __|\/|__  Logo
	     \<..>/               \<..>/             \<..>/         \<..>/    by:
	             |\_/|                                      |\_/|          Gar
	       ______|0 0|______   /////// //\\ \\\\\\\   ______|0 0|______
	        \| | |   |  | |/  //   // //  \\    \\     \| | |   |  | |/
	          \|..\./...|/   /////// ////\\\\    \\      \|..\./...|/

A more detailed example can be seen on my YouTube video at: https://youtu.be/oiP5Clx3D_s

Hints
-----

The module can also be used for other than telnet connections, for example you can connect to HTTP port, send a HTTP request and receive a response.

I made a 3D printable case for the C64: http://www.thingiverse.com/thing:1545605 If you build it for any other device you can use any generic case because you need room for a RS232 level converter such as MAX232, and you do not need holes for any rare connectors.

Future plans and status
-----------------------

* It seems quite complete for me but please send ideas.
* I tested huge transfers with ZModem, seems to be working. Remember to use -e parameter with sz and rz if using over a telnetd.
* Serial multiplayer game of Doom (sersetup.exe) seems to be also working. Remember to use ATNET0 when playing serial games, also on DosBox emulator if it's the other host.
