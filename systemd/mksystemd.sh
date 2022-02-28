#!/bin/bash

PROGNAME=$(basename $0)

WATCHDOG_TIMEOUT=90
WATCHDOG_KICK_PERIOD=$(expr ${WATCHDOG_TIMEOUT} / 2)

# SDL settings
DefaultFbDevice=/dev/fb0
DefaultInputDevice=""
BIN=co2Monitor
InstallDir=/usr/local/bin
ResourceDir=${InstallDir}/${BIN}.d
SDL_TTF_DIR=${ResourceDir}/ttf
SDL_BMP_DIR=${ResourceDir}/bmp

SYSTEMD_DIR=/etc/systemd
MON_SERVICE_SYS_FILE="${SYSTEMD_DIR}/system/monitor@.service"
MON_SERVICE_CONF_DIR="${SYSTEMD_DIR}/monitor.service.d"
MON_SERVICE_CONF_FILE="${MON_SERVICE_CONF_DIR}/local-co2.conf"

PERSISTENT_STORE_DIR="/var/tmp/${BIN}"
PERSISTENT_STORE_FILE="${PERSISTENT_STORE_DIR}/state_info"
PERSISTENT_STORE_CONF_FILE="${PERSISTENT_STORE_DIR}/state.cfg"

CO2MON_LOG_DIR="/var/log/${BIN}"

SERIAL_PORT="/dev/ttyAMA0"

SENSOR_TYPE=
SENSOR_PORT=
LOGLEVEL=
UNINSTALL=0

ERROR=0

usage()
{
  printf "Usage: ${PROGNAME} -l | --loglevel  LOGLEVEL   -s | --sensor  SENSOR 
       ${PROGNAME} -u | --uninstall
  where:
      LOGLEVEL is one of DEBUG, INFO, NOTICE, WARNING, ERR, CRIT, ALERT.
      SENSOR is one of K30, SCD30, sim\n" 1>&2
  exit 2
}

if [[ $(id -u) -ne 0 ]]; then
    printf "This script must be run by root (su)\n" 1>&2
    exit 1
fi

PARSED_ARGUMENTS=$(getopt -n ${PROGNAME} -o l:s:u --longoptions loglevel:,sensor:,uninstall -- "$@")
[[ $? == 0 ]] || usage

eval set -- "$PARSED_ARGUMENTS"
while :
do
    case "$1" in
        -l | --loglevel)
            LOGLEVEL=$(echo $2 | tr "[:lower:]" "[:upper:]")
            case ${LOGLEVEL} in
                DEBUG | INFO | NOTICE | WARNING | ERR | CRIT | ALERT)
                    ;;
                *)
                    printf "Unknown syslog level:  \"$2\"\n" 1>&2
                    usage
            esac
            shift 2
            ;;
        -s | --sensor)
            SENSOR_ARG=$(echo $2 | tr "[:lower:]" "[:upper:]")
            case ${SENSOR_ARG} in
                K30)
                    SENSOR_TYPE=K30
                    SENSOR_PORT=${SERIAL_PORT}
                    ;;
                SCD30)
                    SENSOR_TYPE=SCD30
                    SENSOR_PORT="I2C"
                    ;;
                SIM | NONE)
                    SENSOR_TYPE=sim
                    SENSOR_PORT=
                    ;;
                *)
                    printf "Unknown sensor type: \"$2\"\n" 1>&2
                    usage
                    ;;
            esac
            shift 2
            ;;
        -u | --uninstall)
            UNINSTALL=1
            shift
            break
            ;;
        # -- means the end of the arguments; drop this, and break out of the while loop
        --)
            shift
            break
            ;;
        # If invalid options were passed, then getopt should have reported an error,
        # which we checked as VALID_ARGUMENTS when getopt was called...
        *)
            printf "Unexpected option: $1 - this should not happen.\n" 1>&2
            usage
            ;;
    esac
done

if [[ ${UNINSTALL:-0} == "1" ]]; then
    systemctl stop httpd.service
    systemctl disable httpd.service
    if [[ -f ${MON_SERVICE_SYS_FILE} ]]; then
        systemctl stop monitor@co2.service
        systemctl disable monitor@co2.service
        rm -f ${MON_SERVICE_SYS_FILE} 2>/dev/null
    fi
    [[ -f ${MON_SERVICE_CONF_FILE} ]] && rm -f ${MON_SERVICE_CONF_FILE} 2>/dev/null
    [[ -e ${PERSISTENT_STORE_DIR} ]] && rm -fr ${PERSISTENT_STORE_DIR} 2>/dev/null
    exit
