#!/bin/bash

set -x
set -e

CORES="$1"
if [ -z "$CORES" ]; then
  echo "Usage: pv-janosh.sh <NR_OF_CORES>"
  exit 1
fi

apt-get -y install dh-autoreconf libssl-dev

git clone https://github.com/kallaballa/dynomite.git
cd dynomite
autoreconf -fvi
./configure
make -j$CORES
make install
cd ..


mkdir -p /etc/dynomite/
cat > /etc/dynomite/dynomite.conf <<EODYNOCONF
dyn_o_mite:
  dyn_listen: 0.0.0.0:8101
  data_store: 2
  listen: 0.0.0.0:8102
  dyn_seed_provider: simple_provider
  servers:
  - 127.0.0.1:1978:1
  tokens: 437425602
  stats_listen: 0.0.0.0:22333
EODYNOCONF

cat > /etc/systemd/system/dynomite.service <<EODYNOS
[Unit]
Description=Dynomite Daemon

[Service]
WorkingDirectory=/home/janosh
ExecStart=/usr/local/sbin/dynomite -c /etc/dynomite/dynomite.conf
User=janosh
Group=janosh

[Install]
WantedBy=multi-user.target

EODYNOS

chmod 664 /etc/systemd/system/dynomite.service
systemctl daemon-reload
systemctl enable dynomite
systemctl start dynomite

