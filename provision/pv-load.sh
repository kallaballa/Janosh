#!/bin/bash

set -x
set -e 

apt-get install apache2
a2enmod proxy_wstunnel
a2enmod proxy_balancer
a2enmod lbmethod_byrequests

cat >> /etc/apache2/apache2.conf <<EOAPACHE
<Proxy balancer://mycluster>
    BalancerMember ws://server001:10010
    BalancerMember ws://server002:10010
</Proxy>

ProxyPass /ws balancer://mycluster/
EOAPACHE

cat > /etc/apache2/mods-available/mpm_event.conf <<EOMPMEVENT
# event MPM
# StartServers: initial number of server processes to start
# MinSpareThreads: minimum number of worker threads which are kept spare
# MaxSpareThreads: maximum number of worker threads which are kept spare
# ThreadsPerChild: constant number of worker threads in each server process
# MaxRequestWorkers: maximum number of worker threads
# MaxConnectionsPerChild: maximum number of requests a server process serves

# vim: syntax=apache ts=4 sw=4 sts=4 sr noet
<IfModule mpm_event_module>
StartServers 2
ServerLimit 10
MinSpareThreads 500
MaxSpareThreads 1000
MaxRequestWorkers 4000
ThreadsPerChild 500
ThreadLimit 4000
MaxConnectionsPerChild 0
MaxKeepAliveRequests 4000
KeepAlive On
KeepAliveTimeout 10
</IfModule>
EOMPMEVENT

systemctl stop apache2
systemctl start apache2
