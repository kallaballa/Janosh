Janosh
======

A small command line tool for manipulation of json files

Janosh is written in cpp and only uses json_spirit as it's only build dependency. It is used in the [ScreenInvader](https://github.com/Metalab/ScreenInvader) project which needed a tool for manipulating json files in bash script without dragging in more library dependencies.

## Usage

janosh [-s <value>]  <json-file> <path>

<json-file>    the json file to query/manipulate
<path>         the json path (uses / as separator)

Options:
-s <value>     instead of querieng for a path set it's value

## Build

Install json_spirit (header only library).

Build:
<pre>
  cd Janosh
  make
</pre>
