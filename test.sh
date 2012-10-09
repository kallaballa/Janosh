#!/bin/bash

. ./.functions.sh

[ "$1" == "gen" ] && GENERATE=yes
[ "$1" == "dbg" ] && DEBUG=yes

function prepare() {
  [ -z "$GENERATE" ] && echo
  janosh truncate
}

function finalize() {
  [ -z "$GENERATE" ] && janosh dump
  echo -n "$1:"
  janosh hash
}

function test_truncate() {
  true
}

function test_mkarray() { 
  janosh mkarr /array/. || return 1
  janosh mkarr /array/. && return 1 || return 0
}

function test_mkobject() {
  janosh mkobj /object/. || return 1
  janosh mkobj /object/. && return 1 || return 0
}

function test_append() {
  janosh mkarr /array/.			|| return 1
  janosh append /array/. 0 1 2 3	|| return 1
  [ `janosh size /array/.` -eq 4 ]	|| return 1
  janosh mkobj /object/.		|| return 1
  janosh append /object/. 0 1 2 3 	&& return 1 || return 0
}

function test_set() {
  janosh mkarr /array/. 		|| return 1
  janosh append /array/. 0 		|| return 1
  janosh set /array/0 1 		|| return 1
  janosh set /array/6 0			&& return 1
  [ `janosh size /array/.` -eq 1 ] 	|| return 1
  janosh mkobj /object/. 	 	|| return 1
  janosh add /object/bla 1		|| return 1
  janosh set /object/bla 2		|| return 1
}

function test_add() {
  janosh mkarr /array/. 		|| return 1
  janosh add /array/0 1 		|| return 1
  janosh add /array/0 0			&& return 1
  janosh add /array/5 0			&& return 1
  [ `janosh size /array/.` -eq 1 ] 	|| return 1
  janosh mkobj /object/. 		|| return 1
  janosh add /object/0 1 		|| return 1
  janosh add /object/0 0		&& return 1
  janosh add /object/5 0		|| return 1
  [ `janosh size /object/.` -eq 2 ] 	|| return 1
}

function run() {
  ( 
    prepare
    [ -n "$DEBUG" ] && set -x
    [ -n "$DEBUG" ] && test_$1 || (test_$1 &>/dev/null)
    err=$?
    [ -n "$DEBUG" ] && set +x
    [ -z "$GENERATE" ] && check "$1" "[ $err -eq 0 ]" || (check "$1" "[ $err -eq 0 ]" &> /dev/null)
  ) 2>&1 | fgrep -v "INFO:"

  finalize $1 2>&1 | fgrep -v "INFO:"
}

run truncate
run mkarray
run mkobject
run append
run set
run add