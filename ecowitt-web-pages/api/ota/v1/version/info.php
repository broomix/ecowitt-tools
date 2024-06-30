<?php

// This is to be installed as /api/ota/v1/version/info/index.php
// to satisfy the Ecowitt weather gateway's periodic request
// to check firmware version status.
//
// The device periodically invokes this using GET, like so:
//  GET /api/ota/v1/version/info?id=30%3A83%3A98%3AA7%3A2E%3AD9&model=GW1100C&time=1715698261&user=1&version=V2.3.2&sign=0004C297194E4ACD3E2D67469442BA5F
// So the parameters are:
//	id=30%3A83%3A98%3AA7%3A2E%3AD9	-- URL-encoded device MAC address
//	model=GW1100C
//	time=1715698261			-- Unix timestamp
//	user=1
//	version=V2.3.2
//	sign=0004C297194E4ACD3E2D67469442BA5F	-- MD5 sum of previous parameters + "@ecowittnet"

//
// This script will read the file "firmware-info" to learn about the versions
// of firmware available for each supported model.  This file can also be
// edited to set a preference for a particular device based on its MAC address.
//


// This function reads the firmware information file and builds up the data
// structures that we need to perform our tasks below.  Note that this is
// a very simple parser, so it won't be very difficult to confuse it with
// badly-formatted data.
// If successful, this function will fill in $modelinfo and $urlbase, then
// return true.
function readfirmwareinfo ($fname, &$modelinfo, &$urlbase)
{
	$scriptname = __FILE__;		// for use in error messages

	// set some reasonable defaults:
	$modelname = null;	// no model yet
	$models = array ();	// no device models yet
	$fwinfo = null;		// no firmware info yet, either.

	// determine the current directory for use later.
	$thisdir = getcwd();

	// open the firmware information file.
	$fp = @fopen ($fname, "r");
	if ($fp == NULL) {
		syslog (log_ERR, sprintf ("%s: cannot open firmware description file \"%s\" in directory \"%s\"", $scriptname, $fname, $thisdir));
		return false;
	}

	// okay, got the file open - process the contents
	$linenum = 0;
	for ( ;; ) {
		// read the next line from the file
		$line = fgets ($fp, 4096);
		if (feof ($fp))		// end of file - bail
			break;
		$linenum++;

		// strip the newline, collapse whitespace.
		$line = trim ($line);

		// Split anything with '#' - the trailing part will be ignored
		// as a comment.
		$fields = preg_split ('/#.*$/', $line, 3, PREG_SPLIT_NO_EMPTY);
		$fieldcount = count ($fields);
		if ($fieldcount == 0)	// nothing remaining before the "#"
			continue;
		$line = $fields[0];

		// now split the remaining line into 2 pieces - keyword, value
		$fields = preg_split ("/[\s]+/", $line, 2);
		$cnt = count ($fields);		// count the number of fields

		if ($cnt == 0)			// (effectively) blank line
			continue;
		if ($cnt == 1) {		// malformed line - no value
			syslog (LOG_ERR, "Bad line \"$line\" at line $linenum in \"$fname\" in $thisdir");
			return false;
		}

		$kw = trim ($fields[0]);		// keyword
		$val = trim ($fields[1]);	// value

		switch (strtolower ($kw)) {
		case "urlbase":		// RHS is base URL to prepend to firmware filenames
			$urlbase = $val;
			break;

		case "model":		// start of information for one model
					// RHS is is model name, e.g. "GW1100"
			$modelname = $val;
			$models[$modelname] = array();
			$fwinfo = null;
			break;

		case "firmware":	// start of info for a particular version
					// RHS is the firmware version, i.e. "V2.0.6"
			if ($modelname == null) {
				syslog (LOG_ERR, "$scriptname: keyword \"$kw\" must follow a \"model\" entry - line $linenum in \"$fname\" in $thisdir");
				return false;
				break;
			}
			$version = $val;
			$fwinfo["version"] = $val;
			$models[$modelname]["firmware"]["$version"] = $fwinfo;
			break;

		case "file":		// filename for the current version
		case "file1":		// filename for the current version (file1 for two-file models)
			if ($previouskw != "firmware") {
				syslog (LOG_ERR, "$scriptname: keyword \"$kw\" must follow \"firmware\" entry - line $linenum in \"$fname\" in $thisdir");
				return false;
				break;
			}
			$fwinfo["file1"] = $val;
			$models[$modelname]["firmware"]["$version"] = $fwinfo;
			break;

		case "file2":		// filename for the current version (file2 for two-file models)
			if ($previouskw != "file1") {
				syslog (LOG_ERR, "$scriptname: keyword \"$kw\" must follow \"file1\" entry - line $linenum in \"$fname\" in $thisdir");
				return false;
				break;
			}
			$fwinfo["file2"] = $val;
			$models[$modelname]["firmware"]["$version"] = $fwinfo;
			break;

		case "log":		// changelog information
			if ($previouskw != "file" && $previouskw != "file1" && $previouskw != "file2") {
				syslog (LOG_ERR, "$scriptname: keyword \"$kw\" must follow \"file*\" entry - line $linenum in \"$fname\" in $thisdir");
				return false;
				break;
			}
			$changelog = $val;
			$fwinfo["changelog"] = $val;
			$models[$modelname]["firmware"]["$version"] = $fwinfo;
			break;

		case "want":		// RHS is the preferred version
					// RHS may be simply "V2.3.2"
					// or rhs it may specify version and MAC
					//  e.g. "V2.1.8 for dc:da:0c:fa:c5:e0"
			if ($modelname == null) {
				syslog (LOG_ERR, "$scriptname: keyword \"$kw\" must be within a \"model\" entry - line $linenum in \"$fname\" in $thisdir");
				return false;
				break;
			}
			// see if $val includes "for MACADDR"
			$fields = preg_split ('/for\s+(.*)$/', $val, 3, PREG_SPLIT_DELIM_CAPTURE | PREG_SPLIT_NO_EMPTY);
			if (count ($fields) == 1) {	// want V2.3.2
				// specify the default version for all
				$models[$modelname]["want"]["default"] = trim ($fields[0]);
			} else {			// want V2.1.8 for dc:da:0c:fa:c5:e0
				// specify the version for a particular MAC address
				$macaddr = trim ($fields[1]);
				$models[$modelname]["want"][strtolower($macaddr)] = trim ($fields[0]);
			}
			break;

		default:		// unknown keyword
			syslog (LOG_ERR, "$scriptname: unknown keyword \"$kw\" specified at line $linenum in \"$fname\" in $thisdir");
			return false;
			break;
		}
		// remember this keyword for the next pass through the loop:
		$previouskw = $kw;
	}
	fclose ($fp);
	$modelinfo = $models;	// fill in the caller's parameter
	return true;
}


