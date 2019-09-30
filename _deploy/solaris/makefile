VPATH :sh = pwd
ARCH :sh = isalist | awk '{print $1}'
OS :sh = uname -r
BASEDIR :sh = awk -F '=' '/BASEDIR/{print $$2}' pkginfo.in | tr -d '"'
VERSION :sh = awk '/^VERSION *=|SUBVERSION *=|EXTRAVERSION *=/{VERSION=VERSION DOT $3;DOT="."}END{print VERSION}' ../../Makefile.mk.in

include solaris.mk
