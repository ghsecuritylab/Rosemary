SSID='"'$1'"'
PSK='"'$2'"'

echo "SSID=${SSID}, PSK=${PSK}"

wpa_cli -iwlan0 add_network
wpa_cli -iwlan0 set_network 0 auth_alg OPEN
wpa_cli -iwlan0 set_network 0 ssid $SSID
wpa_cli -iwlan0 set_network 0 key_mgmt WPA-PSK
wpa_cli -iwlan0 set_network 0 psk $PSK
wpa_cli -iwlan0 set_network 0 pairwise CCMP TKIP
wpa_cli -iwlan0 set_network 0 group CCMP TKIP
wpa_cli -iwlan0 set_network 0 mode 0
wpa_cli -iwlan0 select_network 0
wpa_cli -iwlan0 enable_network 0
wpa_cli -iwlan0 reassociate
wpa_cli -iwlan0 status

