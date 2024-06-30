<?php

// This is to be installed as api/index/initialization.php
// to satisfy the Ecowitt weather gateway's startup request ...
//
// The device sends to the host 
//		ota.ecowitt.net
//
// The device invokes this via
//  GET /api/index/initialization?mac=30:83:98:7A:E2:9D&model=GW1100C&version=V2.3.2&sign=63A79A95BEED8A20ACA9C34B8AFD046D&last_ret=-1
//
//	By observation, last_ret can have the values "-1", "1", "3", and "4".
//
// The real host responds with HTTP code 200 like so:
//
// {"code":0,"msg":"success","time":"1719790567","data":{"ip":"47.108.224.137","is_cnip":false,"ota_host":["ota.ecowitt.net"],"rtp_host":["rtpdate.ecowitt.net","cdnrtpdate.ecowitt.net"],"rtpmedia_host":["rtpmedia.ecowitt.net","cdnrtpmedia.ecowitt.net"],"mqtt_host":["iot0.ecowitt.net","iot.ecowitt.net"],"mqtts_url":["iot0.ecowitt.net:8883","iot.ecowitt.net:8883"],"api_host":["api.ecowitt.net"]}}
//
// Formatted legibly, that is:
// {
//	"code":0,
//	"msg":"success",
//	"time":"1719790567",
//	"data":{
//		"ip":"47.108.224.137",
//		"is_cnip":false,
//		"ota_host":["ota.ecowitt.net"],
//		"rtp_host":["rtpdate.ecowitt.net","cdnrtpdate.ecowitt.net"],
//		"rtpmedia_host":["rtpmedia.ecowitt.net","cdnrtpmedia.ecowitt.net"],
//		"mqtt_host":["iot0.ecowitt.net","iot.ecowitt.net"],
//		"mqtts_url":["iot0.ecowitt.net:8883","iot.ecowitt.net:8883"],
//		"api_host":["api.ecowitt.net"]
//		}
// }


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
?>
