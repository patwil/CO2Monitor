cat <<xEOFx >/etc/systemd/system/monitor\@.service 
[Unit]
Description=CO2, Temperature and Relative Humidity Monitor (%i)

[Service]
Restart=always
RestartSec=30s
WatchdogSec=90s
NotifyAccess=main
EnvironmentFile=/etc/systemd/monitor.service.d/local-%i.conf
ExecStart=/usr/local/bin/co2monitor --port=${CO2PORT} --logdir=${LOGDIR}

[Install]
WantedBy=multi-user.target
xEOFx

mkdir -p /etc/systemd/monitor.service.d

cat <<xEOFx >/etc/systemd/monitor.service.d/local-co2.conf 
CO2PORT="/dev/ttyAMA0"
LOGDIR="/var/tmp/co2monitor"
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
