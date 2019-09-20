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

test_single_error_redirection() {
	serDir="${tmpDir}/ser"
	mkdir -p "${serDir}"
	serFile="${serDir}/err.txt"
	assert_equals "Output was captured" "$(try ${inouterr} abc 123 '!>' ${serFile})" "abc"
	assert_equals "Run single error redirection" "$(cat ${serFile})" '123'
}

test_error_redirection_with_pipe() {
	serDir="${tmpDir}/ser"
	mkdir -p "${serDir}"
	serFile="${serDir}/err.txt"
	assert_equals "Output was captured" "$(try ${inouterr} abc 123 '!>' ${serFile} '| tr a-z A-Z')" "ABC"
	assert_equals "Run single error redirection" "$(cat ${serFile})" '123'
}

test_multi_error_redirection_with_pipe() {
	serDir="${tmpDir}/ser"
	mkdir -p "${serDir}"
	serFile1="${serDir}/err1.txt"
	serFile2="${serDir}/err2.txt"
	assert_equals "Output was captured" "$(try ${inouterr} abc 123 Xyz '!>' ${serFile1} '|' ${inouterr} '!>' ${serFile2} '| tr a-z A-Z')" "CBA"
	assert_equals "Run single error redirection 1" "$(cat ${serFile1})" '123'
	assert_equals "Run single error redirection 2" "$(cat ${serFile2})" 'xyz'
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

test_short_cirquit_and() {
	assert_equals "Run false and cmd" "$(try 'false && echo fail')" 'Exit 1'
	assert_equals "Run true and cmd" "$(try 'echo test && echo ok')" 'test ok'
}

test_short_cirquit_or() {
	assert_equals "Run false or cmd" "$(try 'false || echo ok')" 'Exit 1 ok'
	assert_equals "Run true or cmd" "$(try 'echo ok || echo fail')" 'ok'
}

test_ambiguous_redirect() {
	assert_equals "Run ambiguous redirection" "$(try 'echo word > a > b')" 'Ambiguous output redirect.'
}

test_pipe_invalid_null_command_front() {
	assert_equals "Run null command in front of pipe" "$(try '|grep a')" 'Invalid null command.'
}

test_pipe_invalid_null_command_back() {
	assert_equals "Run null command at the end of pipe" "$(try 'ls|')" 'Invalid null command.'
}

test_bool_invalid_null_command_front() {
	assert_equals "Run null command in front of pipe" "$(try '&& true')" 'Invalid null command.'
}

test_bool_invalid_null_command_back() {
	assert_equals "Run null command at the end of pipe" "$(try 'true &&')" 'Invalid null command.'
}

test_pipe_huginn_to_system() {
	assert_equals \
		"Run pipe from huginn to system" \
		"$(try '
\import Algorithms as algo
\import Text as text
print(text.join(algo.materialize(algo.map(algo.range(3),text.cardinal),list), "\n" ) + "\n" )\; | tr a-z A-Z')" \
		'ZERO ONE TWO'
}

test_pipe_system_to_huginn() {
	assert_equals \
		"Run pipe from system to huginn" \
		"$(try 'seq 5 7 | while ( ( l = input() ) != none ) { print( "{}\n".format(number(l)!) )\; }')" \
		'120 720 5040'
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

