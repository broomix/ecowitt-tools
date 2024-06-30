<?php

// This is to be installed as /data/ip_api/index.php
// to satisfy the Ecowitt weather gateway's periodic requests,
// which it sends to the hosts
//	cdnrtpdate1.ecowitt.net
//	cdnrtpdate.ecowitt.net
//	rtpdate.ecowitt.net
// via
//	POST /data/ip_api/
// *NOTE* that the device includes the trailing slash on this request.
//
// The posted data sent is similar to:
//	mac = 24:62:AB:16:FD:0D
//	stationtype = GW1000_V1.7.7
//	fields = timezone,utc_offset,dst,date_sunrise,date_sunset
//
// The returned information includes the timezone name, the timezone offset from
// UTC in seconds, whether DST is in effect, and sunrise & sunset times.
//
// Generating the correct values requires knowing the correct timezone and DST
// status - for this, we use the system information. Determining sunrise and
// sunset requires knowing the correct latitude and longitude - to keep this
// simple, this information is hardwired into this script. Look for "EDIT THIS"
// below, and edit the array to provide your local information.
//

$method = $_SERVER["REQUEST_METHOD"];
$ipaddr = $_SERVER["REMOTE_ADDR"];
if (array_key_exists ("HTTP_HOST", $_SERVER))	// may not be set
	$httphost = $_SERVER["HTTP_HOST"];
else
	$httphost = "<notset>";

if (1) {	// DEBUG logging
	syslog (LOG_ERR, sprintf ("running %s - method[%s], host[%s], http_host[%s]",
		__FILE__, $method, $ipaddr, $httphost));

	// log the request parameters, if any:
	$gotsome = 0;
	foreach ($_REQUEST as $k => $v) {
		syslog (LOG_ERR, sprintf (" var[%s] = \"%s\"", $k, $v));
		$gotsome = 1;
	}
	if ($gotsome > 0)
		syslog (LOG_ERR, "###");
}

// Apparently the Ecowitt device wants a 202 code, not just 200.
http_response_code(202);

/* The response seems to be like so:
{"timezone":"America\/Los_Angeles","utc_offset":"-25200","dst":"1","date_sunrise":"06:40","date_sunset":"18:50"}
 */

//------------------------------------------------------------------------------
// EDIT THIS: you can get your latitude and longitude by looking up your
// address on Google Maps. Right-click on the pin, then click the first line
// in the pop-up, which should put your latitude and longitude into the paste
// buffer.  Paste it into this array, then check your work - it should be two
// decimal values with one comma between them.
//------------------------------------------------------------------------------
// For example:
// $latlong = array ( 33.76266954460827, -118.12121280197603 );
$latlong = array ( EDIT THIS );

$latitude = $latlong[0];
$longitude = $latlong[1];


# Get the current DST flag this way:
$timeinfo = localtime(time(), TRUE);
if ($timeinfo["tm_isdst"] > 0)	// ignore the case of being negative
	$dst = 1;
else
	$dst = 0;


// This is ugly, but we can get the timezone offset (in hours and minutes)
// by using strftime with "%z"!
$zoneinfo = strftime ("%z", time());
// this will be something like "-0700".
// now split it into hours and minutes:
$zonehour = intval( $zoneinfo / 100 );		// this will include the sign
$zonemins = intval( $zoneinfo % 100 );

// now figure out what that is in seconds:
$tzoffset = (($zonehour * 60) + $zonemins) * 60;

// Now try to guess the timezone name, based on the offset and DST flag:
$tzname = timezone_name_from_abbr ("", $tzoffset, $dst);

// figure out the sunrise and sunset information for the lat/lon:
$suninfo = date_sun_info (time(), $latitude, $longitude);
$sunrise = strftime ("%H:%M", $suninfo["sunrise"]);
$sunset  = strftime ("%H:%M", $suninfo["sunset"]);


if (1) {	// DEBUG logging
	// Show our work to the system log:
	syslog (LOG_ERR,
		sprintf ("zoneinfo[%s] hour=%d mins=%d tzoffset[%d] dst[%d] name[%s] sunrise[%s] sunset[%s]",
			$zoneinfo, $zonehour, $zonemins, $tzoffset, $dst,
			$tzname, $sunrise, $sunset));
}

// Create an array of everything that we have gathered:
$data = array ("timezone" => $tzname,
		"utc_offset" => "$tzoffset",	// intentionally in quotes
		"dst" => "$dst",		// intentionally in quotes
		"date_sunrise" => $sunrise,
		"date_sunset" => $sunset);

// output it as JSON that the device expects:
printf ("%s\n", json_encode ($data));
?>
