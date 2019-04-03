#! /bin/bash

trap return_error_message ERR;

huginnPath="./build/debug/huginn/1exec"
tmpDir="/tmp/huginn-tests"
errMsg=""

if [ ! -f "${huginnPath}" ] ; then
	echo "Cannot find the test subject."
fi

rm -rf "${tmpDir}"
mkdir -p "${tmpDir}"

huginnRun="${huginnPath} --no-color --quiet --session-directory=${tmpDir} --shell"

normalize() {
	text="${@}"
	echo "${text}" | tr \\n " " | sed -e 's/^\s\+//' -e 's/\s\+$//'
}

try() {
	echo "${@}" | ${huginnRun} 2>&1
}

return_error_message() {
	echo "${errMsg}"
}

log() {
	echo "${@}" > /dev/tty
}

assert_equals() {
	local message="${1}"
	local actual=$(normalize "${2}")
	local expected=$(normalize "${3}")
	local frame=$(caller 0)
	if [[ "${actual}" != "${expected}" ]] ; then
		 export errMsg="Assertion failed, ${frame}, ${message}, expected: [${expected}], actual: [${actual}]"
		 false
	fi
	export errMsg=""
}

function_call_forwarder() {
	(set -eEu && ${@})
	true
}

run_tests() {
	shopt -s extdebug
	declare -a failures=()
	for functionName in $(declare -F $(declare -F | awk '{print $3}') | sort -nk2 | awk '{print $1}') ; do
		if [[ "${functionName}" =~ ^test_ ]] ; then
			echo -n "${functionName}..."
			errMsg=$(function_call_forwarder ${functionName})
			if [[ -z "${errMsg}" ]] ; then
				echo " ok"
			else
				failures+=("${errMsg}")
				echo " failed"
			fi
		fi
	done
	if [[ ${#failures[@]} > 0 ]] ; then
		echo "${failures}"
	fi
}

