cat <<xEOFx >/etc/systemd/system/monitor\@.service 
[Unit]
Description=CO2, Temperature and Relative Humidity Monitor (%i)

[Service]
Restart=always
RestartSec=30s
WatchdogSec=90s
NotifyAccess=main
EnvironmentFile=/etc/systemd/monitor.service.d/local-%i.conf
ExecStart=/usr/local/bin/co2monitor -c /etc/systemd/monitor.service.d/local-%i.conf

[Install]
WantedBy=multi-user.target
xEOFx

mkdir -p /etc/systemd/monitor.service.d

cat <<xEOFx >/etc/systemd/monitor.service.d/local-co2.conf 
# serial port for CO2 monitor
CO2Port="/dev/ttyAMA0"


#LogDir="/var/tmp/co2monitor"
# Log level is one of DEBUG (verbose), INFO, NOTICE, WARNING, ERR, CRIT, ALERT (highest)
LogLevel=DEBUG

# How often to test network connectivity (in seconds)
NetworkCheckPeriod=30

# How often to kick watchdog. should not be more than half WatchdogSec
WatchdogKickPeriod=45

NetDevice="wlan0"

# Minimum number of seconds network device should be down before
# reboot is initiated.
NetDeviceDownRebootMinTime=5

# These will be used in preference to respective environment variables (if set).
SDL_FBDEV=/dev/fb1
SDL_MOUSEDEV=/dev/input/touchscreen
SDL_MOUSEDRV="TSLIB"
SDL_VIDEODRIVER=fbcon
SDL_MOUSE_RELATIVE="0"

# root dir where screen fonts are stored
SDL_TTF_DIR=/usr/local/bin/

# root dir where screen fonts are stored
SDL_BMP_DIR=/usr/local/bin/

# Screen refresh rate in FPS
ScreenRefreshRate=15

# Screen saver kicks in after this many seconds of inactivity
ScreenTimeout=120

# Amount of time (minutes) fan stays on for manual override
FanOnOverrideTime=60

# Rel Humidity threshold (%) above which fan starts
RelHumFanOnThreshold=80

# CO2 threshold (ppm) above which fan starts
CO2FanOnThreshold=450


xEOFx

systemctl enable monitor@co2.service
systemctl start monitor@co2.service

cd /etc/httpd/conf
openssl req -new -x509 -nodes -newkey rsa:4096 -keyout apache.key -out apache.crt -days 1095 <<xEOFx
CA
Ontario
Nepean
Pat Wilson Software Design Inc.

Patrick Wilson
patw@patwsd.com
xEOFx
chmod 400 apache.key
chmod 444 apache.crt

# uncomment these lines in /etc/httpd/conf/httpd.conf
LoadModule ssl_module modules/mod_ssl.so
LoadModule socache_shmcb_module modules/mod_socache_shmcb.so
Include conf/extra/httpd-ssl.conf

# Then edit /etc/httpd/conf/extra/httpd-ssl.conf to reflect the new key and certificate:
SSLCertificateFile "/etc/httpd/conf/apache.crt"
SSLCertificateKeyFile "/etc/httpd/conf/apache.key"

systemctl enable httpd.service
systemctl start httpd.service
