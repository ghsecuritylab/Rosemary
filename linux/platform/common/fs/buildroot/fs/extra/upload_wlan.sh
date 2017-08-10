insmod /root/wlan.ko ifname=wlan0 if2name=p2p0
ifconfig wlan0 up
wpa_supplicant -B -i wlan0 -D nl80211 -c /etc/wpa_supplicant.conf
