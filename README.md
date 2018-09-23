Janosh
======

![ducktiger](/logo.png?raw=true "logo")

A json document database with a shell interface and lua scripting support.

Janosh is written in C++11. It is used in the [ScreenInvader](https://github.com/Metalab/ScreenInvader) project.

## Build

### Debian

Install required packages:

    apt-get install build-essential g++ libboost-dev libboost-filesystem-dev \
    libboost-system-dev libboost-thread-dev libkyotocabinet-dev libluajit-5.1-dev \
    cmake libzmq3-dev git-core libcrypto++-dev libboost-program-options-dev \
    luarocks

Download source:

    git clone git://github.com/kallaballa/Janosh.git

Build dependencies:

    cd Janosh
    ./build_dependencies.sh

Install luarocks:

    sudo luarocks install lua-resty-template
    sudo luarocks install luaposix
    sudo luarocks install bitlib
    sudo luarocks install lanes

Build Janosh:

    make

Copy janosh binary and default configuration:

    sudo make install
    cp -r .janosh ~/

## Note

Setting /proc/sys/net/ipv4/tcp_tw_reuse to 1 is recommended if you are generating a lot of connections.


Please have a look at the [documentation](https://github.com/kallaballa/Janosh/wiki/Home)!
