#! /bin/bash

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
	assert_equals "Run single pipe" "$(try find "${spDir}" -printf '%f\n' | sort | tr 'a-z' 'A-Z' | grep [[:digit:]])" "789UWV DEF012 JK456MN"
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

run_tests

