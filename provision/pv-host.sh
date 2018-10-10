#/bin/bash

HOSTNAME="$1"

echo "$HOSTNAME" > /etc/hostname
hostname "$HOSTNAME"

cat >> /etc/security/limits.conf <<EOLIMITS
janosh      hard    nofile          1000000
janosh      soft    nofile          1000000
janosh      hard    nproc           10000
janosh      soft    nproc           10000
EOLIMITS

cat >> /etc/rc.local <<EORCLOCAL
#!/bin/bash
ulimit -u 10000
sudo sysctl net.ipv4.ip_local_port_range="10000 61000"
sudo sysctl net.ipv4.tcp_fin_timeout=10
sudo sysctl net.ipv4.tcp_tw_recycle=1
sudo sysctl net.ipv4.tcp_tw_reuse=1
EORCLOCAL

chmod a+x /etc/rc.local

pass="$(perl -e 'print crypt($ARGV[0], "password")' "janosh")"
useradd  -s /bin/bash -m -p $pass janosh
