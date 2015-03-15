Janosh
======

A json document database with a shell interface.

Janosh is written in C++11. It is used in the [ScreenInvader](https://github.com/Metalab/ScreenInvader) project.

## Build

### Debian

Install required packages:
<pre>
  apt-get install build-essential g++4.8 libboost-dev libboost-filesystem-dev libboost-system-dev \
  libboost-thread-dev libkyotocabinet-dev libluajit-5.1-dev
</pre>

Build:
<pre>
  git clone git://github.com/kallaballa/Janosh.git
  cd Janosh
  make
</pre>

Copy janosh binary and default configuration:
<pre>
  sudo make install
  cp -r .janosh ~/
</pre>

Please have a look at the [documentation](https://github.com/kallaballa/Janosh/wiki/Home)!
