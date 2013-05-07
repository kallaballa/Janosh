Janosh
======

A json document database with a shell interface.

Janosh is written in C++11. It is used in the [ScreenInvader](https://github.com/Metalab/ScreenInvader) project.

## Build

### Debian

Install required packages:
<pre>
  apt-get install build-essential g++4.7 libboost-dev libboost-filesystem-dev libboost-system-dev \
  libboost-thread-dev libkyotocabinet-dev
</pre>

Build:
<pre>
  git clone https://github.com/kallaballa/Janosh.git
  cd Janosh
  make
</pre>

Copy janosh binary and default configuration:
<pre>
  cp janosh /usr/bin/
  cp -r .janosh ~/
</pre>

## Example Session

First you need to edit the default configuration. For now we'll leave triggers.json as it is and only edit janosh.json.
Choose where you want to store your database file. In the example the db file is stored in the .janosh directory.

<pre>
 {
    "database" : "/home/user/.janosh/janosh.db",   
    "triggerDirectories": [ "/home/user/.janosh/triggers" ] 
 }
</pre>

