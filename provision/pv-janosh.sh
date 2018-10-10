#!/bin/bash

CORES="$1"
SID="$2"
REPLHOST="$3"

apt-get install vim build-essential g++ libboost-dev libboost-filesystem-dev libboost-system-dev libboost-thread-dev libluajit-5.1-dev cmake libzmq3-dev git-core libcrypto++-dev libboost-program-options-dev luarocks zlib1g-dev sudo libssl1.0-dev psmisc libboost-iostreams-dev cgdb

git clone https://github.com/kallaballa/kyotocabinet.git
cd kyotocabinet
./configure
make -j$CORES
make install
cd ..

git clone https://github.com/kallaballa/kyototycoon.git 
cd kyototycoon
./configure --disable-lua
make -j$CORES
make install
cd ..

wget https://github.com/LuaLanes/lanes/archive/v3.10.1.tar.gz
tar -xf v3.10.1.tar.gz
rm v3.10.1.tar.gz
cd lanes-3.10.1
mkdir build
cd build
cmake ..
make -j$CORES
make install
mkdir -p /usr/local/lib/lua/5.1/lanes/
cp ../build/core.so /usr/local/lib/lua/5.1/lanes/core.so
cp ../src/lanes.lua /usr/local/share/lua/5.1/lanes.lua
cd ../..

wget https://github.com/uNetworking/uWebSockets/archive/v0.14.8.tar.gz
tar -xf v0.14.8.tar.gz
rm v0.14.8.tar.gz
cd uWebSockets-0.14.8/
make -j$CORES
make install
cd ..

wget https://github.com/kallaballa/luajit-rocks/archive/master.zip
unzip master.zip
rm master.zip
cd luajit-rocks-master/luajit-2.0
mkdir build
cd build
make clean
cmake ..
make -j$CORES
make install
cd ../../..

wget https://github.com/kallaballa/libsocket/archive/master.zip
unzip master.zip
rm master.zip
cd libsocket-master
mkdir build
cd build
make clean
cmake ..
make -j$CORES
make install
cd ../..

luarocks install lua-resty-template
luarocks install luaposix
luarocks install bitlib

git clone git://github.com/kallaballa/Janosh.git
cd Janosh
make -j$CORES
make install
cd ..

mkdir ~janosh/.janosh/
chown janosh:users ~janosh/.janosh/
cat > ~janosh/.janosh/janosh.json <<EOJANOSH
{
  "maxThreads": "$CORES",
  "dbstring": "janosh.kct#opts=c#pccap=256m#dfunit=8",
  "bindUrl": "/tmp/janosh",  
  "connectUrl": "/tmp/janosh",
  "ktopts": "-otl -li -pid kyoto.pid -log ktserver.log -sid $SID -port 10001 -ulog ${SID}-ulog -mhost $REPLHOST -mport 10001 -rts ${SID}.rts"

}
EOJANOSH
chown janosh:users ~janosh/.janosh/janosh.json

cat > /etc/systemd/system/janoshd.service <<EOJANOSHD
[Unit]
Description=Janosh Daemon

[Service]
WorkingDirectory=/home/janosh
ExecStart=/usr/local/bin/janosh -dv
User=janosh
Group=users

[Install]
WantedBy=multi-user.target

EOJANOSHD
chmod 664 /etc/systemd/system/janoshd.service
systemctl daemon-reload
systemctl enable janoshd


git clone https://github.com/kallaballa/LiebtDichJanosh.git
mv LiebtDichJanosh /home/janosh/LiebtDichJanosh

cat > /etc/systemd/system/janosh-websocket.service <<EOJANOSHD
[Unit]
Description=Janosh Websocket Client

[Service]
WorkingDirectory=/home/janosh/LiebtDichJanosh/
ExecStart=/usr/local/bin/janosh -v -f websocket.lua
User=janosh
Group=users

[Install]
WantedBy=multi-user.target

EOJANOSHD
chmod 664 /etc/systemd/system/janosh-websocket.service
systemctl daemon-reload
systemctl enable janosh-websocket

sudo -u janosh janosh load init.json


