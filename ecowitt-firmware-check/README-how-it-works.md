This is the script that I have developed to check for device firmware updates
and automatically download them.  It uses the "version" API available at

   http://ota.ecowitt.net/api/ota/v1/version/info

to find out about the most recent firmware version available for each model
that is supported by this API.

I figured out how to use this API by watching my GW1100, GW1200, and GW2000
devices in action - while logged into the device (using its web user interface),
I went to the "Device Setting" page and clicked on the "Check firmware"
button.  Watching the packets between my device and the Internet, I could see
that the device was contacting the URL 

    http://ota.ecowitt.net/api/ota/v1/version/info?id=4C%3AB5%3AEB%3AD6%3ABE%3A42&model=GW1100B&time=1719540356&user=1&version=V2.1.8&sign=17F123A95E4D883C9589891BC1D8F4EE

The firmware contains a couple of interesting strings that seem relevant:

	id=%02X%%3A%02X%%3A%02X%%3A%02X%%3A%02X%%3A%02X
	&model=%s&time=%ld&user=1
	&version=V2.1.8
	&sign=

This is also buried in there:

	@ecowittnet

So we can determine that the query string passed to the URL consists of:

	id={your MAC address in upper case, URL-encoded}
	&model={your model name}
	&time={current Unix time}
	&user=1
	&version={current firmware version, with leading 'V'}
	&sign={some sort of signature}


Doing a bit of digging, combined with experimentation, led me to figure out
that the value for "sign" is computed thusly:

	start with the query string assembled so far:
		id=4C%3AB5%3AEB%3AD6%3ABE%3A42&model=GW1100B&time=1719540356&user=1&version=V2.1.8

	add the literal string "@ecowittnet" to the end, producing:
		id=4C%3AB5%3AEB%3AD6%3ABE%3A42&model=GW1100B&time=1719540356&user=1&version=V2.1.8@ecowittnet

	compute the md5 sum of this string and convert to UPPER case:
		DFAC2881CF652CA8A8A458B556C7A487

	append '&sign=' and the md5 sum to the query string, producing:
		id=4C%3AB5%3AEB%3AD6%3ABE%3A42&model=GW1100B&time=1719540356&user=1&version=V2.1.8&sign=DFAC2881CF652CA8A8A458B556C7A487

So the complete URL becomes:

	http://ota.ecowitt.net/api/ota/v1/version/info?id=4C%3AB5%3AEB%3AD6%3ABE%3A42&model=GW1100B&time=1719540356&user=1&version=V2.1.8&sign=DFAC2881CF652CA8A8A458B556C7A487

Doing a simple HTTP GET of this URL returns information similar to this:

{"code":0,"msg":"Success","time":"1719540525","data":{"id":415,"name":"V2.3.2","content":"1.Support ws85 sensor.\r\n2.Fixed some known bugs.","attach1file":"https:\/\/osswww.ecowitt.net\/ota\/20240425\/568ad1f21d0fe5e612b0e1ae4b7adcdd.bin","attach2file":"","queryintval":86400}}

Formatted for readability, we have this:
	{"code":0,
	 "msg":"Success",
	 "time":"1719540525",
	 "data":{"id":415,
		 "name":"V2.3.2",
		 "content":"1.Support ws85 sensor.\r\n2.Fixed some known bugs.",
		 "attach1file":"https:\/\/osswww.ecowitt.net\/ota\/20240425\/568ad1f21d0fe5e612b0e1ae4b7adcdd.bin",
		 "attach2file":"",
		 "queryintval":86400
		}
	}

So we can see that the current firmware for this model is "V2.3.2",
the changelog information is "1.Support ws85 sensor.\r\n2.Fixed some known bugs.",
and the firmware file is available for download at
to download "https:\/\/osswww.ecowitt.net\/ota\/20240425\/568ad1f21d0fe5e612b0e1ae4b7adcdd.bin"
