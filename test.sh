#!/bin/bash

. ./.functions.sh

HASH=
DEBUG=

function prepare() {
  [ -z "$HASH" ] && echo
  janosh truncate
}

function finalize() {
  [ -n "$VERBOSE" ] && janosh dump
  echo -n "$1:"
  janosh hash
}

function test_truncate() {
  true
}

function test_mkarray() { 
  janosh mkarr /array/.        || return 1
  janosh mkarr /array/.        && return 1 
  [  `janosh size /.` -eq  1 ] || return 1
}

function test_mkobject() {
  janosh mkobj /object/.       || return 1
  janosh mkobj /object/.       && return 1
  [  `janosh size /.` -eq  1 ] || return 1
}

function test_append() {
  janosh mkarr /array/.             || return 1
  janosh append /array/. 0 1 2 3	  || return 1
  [ `janosh size /array/.` -eq 4 ]	|| return 1
  janosh mkobj /object/.		        || return 1
  janosh append /object/. 0 1 2 3 	&& return 1
  [ `janosh size /object/.` -eq 0 ]  || return 1  
}

function test_set() {
  janosh mkarr /array/. 		|| return 1
  janosh append /array/. 0 		|| return 1
  janosh set /array/0 1 		|| return 1
  janosh set /array/6 0			&& return 1
  [ `janosh size /array/.` -eq 1 ] 	|| return 1
  janosh mkobj /object/. 	 	|| return 1
  janosh set /object/bla 1		|| return 1
  [ `janosh size /object/.` -eq 1 ] 	|| return 1
}

function test_add() {
  janosh mkarr /array/.             || return 1
  janosh add /array/0 1             || return 1
  janosh add /array/0 0             && return 1
  janosh add /array/5 0							&& return 1
  [ `janosh size /array/.` -eq 1 ]  || return 1
  janosh mkobj /object/.						|| return 1
  janosh add /object/0 1						|| return 1
  janosh add /object/0 0						&& return 1
  janosh add /object/5 0						|| return 1
  [ `janosh size /object/.` -eq 2 ]	|| return 1
}

function test_replace() {
  janosh mkarr /array/. 		|| return 1
  janosh append /array/. 0 		|| return 1
  janosh replace /array/0 2 		|| return 1
  janosh replace /array/6 0		&& return 1
  [ `janosh size /array/.` -eq 1 ] 	|| return 1
  janosh mkobj /object/. 	 	|| return 1
  janosh set /object/bla 1		|| return 1
  janosh replace /object/bla 2		|| return 1
  janosh replace /object/blu 0		&& return 1
  [ `janosh size /object/.` -eq 1 ] 	|| return 1
}

function test_shift() {
  janosh mkarr /array/.               || return 1
  janosh append /array/. 0 1 2 3      || return 1
  janosh shift /array/0 /array/3      || return 1
  [ `janosh size /.` -eq 1 ]    || return 1
  [ `janosh size /array/.` -eq 4 ]    || return 1
  [ `janosh -r get /array/0` -eq 1 ]  || return 1
  [ `janosh -r get /array/3` -eq 0 ]  || return 1
  janosh mkarr /array/4/.             || return 1
  janosh append /array/4/. 3 2 1 0    || return 1
  janosh shift /array/4/3 /array/4/0  && return 1
  #[ `janosh -r get /sub/1` -eq 2  ]  || return 1  
}


function run() {
  ( 
    prepare
    [ -n "$DEBUG" ] && set -x
    [ -n "$DEBUG" ] && test_$1 || (test_$1 &>/dev/null)
    err=$?
    [ -n "$DEBUG" ] && set +x
    [ -z "$HASH" ] && check "$1" "[ $err -eq 0 ]" || (check "$1" "[ $err -eq 0 ]" &> /dev/null)
  ) 2>&1 | fgrep -v "INFO:"

  finalize $1 2>&1 | fgrep -v "INFO:"
}


while getopts 'dhv' c
do
  case $c in
    d) DEBUG=YES;;
    v) VERBOSE=YES;;
    h) HASH=YES;;
    \?) echo "Unkown switch"; exit 1;;
  esac
done

shift

if [ -z "$1" ]; then
  run truncate
  run mkarray
  run mkobject
  run append
  run set
  run add
  run replace
  run shift
else
  run $1
fi


