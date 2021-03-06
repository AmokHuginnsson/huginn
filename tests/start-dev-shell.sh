#! /bin/sh

test -z "${DEV_SHELL}" && DEV_SHELL=hgnsh exec bash "${0}" "${@}"
unset DEV_SHELL

export YAAL_AUTO_SANITY=yes
export SHELL=${HOME}/src/huginn/tests/reldeb/hgnsh
exec -a "${0}" ${HOME}/src/huginn/build/reldeb/huginn/1exec -L ${HOME}/var/log/hgnsh.log -qs "${@}"

export SHELL=${HOME}/src/huginn/tests/debug/hgnsh
export LD_LIBRARY_PATH=${HOME}/usr/lib:/home/konarski/lib:${HOME}/usr/local/lib:/usr/local/lib
exec -a "${0}" ${HOME}/src/huginn/build/debug/huginn/1exec -L ${HOME}/var/log/hgnsh.log -qs "${@}"

# vim: ft=sh