/*
 * Various errors found when probing the real Ecowitt page:
{"code":41000,"msg":"id require","time":"1707781625","data":[]}
{"code":41000,"msg":"model require","time":"1707781625","data":[]}
{"code":41000,"msg":"user require","time":"1707781625","data":[]}
{"code":41000,"msg":"user must be integer","time":"1707781626","data":[]}
{"code":41000,"msg":"version require","time":"1707781626","data":[]}
{"code":41000,"msg":"time require","time":"1707781626","data":[]}
{"code":41000,"msg":"sign require","time":"1707781626","data":[]}
{"code":41000,"msg":"sign must be 32 characters","time":"1707781627","data":[]}
{"code":40012,"msg":"invalid time","time":"1707781627","data":[]}
{"code":40012,"msg":"invalid sign","time":"1707781627","data":[]}
{"code":40013,"msg":"invalid model","time":"1707782551","data":[]}
{"code":40014,"msg":"invalid version","time":"1707782667","data":[]}
{"code":-1,"msg":"Operation too frequent","time":"1707782840","data":[]}
 */

function sendresponse ($code, $msg, $data)
{
	// The Ecowitt site seems to always responds with a standard 200 code:
	http_response_code(200);

	// on failure, the data element is always an empty array, like so:
	// {"code":41000,"msg":"id require","time":"1707781625","data":[]}
	if ($data == null) {
		$data = array();	// make it an empty array
	}

	if ($data != null) {
		// use $data as-is
	}

	// Create the complete response - "code" and "msg" change for
	// "current" vs "needs updating".
	$response = array ("code" => $code,	// 0 or -1 or 4xxxx
			   "msg"  => $msg,	// "Success" or "The firmware is up to date"
			   "time" => time(),	// Unix timestamp
			   "data" => $data
			);

	// output it as JSON that the device expects:
	$json = json_encode ($response);

	// un-do the JSON escaping of the backslashes - we want "\r" and "\n"
	// to come through to the device.
	$json = str_replace ('\\\\', '\\', $json);

	printf ("%s\n", $json);

	syslog (LOG_ERR, sprintf ("response is [%s]", $json));

	// we're done - exit now.
	exit();
}


