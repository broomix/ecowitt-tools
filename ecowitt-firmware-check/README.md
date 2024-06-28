This is the script that I have developed to check for device firmware updates
and automatically download them from the Ecowitt web site.

(If you want to know about the protocol, see README-how-it-works.md)

How to use it, in increasing levels of difficulty:

1. If all you want to do is see the latest firmware version available for your model, run:

	./check-ecowitt-update -m GW1200B

(substitute "GW1200B" with the correct model for your device, of course.)

This will show something like:

	Model:   GW1200B
	Version: V1.3.1
	Updates: - Added support for WS85.
		 - Fixed for WH46 transmitter not receiving.
		 - Fixed some known bug.
	URL:     https://osswww.ecowitt.net/ota/20240429/4f4535d9ee80468cb18415e088ffd3e7.bin

You can use the displayed URL to download the firmware file if you'd like.


2. If you want the tool to automatically download the firmware, run:

	./check-ecowitt-update -m GW1200B -d firmware

("firmware" is the name of an existing directory where you want the files
stored. Use "." if you simply want them in the current directory.)

And it will display:

	Model:   GW1200B
	Version: V1.3.1
	Updates: - Added support for WS85.
		 - Fixed for WH46 transmitter not receiving.
		 - Fixed some known bug.
	URL:     https://osswww.ecowitt.net/ota/20240429/4f4535d9ee80468cb18415e088ffd3e7.bin
	Successfully downloaded firmware V1.3.1 to firmware/GW1200-V1.3.1-4f4535d9ee80468cb18415e088ffd3e7.bin

As it has indicated, the firmware file has been downloaded into the specified directory, too.

If the firmware version has already been downloaded, you will instead get this output:

	check-ecowitt-update: already have firmware/GW1200-V1.3.1-4f4535d9ee80468cb18415e088ffd3e7.bin


3. If you want the tool to automatically download the firmware AND record the
   information to a log file, add "-l logfile" to the command line - run:

	./check-ecowitt-update -m GW1200 -d firmware -l logfile

This will display to the screen:

	Model:   GW1200B
	Version: V1.3.1
	Updates: - Added support for WS85.
		 - Fixed for WH46 transmitter not receiving.
		 - Fixed some known bug.
	URL:     https://osswww.ecowitt.net/ota/20240429/4f4535d9ee80468cb18415e088ffd3e7.bin
	Successfully downloaded firmware V1.3.1 to firmware/GW1200-V1.3.1-4f4535d9ee80468cb18415e088ffd3e7.bin

And it will add to the log file:

	Date:    2024-06-27T21:31:20-07:00
	Model:   GW1200B
	Version: V1.3.1
	Updates: - Added support for WS85.
		 - Fixed for WH46 transmitter not receiving.
		 - Fixed some known bug.
	URL:     https://osswww.ecowitt.net/ota/20240429/4f4535d9ee80468cb18415e088ffd3e7.bin
	file:    firmware/GW1200-V1.3.1-4f4535d9ee80468cb18415e088ffd3e7.bin


4. If you want to run the tool automatically (like from "cron"), and you only
   want it to generate output when an update is available, add the "-q" option:

	./check-ecowitt-update -m GW1200B -d firmware -l logfile -q

In this case, if a new version is available, it will generate the same output
as example 3 above (including to the log file), and "cron" will send the output
to the cron job owner by email.  But if it already has the latest version, it
will produce no output, and exit with status 0, and "cron" will keep quiet.


ADDITIONAL NOTES:

When running the tool from "cron", note that the process will typically be
started in the home directory of the cron job owner, so any files written
will be in that directory -- unless you specify either a full or relative
pathname.  You may also need to provide the full pathname to the tool itself,
unless you have installed it into a directory in the standard $PATH.

The recommended way to run the tool from a cron entry is like so:

	$HOME/bin/check-ecowitt-update -m GW1200B -d $HOME/ecowitt/firmware -l $HOME/ecowitt/firmware/logfile -q

(This assumes you have installed the tool in your $HOME/bin directory - adjust
 the command name as needed if you have done differently.)