fi

# We need SENSOR and SYS LOG LEVEL
[[ ${SENSOR_TYPE} == "" || ${LOGLEVEL} == "" ]] && usage


# Check for touchscreen device for resistive/capacitive TFT
grep -qe "^dtoverlay=.*-resistive" /boot/config.txt 2>/dev/null && TFT="R"
grep -qe "^dtoverlay=.*-capacitive" /boot/config.txt 2>/dev/null && TFT="C"
if [ ${TFT} == "R" ]; then

    DefaultInputDevice=/dev/input/touchscreen
    if [[ ! -e ${DefaultInputDevice} ]]; then
        printf "Error: touchscreen device \"%s\" not found\n" ${DefaultInputDevice} 1>&2
        ERROR=1
    fi

elif [ ${TFT} == "C" ]; then

    DefaultInputDevice=/dev/input/touchscreen
    if [[ ! -e ${DefaultInputDevice} ]]; then
        printf "Warning: touchscreen device \"%s\" not found\n" ${DefaultInputDevice} 1>&2
        DefaultInputDevice=$(ls /dev/input/by-path/*.i2c-event 2>/dev/null)
        if [[ ! -e ${DefaultInputDevice} ]]; then
            printf "Error: touchscreen device \"%s\" not found\n" ${DefaultInputDevice} 1>&2
            ERROR=1
        fi
    fi

else
    printf "Missing dtoverlay for TFT (resistive or capacitive)\n" 1>&2
    ERROR=1
fi

if [[ -L ${DefaultInputDevice} ]]; then
    RealInputDevice=$(readlink -f ${DefaultInputDevice})
    if [[ ! -c ${RealInputDevice} ]]; then
        printf "Error: touchscreen device \"%s\" not a character device\n" ${RealInputDevice} 1>&2
        ERROR=1
    fi
elif [[ ! -c ${DefaultInputDevice} ]]; then
    printf "Error: touchscreen device \"%s\" not a character device\n" ${DefaultInputDevice} 1>&2
    ERROR=1
fi

printf "Input device is: ${DefaultInputDevice}\n"

if [[ -d ${InstallDir} ]]; then
    if [[ ! -x ${InstallDir}/${BIN} ]]; then
        printf "Missing executable: \"%s\"\n" ${InstallDir}/${BIN} 1>&2
        ERROR=1
    fi
else
    printf "Missing executable directory: \"%s\"\n" ${InstallDir} 1>&2
    ERROR=1
fi


if [[ -d ${SDL_TTF_DIR} ]]; then
    if [[ $(ls -1 ${SDL_TTF_DIR} | wc -l) -eq 0 ]]; then
        printf "Missing TTF files in: \"%s\"\n" ${SDL_TTF_DIR} 1>&2
        ERROR=1
    fi
else
    printf "Missing TTF directory: \"%s\"\n" ${SDL_TTF_DIR} 1>&2
    ERROR=1
fi

if [[ -d ${SDL_BMP_DIR} ]]; then
    if [[ $(ls -1 ${SDL_BMP_DIR} | wc -l) -eq 0 ]]; then
        printf "Missing bitmap files in: \"%s\"\n" ${SDL_BMP_DIR} 1>&2
        ERROR=1
    fi
else
    printf "Missing bitmap directory: \"%s\"\n" ${SDL_BMP_DIR} 1>&2
    ERROR=1
fi

if [[ ${ERROR} != 0 ]]; then
    printf "Exiting because of error(s)\n" 1>&2
    exit 1
fi

if [[ ! -f ${MON_SERVICE_SYS_FILE} ]]; then
cat <<xEOFx >${MON_SERVICE_SYS_FILE} 
[Unit]
Description=CO2, Temperature and Relative Humidity Monitor (%i)
After=network-online.target
Wants=network-online.target
Wants=fbcp.service

[Service]
Restart=always
RestartSec=30s
WatchdogSec=${WATCHDOG_TIMEOUT}s
NotifyAccess=main
EnvironmentFile=/etc/systemd/monitor.service.d/local-%i.conf
ExecStart=${InstallDir}/${BIN} -c /etc/systemd/monitor.service.d/local-%i.conf

[Install]
WantedBy=multi-user.target
xEOFx
fi

if [[ ! -d ${MON_SERVICE_CONF_DIR} ]]; then
mkdir -p ${MON_SERVICE_CONF_DIR}
fi

if [[ ! -f ${MON_SERVICE_CONF_FILE} ]]; then
cat <<xEOFx >${MON_SERVICE_CONF_FILE} 
# CO2 Sensor is defined here for co2Monitor to configure at runtime
SensorType="${SENSOR_TYPE}"
SensorPort="${SENSOR_PORT}"

# where we store states, info, etc. which must persist between app restarts and system reboots
PersistentStoreFileName="${PERSISTENT_STORE_FILE}"
PersistentStoreConfigFile="${PERSISTENT_STORE_CONF_FILE}"

# where we store sensor readings in timestamped CSV daily files: ${CO2MON_LOG_DIR}/YYYY/MM/DD
Co2LogBaseDir="${CO2MON_LOG_DIR}"

# Log level is one of DEBUG (verbose), INFO, NOTICE, WARNING, ERR, CRIT, ALERT (highest)
LogLevel=${LOGLEVEL}

# How often to test network connectivity (in seconds)
NetworkCheckPeriod=30

# How often to kick watchdog. should not be more than half WatchdogSec
WatchdogKickPeriod=${WATCHDOG_KICK_PERIOD}

NetDevice="wlan0"

# Minimum number of seconds network device should be down before
# reboot is initiated.
NetDeviceDownRebootMinTime=5

# Minimum number of seconds network connectivity should be down before
# reboot is initiated.
NetDownRebootMinTime=1800

# These will be used in preference to respective environment variables (if set).
SDL_FBDEV=${DefaultFbDevice}
SDL_MOUSEDEV=${DefaultInputDevice}
SDL_MOUSEDRV="TSLIB"
SDL_MOUSE_RELATIVE="0"

# root dir where screen fonts are stored
SDL_TTF_DIR=${SDL_TTF_DIR}

# root dir where screen bitmaps are stored
SDL_BMP_DIR=${SDL_BMP_DIR}

# Screen refresh rate in FPS
ScreenRefreshRate=10

# Screen saver kicks in after this many seconds of inactivity
ScreenTimeout=120

# Amount of time (minutes) fan stays on for manual override
FanOnOverrideTime=90

# Rel Humidity threshold (%) above which fan starts
RelHumFanOnThreshold=80

# CO2 threshold (ppm) above which fan starts
CO2FanOnThreshold=800


xEOFx
fi

systemctl enable monitor@co2.service
systemctl restart monitor@co2.service

APACHE_CONF_DIR=/etc/httpd/conf
if [[ ! -f ${APACHE_CONF_DIR}/apache.key ]]; then

echo "Setting up Apache + openssl..."
cd ${APACHE_CONF_DIR}
openssl req -new -x509 -nodes -newkey rsa:4096 -keyout apache.key -out apache.crt -days 1095 >/dev/null 2>&1 <<xEOFx
CA
Ontario
Nepean
Pat Wilson Software Design Inc.

Patrick Wilson
patw@patwsd.com
xEOFx

chmod 400 apache.key
chmod 444 apache.crt

fi

# uncomment these lines in /etc/httpd/conf/httpd.conf
HTTPD_CONF="/etc/httpd/conf/httpd.conf"
sed -i -e '/LoadModule ssl_module modules\/mod_ssl.so/s/^# *//' "${HTTPD_CONF}"
sed -i -e '/LoadModule socache_shmcb_module modules\/mod_socache_shmcb.so/s/^# *//' "${HTTPD_CONF}"
sed -i -e '/Include conf\/extra\/httpd-ssl.conf/s/^# *//' "${HTTPD_CONF}"

# Then edit /etc/httpd/conf/extra/httpd-ssl.conf to reflect the new key and certificate:
HTTP_SSL_CONF="/etc/httpd/conf/extra/httpd-ssl.conf"
grep -q "SSLCertificateFile ${APACHE_CONF_DIR}/apache.crt" "${HTTP_SSL_CONF}" >&/dev/null || \
printf "\n\nAdd the following lines to ${HTTP_SSL_CONF}:\n\nSSLCertificateFile ${APACHE_CONF_DIR}/apache.crt\nSSLCertificateKeyFile ${APACHE_CONF_DIR}/apache.key\n\n"


systemctl enable httpd.service
systemctl restart httpd.service
