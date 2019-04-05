#! /usr/bin/env bash

set -eEu

source ./tests/shell-tests-framework.sh

test_single_command() {
	stDir="${tmpDir}/st"
	mkdir -p "${stDir}"
	touch "${stDir}/a" "${stDir}/b"
	assert_equals "Run single shell command" "$(try ls "${stDir}")" "a b"
}

test_single_pipe() {
	spDir="${tmpDir}/sp"
	mkdir -p "${spDir}"
	for f in abc def012 ghi jk456mn opqr 789uwv ; do
		touch "${spDir}/${f}"
	done
	assert_equals "Run single pipe" "$(try find "${spDir}" -print | xargs -I {} basename {} | sort | tr 'a-z' 'A-Z' | grep -P [[:digit:]])" "789UWV DEF012 JK456MN"
}

test_single_output_redirection() {
	sorDir="${tmpDir}/sor"
	mkdir -p "${sorDir}"
	sorFile="${sorDir}/out.txt"
	try "echo 'some text' > ${sorFile}"
	assert_equals "Run single output redirection" "$(cat ${sorFile})" 'some text'
}

test_single_input_redirection() {
	sirDir="${tmpDir}/sir"
	mkdir -p "${sirDir}"
	sirFile="${sirDir}/in.txt"
	echo 'some text' > "${sirFile}"
	assert_equals "Run single input redirection" "$(try "tr a-z A-Z < ${sirFile}")" 'SOME TEXT'
}

test_input_and_output_redirection() {
	iorDir="${tmpDir}/ior"
	mkdir -p "${iorDir}"
	irFile="${iorDir}/in.txt"
	orFile="${iorDir}/out.txt"
	echo 'some text' > "${irFile}"
	try "tr a-z A-Z < ${irFile} > ${orFile}"
	assert_equals "Run input and output redirection" "$(cat ${orFile})" 'SOME TEXT'
}

test_ambiguous_redirect() {
	assert_equals "Run ambiguous redirection" "$(try 'echo word > a > b')" 'Ambiguous output redirect.'
}

test_environment_variables() {
	export ENV_VAR="envVal"
	export OTHER_ENV="failed?"
	export DIRECT="_direct_"
	assert_equals "Enviromnent variables expansion" "$(try 'echo "Env: ${ENV_VAR}" '"'\${OTHER_ENV}' \${DIRECT}")" 'Env: envVal ${OTHER_ENV} _direct_'
}

test_brace_expansion_word() {
	assert_equals "Brace expansion" "$(try 'echo prefix-{infix,other,}-suffix')" 'prefix-infix-suffix prefix-other-suffix prefix--suffix'
}

test_brace_expansion_number() {
	assert_equals "Brace expansion" "$(try 'echo prefix-{1..5}')" 'prefix-1 prefix-2 prefix-3 prefix-4 prefix-5'
}

run_tests "${1:-.}"

