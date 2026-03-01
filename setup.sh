#/bin/bash

set -e

addgroup --system mcsv-mgr
adduser --system --ingroup mcsv-mgr --no-create-home mcsv-mgr
adduser --system --ingroup mcsv-mgr --no-create-home mcsv

# Allow Apache (www-data) to write to the Unix Socket
usermod -aG mcsv-mgr www-data

mkdir -p /srv/minecraft/{instances,shared}
chown -R mcsv-mgr:mcsv-mgr /srv/minecraft
chmod -R 2775 /srv/minecraft

cat <<EOF > /etc/apache2/conf-available/main-site-parts/mcsv_manager.conf
<Location "/api/mcsv_manager/">
    ProxyPreserveHost On
    RewriteEngine On

    # --- WebSocket Support ---
    RewriteCond %{HTTP:Upgrade} websocket [NC]
    RewriteCond %{HTTP:Connection} upgrade [NC]
    RewriteRule ^(.*)$ unix:/run/mcsv_manager.sock|ws://a/$1 [P,L]

    RewriteCond %{LA-U:REMOTE_USER} (.+)
    RewriteRule . - [E=RU:%1]
    RequestHeader set X-Remote-User %{RU}e env=RU

    ProxyPass        unix:/run/mcsv_manager.sock|http://a/
    ProxyPassReverse http://a/
</Location>
EOF

cat <<EOF > /etc/systemd/system/mcsv_manager.socket
[Unit]
Description=Minecraft Server Manager API socket

[Socket]
ListenStream=/run/mcsv_manager.sock
SocketUser=mcsv-mgr
SocketGroup=mcsv-mgr
SocketMode=0660

[Install]
WantedBy=sockets.target
EOF

cat <<EOF > /etc/systemd/system/mcsv_manager.service
[Unit]
Description=Minecraft Server Manager daemon
Requires=mcsv_manager.socket
After=network.target

[Service]
User=mcsv-mgr
Group=mcsv-mgr

ExecStart=/usr/local/bin/mcsv_manager
Restart=on-failure

StandardOutput=journal
StandardError=journal

# socket activation
StandardInput=socket
NonBlocking=true

# hardening (safe defaults)
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ReadOnlyPaths=/usr/local
ProtectHome=true
ReadWritePaths=/run
# CapabilityBoundingSet=
# AmbientCapabilities=

[Install]
WantedBy=multi-user.target
EOF

cat <<EOF > /etc/tmpfiles.d/minecraft.conf
# Type Path                       Mode UID      GID       Age Argument
d    /run/minecraft               0755 mcsv-mgr mcsv-mgr  -
EOF

cat <<EOF > /etc/systemd/system/minecraft@.socket
[Unit]
Description=Minecraft stdin socket for %i

[Socket]
ListenFIFO=/run/minecraft/%i.stdin
SocketUser=mcsv
SocketGroup=mcsv-mgr
SocketMode=0660

[Install]
WantedBy=sockets.target
EOF

cat <<EOF > /etc/polkit-1/rules.d/10-mcsv-manager.rules
polkit.addRule(function(action, subject) {
    if (action.id == "org.freedesktop.systemd1.manage-units" &&
        subject.user == "mcsv-mgr") {
        return polkit.Result.YES;
    }
});
EOF