#!/bin/sh
# Sets up the local broker in an Alpine WSL distro: Mosquitto with TLS for the boards on the
# LAN, WebSockets on localhost for an onion service, and Tor publishing that onion service.
# Run as root inside the distro (see the README's "Local broker in WSL"):
#   BROKER_ADDRESS=<this PC's LAN IP> BROKER_NAME=<this PC's name> #   CREDENTIALS=/mnt/c/Users/<you>/.ssh/<credentials file> sh setup.sh
# The credentials file holds lines like MQTT_DEVICE_USER=..., MQTT_DEVICE_PASSWORD=...,
# and the same for MQTT_ADMIN, DASHBOARD and OPERATOR. Prints no passwords.
set -eu
: "${BROKER_ADDRESS:?set BROKER_ADDRESS to this PC's LAN address}"
: "${BROKER_NAME:?set BROKER_NAME to this PC's name}"
: "${CREDENTIALS:?set CREDENTIALS to the credentials file}"
apk add -q mosquitto mosquitto-clients tor openssl

# --- Certificates: a CA kept in /root/ca, and a server certificate for this PC.
mkdir -p /root/ca /etc/mosquitto/certs
chmod 700 /root/ca
cd /root/ca
if [ ! -f ca.key ]; then
    openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -days 3650 \
        -keyout ca.key -out ca.crt -subj "/CN=Clanker Cleaner local CA" 2>/dev/null
fi
cat > server.ext <<EOF
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyAgreement
extendedKeyUsage = serverAuth
subjectAltName = IP:$BROKER_ADDRESS, DNS:$BROKER_NAME, DNS:localhost, IP:127.0.0.1
EOF
openssl req -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes \
    -keyout server.key -out server.csr -subj "/CN=$BROKER_NAME" 2>/dev/null
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -days 1825 \
    -extfile server.ext -out server.crt 2>/dev/null
cp server.crt server.key ca.crt /etc/mosquitto/certs/
chown -R mosquitto:mosquitto /etc/mosquitto/certs
chmod 600 /etc/mosquitto/certs/server.key

# --- Logins, from the credentials file.
creds=$CREDENTIALS
value() { sed -n "s/^$1=//p" "$creds" | tr -d '\r' | sed "s/^[\"']//; s/[\"']\$//"; }
: > /etc/mosquitto/passwd
for prefix in MQTT_DEVICE MQTT_ADMIN DASHBOARD OPERATOR; do
    mosquitto_passwd -b /etc/mosquitto/passwd "$(value ${prefix}_USER)" "$(value ${prefix}_PASSWORD)"
done
chown mosquitto:mosquitto /etc/mosquitto/passwd
chmod 600 /etc/mosquitto/passwd

cat > /etc/mosquitto/acl <<EOF
# The boards publish and subscribe under their prefix.
user $(value MQTT_DEVICE_USER)
topic readwrite clanker-cleaner/#

# Admin: everything.
user $(value MQTT_ADMIN_USER)
topic readwrite #

# The dashboard only watches.
user $(value DASHBOARD_USER)
topic read clanker-cleaner/#

# The operator watches and sends commands, to the main board or a test board.
user $(value OPERATOR_USER)
topic read clanker-cleaner/#
topic write clanker-cleaner/commands/#
topic write clanker-cleaner/+/commands/#
EOF
chown mosquitto:mosquitto /etc/mosquitto/acl
chmod 600 /etc/mosquitto/acl

cat > /etc/mosquitto/mosquitto.conf <<'EOF'
# Clanker Cleaner broker.
allow_anonymous false
password_file /etc/mosquitto/passwd
acl_file /etc/mosquitto/acl
persistence true
persistence_location /var/lib/mosquitto/
log_dest file /var/log/mosquitto/mosquitto.log
log_type error
log_type warning
log_type notice
connection_messages true

# The boards, on the LAN, over TLS.
listener 8883 0.0.0.0
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key

