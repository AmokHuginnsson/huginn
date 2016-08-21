#! /bin/sh

if [ ! -f Makefile.mk.in ] ; then
	echo "Setup script must be exeuted from project's root directory."
	exit 1
fi

set -- "../yaal/_aux" "/usr/share/yaal/_aux"

for path do
	if [ -d "${path}" ] ; then
		ln -nsf "${path}"
		exit 0
	fi
done

echo "yaal's auxilary files were not found on this system."
exit 1

