# Ecowitt Local Web Pages

```
Jonathan Broome
jbroome@wao.com
June 2024
```

This is a set of PHP scripts and files intended to be used to provide a
"fake" local-only version of the Ecowitt web sites, providing the various
interfaces that the Ecowitt weather gateway and console devices expect
to find online.  The intention is that all of these will be hosted on
your own local web server, and your local DNS will have entries for
the various Ecowitt host names which all point to your local web server.

With all of this in place, your devices should not need an external
Internet connection to perform their basic functionality.  Of course,
if you want them to post data to Wunderground or Weathercloud, et al,
they will still need to be able to reach those sites.


## Edits you will want to make
You will want to edit a few of these files to provide your localized
information:

1. The script `data/ip_api/index.php` MUST be edited to specify your
   own latitude and longitude information -- look for the string
   "EDIT THIS" and follow the instructions.

2. The file `api/ota/v1/version/firmware-info` should be edited to
   specify the correct "urlbase" value for your local web server, with
   the proper directory where you will store the firmware images.

   This same file also needs to be edited to reflect the device models
   that you will support, and to specify the actual firmware images
   that you are making available to these devices.  (You will need
   to download these images from Ecowitt - they are not included in
   this repository.)


## DNS
These are the DNS names that I have found necessary to "fake" (redirect
to my local web server) in order for everything to work properly:

*	cdnrtpdate.ecowitt.net
*	cdnrtpdate1.ecowitt.net
*	ota.ecowitt.net
*	rtpdate.ecowitt.net
*	rtpdate1.ecowitt.net

(Note that while this list is accurate as of June 2024, it it possible that
more names will be required with future firmware updates.)

I have my own local DNS server (ISC BIND, aka "named") running, and so I
have created my own zone files which specify each of these names with the
local IP address for my web server.  I use a network-private address
(e.g. "192.168.x.x"), so that there is no risk of my local data being
accessible from outside the LAN.

For my web server, I use Apache with PHP. In order to properly respond to
requests for these ecowitt.net names, I have an apache "vhost" configured
with ServerName and ServerAlias entries for each of the hostnames listed
above.  I have the DocumentRoot pointed to the top of the directory
structure containing the various files in this repository.  In addition
to these files, I have a directory named "firmware", containing each
of the firmware files downloaded from Ecowitt. (Remember to edit
`api/ota/v1/version/firmware-info` to reflect these firmware files.)
