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

test_single_redirection() {
	srDir="${tmpDir}/sr"
	mkdir -p "${srDir}"
	srFile="${srDir}/out.txt"
	try "echo 'some text' > ${srFile}"
	assert_equals "Run single redirection" "$(cat ${srFile})" 'some text'
}

run_tests

