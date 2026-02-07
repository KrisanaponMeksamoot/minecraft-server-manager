#/bin/bash

addgroup --system mcsv-mgr
adduser --system --ingroup mcsv-mgr mcsv-mgr
usermod -aG mcsv-mgr www-data
adduser --system --ingroup mcsv-mgr mcsv

cat <<EOF > /etc/apache2/sites-available/mcsv_manager.conf
<VirtualHost *:80>
    ServerName _
    ServerAlias *

    ProxyPreserveHost On

    ProxyPass        /api/mcsv_manager/ unix:/run/mcsv_manager.sock|http://localhost/
    ProxyPassReverse /api/mcsv_manager/ http://localhost/
</VirtualHost>
EOF

cat <<EOF > /etc/systemd/system/mcsv_manager.socket
[Unit]
Description=mcsv_manager API socket

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
Description=mcsv_manager daemon
Requires=mcsv_manager.socket
After=network.target

[Service]
User=mcsv-mgr
Group=mcsv-mgr

ExecStart=/usr/local/bin/minecraft_server_manager
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