#! /bin/sh

here=$(pwd)
huginnExecutable="${here}/build/reldeb/huginn/1exec"
yaalTools="libyaal_tools-rd.so.0.0"
yaalRealPath="${HOME}/usr/lib"
yaalArtifact="${yaalRealPath}/${yaalTools}"

if [ ! -f "${yaalArtifact}" ] ; then
	echo "Cannot find yaal libraries (${yaalArtifact})."
	exit 1
fi

if [ ! -f "${huginnExecutable}" ] ; then
	echo "Cannot find Huginn executable (${huginnExecutable})."
	exit 1
fi

yaalSymPath="/usr/local/lib/yaal"

ln -sf "${here}/tests/start-dev-shell.sh" /usr/local/bin/hgnsh
mkdir -p "${yaalSymPath}"
chmod 755 "${yaalSymPath}"
ln -sf "${yaalRealPath}"/libyaal_*-rd.so.0.0 "${yaalSymPath}"

ldSoDir="/etc/ld.so.conf.d"
if [ -d "${ldSoDir}" ] ; then
	ldSoConf="${ldSoDir}/yaal-rd.conf"
	echo "${yaalSymPath}" > "${ldSoConf}"
	chmod 644 "${ldSoConf}"
	ldconfig
fi

if [ -f /etc/freebsd-update.conf ] ; then
	ldconfig -m "${yaalSymPath}"
fi
