# Ecowitt Firmware Updater
_Jonathan Broome_<br>
_jbroome@wao.com_<br>
_June 2024_

This is my standalone utility to update the firmware in Ecowitt gateway and
console devices.  It should work with any of the devices which can be updated
onlne (i.e. using the WSView app). It does NOT work for devices which require
that updates be installed using the microSD memory card.

I have tested it myself with the GW1000, GW1100, GW1200, and GW2000 gateways.
I have upgraded and downgraded eight different devices hundreds of times
quite successfully with this utility.  If you try it on something else,
please let me know the outcome, and I will update this list.


## How to build it:

0. Just Build It. This should be as simple as typing "make", assuming your
   system has the standard C compiler tools installed.  It does not require
   any special libraries, and should compile on any modern Unix or Linux
   system, including the Raspberry Pi.




## How to use it:

1. First, verify that the updater can connect to your device and query its
   current mac address model number, and firmware version. This will NOT
   make any changes to your device.  You may specify either a hostname
   OR an IP address after the "-h" option:

```
	$ ./ecowitt-firmware-updater -h gw1000-433
	MAC Address [24:62:ab:16:fd:0d]
	Firmware Version [GW1000_V1.6.9]

	$ ./ecowitt-firmware-updater -h 192.168.21.83
	MAC Address [30:83:98:a7:e2:9d]
	Firmware Version [GW1100C_V2.1.8]

	$ ./ecowitt-firmware-updater -h gw1200-915
	MAC Address [dc:da:0c:f4:2f:49]
	Firmware Version [GW1200B_V1.2.2]
```


> [!IMPORTANT]
> When you run the update command below, you may see a significant pause after the updater
> prints the message "_file size is NNNNNNN bytes._", before it displays
> "**>>> start**".  This appears to be due to the device taking a few seconds
> to prepare its internal flash memory where the new firmware will be
> stored. DO NOT WORRY - the device is functioning properly.

2. If you are ready to update the firmware in the device, invoke the updater
   with the "-u" option to perform the update, and specify the name of the
   firmware file(s) that you want installed.  

   For example, to update the GW1200 device shown above, use a command such as this:

```
	$ ./ecowitt-firmware-updater -h gw1200-915 -u firmware/GW1200-V1.3.1-4f4535d9ee80468cb18415e088ffd3e7.bin

	MAC Address [dc:da:0c:f4:2f:49]
	Firmware Version [GW1200B_V1.2.2]
	Updating firmware (fname1=firmware/GW1200-V1.3.1-4f4535d9ee80468cb18415e088ffd3e7.bin, fname2=<null>):
	command socket address is 192.168.20.16, port 26420.
	firmware server socket is bound to host address 192.168.20.16, port 37523
	Waiting for inbound connection... 
	update_firmware: received inbound connection from address 192.168.21.86, port 57498
	>>> user1.bin
	file size is 1611376 bytes.
	>>> start
	sending packet    1 - 1024 bytes   - sent=1024
	>>> continue
	sending packet    2 - 1024 bytes   - sent=2048
	>>> continue
	sending packet    3 - 1024 bytes   - sent=3072
	[ ... ]
	>>> continue
	sending packet 1572 - 1024 bytes   - sent=1609728
	>>> continue
	sending packet 1573 - 1024 bytes   - sent=1610752
	>>> continue
	sending packet 1574 -  624 bytes   - sent=1611376
	>>> end
	Client closed the connection.
	do_firmware_service: sent total of 1574 packets, 1611376 bytes.
```


3. Note that the older ESP8266-based devices (specifically the GW1000) require
   **both** a "user1" and "user2" firmware file - if you try to update one of these
   devices without specifying the second (user2) file, you will receive this
   error if the device requests the "user2" file:

```
	$ ./ecowitt-firmware-updater -h gw1000-433 -u firmware/gw1000/gw1000_user1_177.bin

	MAC Address [24:62:ab:16:fd:0d]
	Firmware Version [GW1000_V1.6.9]
	Updating firmware (fname1=firmware/gw1000/gw1000_user1_177.bin, fname2=<null>):
	command socket address is 192.168.20.16, port 65194.
	firmware server socket is bound to host address 192.168.20.16, port 54335
	Waiting for inbound connection... 
	update_firmware: received inbound connection from address 192.168.21.80, port 29964
	>>> user2.bin

	*** ecowitt-firmware-updater: device requested user2, but second firmware image was not specified ***

	Firmware update failed.
```


   You should be able to repeat the command, adding the second image to the
   command line, like so:

```
	$ ./ecowitt-firmware-updater -h gw1000-433 -u firmware/gw1000/gw1000_user1_177.bin firmware/gw1000/gw1000_user2_177.bin

	MAC Address [24:62:ab:16:fd:0d]
	Firmware Version [GW1000_V1.6.9]
	Updating firmware (fname1=firmware/gw1000/gw1000_user1_177.bin, fname2=firmware/gw1000/gw1000_user2_177.bin):
	command socket address is 192.168.20.16, port 37753.
	firmware server socket is bound to host address 192.168.20.16, port 32200
	Waiting for inbound connection... 
	update_firmware: received inbound connection from address 192.168.21.80, port 5607
	>>> user2.bin
	file size is 404932 bytes.
	>>> start
	sending packet    1 - 1024 bytes   - sent=1024
	>>> continue
	sending packet    2 - 1024 bytes   - sent=2048
	>>> continue
	sending packet    3 - 1024 bytes   - sent=3072
	[ ... ]
	sending packet  394 - 1024 bytes   - sent=403456
	>>> continue
	sending packet  395 - 1024 bytes   - sent=404480
	>>> continue
	sending packet  396 -  452 bytes   - sent=404932
	>>> end
	Client closed the connection.
	do_firmware_service: sent total of 396 packets, 404932 bytes.
```

