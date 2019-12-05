#! /bin/bash

export YAAL_AUTO_SANITY=yes
exec -a hgnsh ${HOME}/src/huginn/build/reldeb/huginn/1exec -L ${HOME}/var/log/hgnsh.log -qs "${@}"

# vim: ft=sh