// Get the inbound request information:
$method = $_SERVER["REQUEST_METHOD"];
$ipaddr = $_SERVER["REMOTE_ADDR"];
if (array_key_exists ("HTTP_HOST", $_SERVER))	// may not be set
	$httphost = $_SERVER["HTTP_HOST"];
else
	$httphost = "<notset>";

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

// The Ecowitt site responds to this with a standard 200 code:
http_response_code(200);

if (readfirmwareinfo ("firmware-info", $modelinfo, $urlbase) != true) {
	// we encountered problems with our firmware information file. Oops.
	$code = -1;
	$msg = "internal configuration error";
	sendresponse ($code, $msg, null);
}



// Get the id, which is the device MAC address:
if (! array_key_exists ("id", $_REQUEST)) {
	syslog (LOG_ERR, sprintf ("missing id (MAC address"));
	http_response_code(301);	//XXX what is the response code?
	$code = 41000;
	$msg = "id require";

	// {"code":41000,"msg":"id require","time":"1707781625","data":[]}
	sendresponse ($code, $msg, null);
}
$id = strtolower ($_REQUEST["id"]);		// this is the MAC address

// Get the model and version from the request:
if (! array_key_exists ("model", $_REQUEST)) {
	syslog (LOG_ERR, sprintf ("missing model"));
	http_response_code(301);	//XXX what is the response code?

	$code = 41000;
	$msg = "model require";

	// {"code":41000,"msg":"model require","time":"1707781625","data":[]}
	sendresponse ($code, $msg, null);
}
$model = $_REQUEST["model"];

if (! array_key_exists ("version", $_REQUEST)) {
	syslog (LOG_ERR, sprintf ("missing version"));
	http_response_code(301);	//XXX what is the response code?

	$code = 41000;
	$msg = "version require";

	// {"code":41000,"msg":"version require","time":"1707781626","data":[]}
	sendresponse ($code, $msg, null);
}
$currentversion = $_REQUEST["version"];

// If we don't like the specified version:
//	$response["code"] = "40014";
//	$response["msg"] = "invalid version";
// {"code":40014,"msg":"invalid version","time":"1716531476","data":[]}

// strip the trailing letter from the model (A,B,C,D):
$model = substr ($model, 0, -1);	// eg. "GW2000B" becomes "GW2000"

// find the firmware information for the model:
if (! array_key_exists ("$model", $modelinfo)) {
	syslog (LOG_ERR, sprintf ("unsupported model \"%s\"", $model));
	http_response_code(301);	//XXX what is the response code?

	$code = 40013;
	$msg = "invalid model";

	// {"code":40013,"msg":"invalid model","time":"1716531390","data":[]}
	sendresponse ($code, $msg, null);
}
$modelinfo = $modelinfo["$model"];

