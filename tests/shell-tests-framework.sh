#! /bin/bash

startDir=$(pwd)
huginnPath="${startDir}/build/${TARGET:-debug}/huginn/1exec"
tmpDir="/tmp/huginn-tests"

trap return_error_message ERR;
trap 'test ${?} -eq 0 && /bin/rm -rf ${tmpDir}' EXIT

if [ ! -f "${huginnPath}" ] ; then
	echo "Cannot find the test subject."
fi

rm -rf "${tmpDir}"
mkdir -p "${tmpDir}"

huginnRun='${huginnPath} --no-color --no-default-init --quiet --log-path=${tmpDir}/${currentTest}.log --session-directory=${tmpDir} --session=${currentTest}_$(date +"%Y%m%d_%H%M%S") --shell'
inouterr="${huginnPath} ${startDir}/tests/data/inouterr.hgn"
params="${huginnPath} ${startDir}/tests/data/params.hgn"

normalize() {
	text="${@}"
	echo "${text}" | tr \\n " " | sed -E -e 's/\[\?2004[hl]//g;s/^[[:blank:]]+//;s/[[:blank:]]+$//;s/	/ /g'
}

fix_path() {
	local path="${1}"
	if ! echo "${path}" | grep -q '^/' ; then
		path="${startDir}/${path}"
	fi
	echo "${path}"
}

try() {
	out=$(echo "${@} || true" | eval "${huginnRun} 2> ${tmpDir}/err || true" )
	cat ${tmpDir}/err
	echo -n "${out}"
}

make_script() {
	local script="${tmpDir}/script_${currentTest}_$(date +'%Y%m%d_%H%M%S')"
	echo "#! ${huginnPath} -qs" > "${script}"
	echo "${params} ${@}" >> "${script}"
	chmod 700 "${script}"
	echo "${script}"
}

errMsg=""
return_error_message() {
	local lineNo=$(caller 0 | awk '{print $1}')
	local function=$(caller 0 | awk '{print $2}')
	local file=$(caller 0 | awk '{print $3}')
	realFile=$(fix_path "${file}")
	if [ -n "${errMsg}" ] ; then
		echo "${errMsg}"
	elif [ "${function}" != "function_call_forwarder" ] ; then
		echo "${file}:${lineNo}: ${function} - $(sed -n "${lineNo}p" "${realFile}" | sed -e 's/^[[:space:]]//g') - command failed"
	fi
}

currentTest=""
capture() {
	"${@}" >> "${tmpDir}/logs/${currentTest}.log" 2>&1
}

log() {
	echo "${@}" > /dev/tty
}

assert_equals() {
	local message="${1}"
	local actual=$(normalize "${2}")
	local expected=$(normalize "${3}")
	local lineNo=$(caller 0 | awk '{print $1}')
	local function=$(caller 0 | awk '{print $2}')
	local file=$(caller 0 | awk '{print $3}')
	if [[ "${actual}" != "${expected}" ]] ; then
		errMsg="${file}:${lineNo}: ${function} - Assertion failed: ${message}, expected: [${expected}], actual: [${actual}]"
		echo -n "F" >&2
		false
	else
		echo -n "." >&2
	fi
	errMsg=""
}

function_call_forwarder() {
	errMsg=""
	cd "${tmpDir}"
	(set -eEu && "${@}")
	true
}

run_tests() {
	pattern="${1}"
	shopt -s extdebug
	declare -a failures=()
	local count=0
	for functionName in $(declare -F $(declare -F | awk '{print $3}') | sort -nk2 | awk '{print $1}' | grep "${pattern}") ; do
		if [[ "${functionName}" =~ ^test_ ]] ; then
			echo -n "${functionName} "
			currentTest="${functionName}"
			errMsg=$(function_call_forwarder ${functionName})
			errMsg=$(normalize "${errMsg}")
			if [[ -z "${errMsg}" ]] ; then
				echo -e " \033[72Gok"
			else
				failures+=("${errMsg}")
				echo -e " \033[72Gfailed"
			fi
			count=$((count + 1))
		fi
	done
	if [[ ${#failures[@]} > 0 ]] ; then
		echo ""
		echo "${#failures[@]} out of ${count} tests have failed:"
		printf "%s\n" "${failures[@]}"
		exit 1
	fi
}

