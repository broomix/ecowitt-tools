#!/bin/sh

# A simple shell script to download a few Ecowitt firmware images.

#silent="--silent"
silent=

while read dst src
do
	echo "Downloading $src -> $dst"
	curl $silent --remote-time -o "$dst" "$src"
	echo ""
done << \EOF
GW1100-V2.0.6-63bf599c5070fdb1cc5daf0807c2c038.bin	https://osswww.ecowitt.net/ota/20211013/63bf599c5070fdb1cc5daf0807c2c038.bin
GW1100-V2.1.8-db2869ec13962015c813128b74693d13.bin	https://osswww.ecowitt.net/ota/20220811/db2869ec13962015c813128b74693d13.bin
GW1100-V2.3.1-68dfe9b8d7e459739e85f2ba44188cf2.bin	https://osswww.ecowitt.net/ota/20240126/68dfe9b8d7e459739e85f2ba44188cf2.bin
GW1100-V2.3.2-568ad1f21d0fe5e612b0e1ae4b7adcdd.bin	https://osswww.ecowitt.net/ota/20240425/568ad1f21d0fe5e612b0e1ae4b7adcdd.bin
GW1100-V2.3.3-818c81b00866afbaf227d21d1b44e4ae.bin	https://oss.ecowitt.net/ota/20240628/818c81b00866afbaf227d21d1b44e4ae.bin
GW1200-V1.2.8-c63ef3ec41b6c8d9e7e8a40cdcde0658.bin	https://osswww.ecowitt.net/ota/20240219/c63ef3ec41b6c8d9e7e8a40cdcde0658.bin
GW1200-V1.3.0-fa57569a964b6a851a4537bb50a2910a.bin	https://osswww.ecowitt.net/ota/20240427/fa57569a964b6a851a4537bb50a2910a.bin
GW1200-V1.3.1-4f4535d9ee80468cb18415e088ffd3e7.bin	https://osswww.ecowitt.net/ota/20240429/4f4535d9ee80468cb18415e088ffd3e7.bin
GW1200-V1.3.2-fd0b98b0f7e7f6b3598fd5aaa6a6c7cc.bin	https://oss.ecowitt.net/ota/20240704/fd0b98b0f7e7f6b3598fd5aaa6a6c7cc.bin
GW2000-V3.1.2-d0230943cdf23c9469244c49e32bc0e1.bin	https://osswww.ecowitt.net/ota/20240312/d0230943cdf23c9469244c49e32bc0e1.bin
GW2000-V3.1.3-fedec1810580d5bc0b8a0c26c057202c.bin	https://oss.ecowitt.net/ota/20240521/fedec1810580d5bc0b8a0c26c057202c.bin
GW2000-V3.1.4-2c83b647854e9a1c0ba560ec96082776.bin	https://oss.ecowitt.net/ota/20240621/2c83b647854e9a1c0ba560ec96082776.bin
HP2550-V1.9.6-0e1c35332fbe187dd569b91c28a2e395.bin	https://osswww.ecowitt.net/ota/20240502/0e1c35332fbe187dd569b91c28a2e395.bin
WN1820-V1.2.8-0a4e883b2fed37bf2edbcc77c744c420.bin	https://osswww.ecowitt.net/ota/20240220/0a4e883b2fed37bf2edbcc77c744c420.bin
WN1820-V1.3.0-35273a44565d572fb1b62716f21f306c.bin	https://osswww.ecowitt.net/ota/20240430/35273a44565d572fb1b62716f21f306c.bin
WS3800-V1.2.8.1-43ebdcb4964e074d406bfa3a2bf76bb4.bin	https://osswww.ecowitt.net/ota/20240325/43ebdcb4964e074d406bfa3a2bf76bb4.bin
EOF