// First see if we have a version specified for this particular MAC address:
if (array_key_exists ("want", $modelinfo) && array_key_exists ($id, $modelinfo["want"])) {
	// use the version specified for this one MAC address
	$desiredversion = $modelinfo["want"][$id];
	syslog (LOG_ERR, sprintf ("version for MAC[%s] is [%s]", $id, $desiredversion));
} else if (array_key_exists ("want", $modelinfo) && array_key_exists ("default", $modelinfo["want"])) {
	// use the default version
	$desiredversion = $modelinfo["want"]["default"];
	syslog (LOG_ERR, sprintf ("default version is [%s]", $desiredversion));
} else {
	// If the wanted version is not specified, find the highest version
	// listed in $modelinfo[firmware]

	$versions = array_keys ($modelinfo["firmware"]);
	syslog (LOG_ERR, sprintf ("versions for model %s: { %s }",
			$model, var_export($versions, true)));
	$highest = "";
	$lowest = "";
	foreach ($versions as $k => $v) {
		// use version_compare(string $version1, string $version2) -
		// returns -1 if the first version is lower than the second,
		// 0 if they are equal, and 1 if the second is lower.
		$r = version_compare($v, $highest);
		if ($r > 0)	// $v is higher than the highest so far
			$highest = $v;
		$r = version_compare($v, $lowest);
		if ($r < 0 || $lowest == "")	// $v is lower than lowest
			$lowest = $v;
	}
	syslog (LOG_ERR, sprintf ("version: highest[%s] lowest[%s]",
			$highest, $lowest));

	// use the highest version that we found:
	$desiredversion = $highest;
}

syslog (LOG_ERR, sprintf ("model[%s] - desired version[%s] current[%s]",
	$model, $desiredversion, $currentversion));

// get to the information for the specific firmware instance:
$fwinfo = $modelinfo["firmware"]["$desiredversion"];

// We always fill in the information for the desired firmware:
$name = $desiredversion;		// version string
$content = $fwinfo["changelog"];	// changelog for this version

// URL for "user1" file:
$attach1file = "${urlbase}/" . $fwinfo["file1"];

// URL for "user2" file, if any:
if (isset ($fwinfo["file2"])) {
	$attach2file = "${urlbase}/" . $fwinfo["file2"];
} else {
	$attach2file = "";
}

// Is the version the same as what the device already has?
if (strcasecmp ($currentversion, $desiredversion) == 0) {
	// yes - the device is current
	$code = -1;
	$msg = "The firmware is up to date";
} else {
	// no - set the code and msg as needed to trigger update:
	$code = 0;
	$msg = "Success";
}

/* If the firmware is the correct version, the response is like so:
{"code":-1,"msg":"The firmware is up to date","time":"1715668782","data":{"id":399,"name":"V3.1.2","content":"- Fix the memory leaks and some known crashes.\r\n- T&HP sensor can replace indoor temperature, humidity, pressure data.\r\n- Fixed a bug when wifi is turned off.","attach1file":"https:\/\/osswww.ecowitt.net\/ota\/20240312\/d0230943cdf23c9469244c49e32bc0e1.bin","attach2file":"","queryintval":86400}}
 *
 * and if the firmware needs to be upgraded, the response is like so:
{"code":0,"msg":"Success","time":"1715668969","data":{"id":399,"name":"V3.1.2","content":"- Fix the memory leaks and some known crashes.\r\n- T&HP sensor can replace indoor temperature, humidity, pressure data.\r\n- Fixed a bug when wifi is turned off.","attach1file":"https:\/\/osswww.ecowitt.net\/ota\/20240312\/d0230943cdf23c9469244c49e32bc0e1.bin","attach2file":"","queryintval":86400}}
 */

// figure out the "data" part of the response - this is always provided:
$data = array ("id" => posix_getpid(),
		"name" => $name,
		"content" => $content,
		"attach1file" => $attach1file,
		"attach2file" => $attach2file,	// may be empty
		"queryintval" => 86400		// 24 hours, in seconds
		);

// Send the complete response, and we're done:
sendresponse ($code, $msg, $data);
?>
