Janosh
======

A small command line tool for quering/manipulating json files

Janosh is written in c++ with json_spirit as its only build dependency. It is used in the [ScreenInvader](https://github.com/Metalab/ScreenInvader) project which needed a tool for manipulating json files in bash script without dragging in more library dependencies.

## Usage

janosh [-s &lt;value&gt;]  &lt;json-file&gt; &lt;path&gt;

&lt;json-file&gt;    the json file to query/manipulate
&lt;path&gt;         the json path (uses / as separator)

Options:
-s &lt;value&gt;     instead of querieng for a path set its value

## Build

Install json_spirit (header only library).

Build:
&lt;pre&gt;
  cd Janosh
  make
&lt;/pre&gt;
