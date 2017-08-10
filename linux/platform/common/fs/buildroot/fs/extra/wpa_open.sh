SSID='"'$1'"'

echo "SSID=${SSID}"

wpa_cli -iwlan0 add_network
wpa_cli -iwlan0 set_network 1 auth_alg OPEN
wpa_cli -iwlan0 set_network 1 key_mgmt NONE
wpa_cli -iwlan0 set_network 1 mode 0
wpa_cli -iwlan0 set_network 1 ssid $SSID
wpa_cli -iwlan0 select_network 1
wpa_cli -iwlan0 enable_network 1
wpa_cli -iwlan0 reassociate
wpa_cli -iwlan0 status
