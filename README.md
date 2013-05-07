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

Initialize the database:

<pre>
$ janosh truncate
</pre>

Make an array:

<pre>
$ janosh mkarr /array/.
</pre>

Append same values to the arry

<pre>
$ janosh append /array/. a b c d
</pre>

Print document root recursive as json document:
<pre>
$ janosh -j get /.
{ 
"array": [ 
"a",
"b",
"c",
"d" ] 
 } 
</pre>

Print document root recursive as bash associative array:
<pre>
$ janosh -b get /.
( [/array/#0]='a' [/array/#1]='b' [/array/#2]='c' [/array/#3]='d' )
</pre>

Dump document in raw format:
<pre>
$ janosh dump
path: /. value:O1
path: /array/. value:A4
path: /array/#0 value:a
path: /array/#1 value:b
path: /array/#2 value:c
path: /array/#3 value:d
</pre>

Shift value of index 3 to index 1:
<pre>
$ janosh shift /array/#3 /array/#1
$ janosh -j get /.
{ 
"array": [ 
"a",
"d",
"b",
"c" ] 
 } 
</pre>

Remove index 3:
<pre>
$ janosh remove /array/#3
$ janosh -j get /.
{ 
"array": [ 
"a",
"d",
"b" ] 
 } 
</pre>

Get array size:
<pre>
$ janosh size /array/.
3
</pre>

Create an object:
<pre>
$ janosh mkobj /object/.
$ janosh -j get /.
{ 
"array": [ 
"a",
"d",
"b" ] 
,
"object": { 
 } 
 }
</pre>

Move the array into the object:
<pre>
$ janosh move /array/. /object/array/.
{ 
"object": { 
"array": [ 
"a",
"d",
"b" ] 
 } 
 } 
</pre>

Write document into a file, truncate the database and load the file:
<pre>
$ janosh -j get /. > db.json
$ janosh truncate
$ janosh load db.json
$ janosh -j get /.
{
"object": {
"array": [
"a",
"d",
"b" ]
 }
 }
</pre>

