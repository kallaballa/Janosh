![ducktiger](http://asset-2.soup.io/asset/11244/5869_2544_500.png)
Janosh
======

A json document database with a shell interface and lua scripting support.

Janosh is written in C++11. It is used in the [ScreenInvader](https://github.com/Metalab/ScreenInvader) project.

## Build

### Debian

Install required packages:
<pre>
  apt-get install build-essential g++4.8 libboost-dev libboost-filesystem-dev \
  libboost-system-dev libboost-thread-dev libkyotocabinet-dev libluajit-5.1-dev \
  cmake libzmq3-dev
</pre>

Download source:
<pre>
  git clone git://github.com/kallaballa/Janosh.git
</pre>

Build dependencies:
<pre>
  cd Janosh
 ./build_dependencies.sh
</pre>

Build Janosh:
<pre>
  make
</pre>

Copy janosh binary and default configuration:
<pre>
  sudo make install
  cp -r .janosh ~/
</pre>

##N ote

Setting /proc/sys/net/ipv4/tcp_tw_recycle and /proc/sys/net/ipv4/tcp_tw_reuse to 1 is recommended.


Please have a look at the [documentation](https://github.com/kallaballa/Janosh/wiki/Home)!
