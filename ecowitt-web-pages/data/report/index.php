<?php

// This is to be installed as /data/report/index.php
//
// The device uses POST to host "cdnrtpdate.ecowitt.net", like so:
//	POST /data/report/
// It uses this to post the Ecowitt-formatted weather data,
// when the "Ecowitt.net" interval is non-zero.
//
// NOTE that the device includes the trailing slash on this request.

// Sample information includes:
//	[PASSKEY] = "606.........................B888"
//	[stationtype] = "GW1100B_V2.3.2"
//	[runtime] = "100570"
//	[heap] = "23984"
//	[dateutc] = "2024-06-28 09:17:20"
//	[tempinf] = "78.08"
//	[humidityin] = "59"
//	[baromrelin] = "29.958"
//	[baromabsin] = "29.816"
//	[tempf] = "64.40"
//	[humidity] = "87"
//	[winddir] = "234"
//	[windspeedmph] = "0.22"
//	[windgustmph] = "1.12"
//	[maxdailygust] = "9.17"
//	[solarradiation] = "0.00"
//	[uv] = "0"
//	[rainratein] = "0.000"
//	[eventrainin] = "0.000"
//	[hourlyrainin] = "0.000"
//	[dailyrainin] = "0.000"
//	[weeklyrainin] = "0.000"
//	[monthlyrainin] = "0.004"
//	[yearlyrainin] = "17.134"
//	[soilmoisture1] = "0"
//	[soilad1] = "31"
//	[tf_co2] = "87.98"
//	[humi_co2] = "44"
//	[pm1_co2] = "2.6"
//	[pm1_24h_co2] = "3.1"
//	[pm4_co2] = "4.8"
//	[pm4_24h_co2] = "4.3"
//	[pm25_co2] = "3.8"
//	[pm25_24h_co2] = "3.9"
//	[pm10_co2] = "5.3"
//	[pm10_24h_co2] = "4.6"
//	[co2] = "500"
//	[co2_24h] = "459"
//	[lightning_num] = "4"
//	[lightning] = "12"
//	[lightning_time] = "1719564960"
//	[leak_ch1] = "0"
//	[tf_ch1] = "66.02"
//	[tf_ch2] = "55.04"
//	[tf_ch3] = "76.64"
//	[tf_ch4] = "-6.16"
//	[tf_ch5] = "80.78"
//	[tf_ch6] = "79.16"
//	[wh65batt] = "0"
//	[wh68batt] = "1.86"
//	[wh25batt] = "0"
//	[soilbatt1] = "1.5"
//	[wh57batt] = "3"
//	[leakbatt1] = "5"
//	[tf_batt1] = "1.42"
//	[tf_batt5] = "1.32"
//	[ckset] = "0"

$method = $_SERVER["REQUEST_METHOD"];
$ipaddr = $_SERVER["REMOTE_ADDR"];
if (array_key_exists ("HTTP_HOST", $_SERVER))	// may not be set
	$httphost = $_SERVER["HTTP_HOST"];
else
	$httphost = "<notset>";

if (1) {	// DEBUG logging
	syslog (LOG_ERR, sprintf ("running %s - method[%s], host[%s], http_host[%s]",
		__FILE__, $method, $ipaddr, $httphost));

	foreach ($_REQUEST as $k => $v) {
		syslog (LOG_ERR, sprintf (" var[%s] = \"%s\"", $k, $v));
	}
	syslog (LOG_ERR, "###");
}

// Apparently the Ecowitt device wants a 202 code, not just 200.
http_response_code(202);

printf ("ok\r\n");

?>
