#/bin/bash

HOSTNAME="$1"
if [ -z "$HOSTNAME" ]; then
  echo "Usage: pv-host.sh <HOSTNAME>"
  exit 1
fi

apt-get -y install nmon cgdb sudo git-core vim psmisc net-tools

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

cat > /etc/default/grub <<EOGRUB
# If you change this file, run 'update-grub' afterwards to update
# /boot/grub/grub.cfg.
# For full documentation of the options in this file, see:
#   info -f grub -n 'Simple configuration'

GRUB_DEFAULT=0
GRUB_TIMEOUT=2
GRUB_DISTRIBUTOR=\`lsb_release -i -s 2> /dev/null || echo Debian\`
GRUB_CMDLINE_LINUX_DEFAULT="quiet net.ifnames=0 biosdevname=0"
GRUB_CMDLINE_LINUX=""

# Uncomment to enable BadRAM filtering, modify to suit your needs
# This works with Linux (no patch required) and with any kernel that obtains
# the memory map information from GRUB (GNU Mach, kernel of FreeBSD ...)
#GRUB_BADRAM="0x01234567,0xfefefefe,0x89abcdef,0xefefefef"

# Uncomment to disable graphical terminal (grub-pc only)
#GRUB_TERMINAL=console

# The resolution used on graphical terminal
# note that you can use only modes which your graphic card supports via VBE
# you can see them in real GRUB with the command 
#GRUB_GFXMODE=640x480

#GRUB_DISABLE_LINUX_UUID=true

# Uncomment to disable generation of recovery mode menu entries
#GRUB_DISABLE_RECOVERY="true"

# Uncomment to get a beep at grub start
#GRUB_INIT_TUNE="480 440 1"
EOGRUB

update-grub