# Local tests from inside WSL.
listener 1883 127.0.0.1

# The onion service forwards here; nothing else can reach it.
listener 9001 127.0.0.1
protocol websockets
EOF
mkdir -p /var/lib/mosquitto /var/log/mosquitto
chown mosquitto:mosquitto /var/lib/mosquitto /var/log/mosquitto

# --- Tor: one onion service, port 80 to the local WebSocket listener.
cat > /etc/tor/torrc <<'EOF'
User tor
DataDirectory /var/lib/tor
Log notice file /var/log/tor/notices.log
# Local SOCKS port for testing the onion address from inside WSL only.
SocksPort 127.0.0.1:9050
HiddenServiceDir /var/lib/tor/clanker/
HiddenServicePort 80 127.0.0.1:9001
EOF
mkdir -p /var/lib/tor /var/log/tor
chown -R tor:tor /var/lib/tor /var/log/tor
chmod 700 /var/lib/tor

# --- Start script: starts Tor and Mosquitto, and with "run", restarts either if it stops.
cat > /usr/local/bin/clanker-broker <<'EOF'
#!/bin/sh
# Starts and watches the Clanker Cleaner broker: Tor (the onion service) and Mosquitto.
#   clanker-broker start   start whichever isn't running, then return. /etc/wsl.conf runs
#                          this whenever the distro starts.
#   clanker-broker run     the same, then keep checking every 15 s and restart either one if
#                          it stops. Runs until killed, which also keeps the distro up; the
#                          Windows task runs this.
#   clanker-broker status  show whether each is running.
# Log: /var/log/clanker-broker.log

Log=/var/log/clanker-broker.log

note() {
    echo "$(date -u '+%Y-%m-%d %H:%M:%S') $*" >> "$Log"
}

running() {
    pgrep -x "$1" >/dev/null
}

start_tor() {
    if ! running tor; then
        if tor --runasdaemon 1 -f /etc/tor/torrc >/dev/null 2>&1; then
            note "started tor"
        else
            note "tor failed to start; see /var/log/tor/notices.log"
        fi
    fi
}

start_mosquitto() {
    if ! running mosquitto; then
        if mosquitto -d -c /etc/mosquitto/mosquitto.conf; then
            note "started mosquitto"
        else
            note "mosquitto failed to start; see /var/log/mosquitto/mosquitto.log"
        fi
    fi
}

start_all() {
    start_tor
    start_mosquitto
}

case "${1:-run}" in
    start)
        start_all
        ;;
    run)
        note "supervisor started"
        trap 'note "supervisor stopped"; exit 0' INT TERM
        start_all
        while :; do
            sleep 15
            running tor || { note "tor not running"; start_tor; }
            running mosquitto || { note "mosquitto not running"; start_mosquitto; }
        done
        ;;
    status)
        for name in tor mosquitto; do
            if running "$name"; then echo "$name: running"; else echo "$name: stopped"; fi
        done
        ;;
    *)
        echo "usage: clanker-broker [start|run|status]" >&2
        exit 2
        ;;
esac
EOF
chmod 755 /usr/local/bin/clanker-broker

# --- Start both whenever the distro starts.
cat > /etc/wsl.conf <<'EOF'
# ClankerBroker distro settings.
[boot]
# Start Tor and Mosquitto whenever the distro starts, however it's started.
command = /usr/local/bin/clanker-broker start

[user]
default = root
EOF

mosquitto -c /etc/mosquitto/mosquitto.conf --test-config && echo "mosquitto config ok"
tor --verify-config -f /etc/tor/torrc | tail -1
openssl x509 -in /etc/mosquitto/certs/server.crt -noout -subject -ext subjectAltName -enddate
echo "logins: $(cut -d: -f1 /etc/mosquitto/passwd | tr '\n' ' ')"
echo "onion address, once Tor has started: /var/lib/tor/clanker/hostname"
