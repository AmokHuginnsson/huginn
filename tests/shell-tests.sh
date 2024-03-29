#! /usr/bin/env bash

set -eEu

source ./tests/shell-tests-framework.sh

test_parser() {
	assert_equals "Parser 0" "$(try ${params} abc\'def\'zz)" '[0]:"params" [1]:"abcdefzz"'
	assert_equals "Parser 1" "$(try ${params} \"abc\'def\'\"zz)" '[0]:"params" [1]:"abc'"'"'def'"'"'zz"'
	assert_equals "Parser 2" "$(try ${params} \'abc\"def\"\'zz)" '[0]:"params" [1]:"abc"def"zz"'
	assert_equals "Parser 3" "$(try ${params} abc\'def\'zz{0,1})" '[0]:"params" [1]:"abcdefzz0" [2]:"abcdefzz1"'
	assert_equals "Parser unmathed 0" "$(try echo \'aaa)" "code: \`echo 'aaa || true\` *standard input*:1: Unmatched '''."
	assert_equals "Parser unmathed 1" "$(try echo \"aaa)" "code: \`echo \"aaa || true\` *standard input*:1: Unmatched '\"'."
	assert_equals "Parser unmathed 2" "$(try echo \$\(aaa)" "code: \`echo \$(aaa || true\` *standard input*:1: Unmatched '\$('."
	assert_equals "Parser excl(!)" "$(try echo \\!\\!\\!)" "!!!"
	assert_equals "Parser excl(!)" "$(try echo a!b c!d e!f)" "a!b c!d e!f"
}

test_quotes() {
	assert_equals "Quotes env 0" "$(try setenv QQ rr\\ ss \&\& ${params} aa\${QQ}bb)" '[0]:"params" [1]:"aarr" [2]:"ssbb"'
	assert_equals "Quotes env 1" "$(try setenv QQ rr\\ ss \&\& ${params} '"'aa\${QQ}bb'"')" '[0]:"params" [1]:"aarr ssbb"'
	assert_equals "Quotes env 2" "$(try setenv QQ rr\\ ss \&\& ${params} aa'"'\${QQ}'"'bb)" '[0]:"params" [1]:"aarr ssbb"'
	assert_equals "Quotes exe 0" "$(try ${params} 'aa$(echo rr ss)bb')" '[0]:"params" [1]:"aarr" [2]:"ssbb"'
	assert_equals "Quotes exe 1" "$(try ${params} '"aa$(echo rr ss)bb"')" '[0]:"params" [1]:"aarr ssbb"'
	assert_equals "Quotes exe 2" "$(try ${params} '"aa'"'"'$(echo rr ss)'"'"'bb"')" '[0]:"params" [1]:"aa'"'"'rr ss'"'"'bb"'
	assert_equals "Quotes exe 0" "$(try ${params} 'aa$(echo rr ss; echo 11 22)bb')" '[0]:"params" [1]:"aarr" [2]:"ss" [3]:"11" [4]:"22bb"'
	assert_equals "Quotes exe 0" "$(try ${params} 'aa$(echo rr ss && echo 11 22)bb')" '[0]:"params" [1]:"aarr" [2]:"ss" [3]:"11" [4]:"22bb"'
	assert_equals "Quotes exe 0" "$(try ${params} 'aa$(echo rr ss | grep r && echo 11 22)bb')" '[0]:"params" [1]:"aarr" [2]:"ss" [3]:"11" [4]:"22bb"'
	assert_equals "Quotes exe 3" "$(try echo '"aa$(echo '"'"'rr ss)bb"')" "code: \`echo \"aa\$(echo 'rr ss)bb\" || true\` *standard input*:1: Unmatched '''."
	assert_equals "Quotes exe 4" "$(try echo '"aa$(historz '"'"'rr ss'"'"')bb"')" "expected one of characters: [ historz: command not found, did you mean: \`history\`? *standard input*:1: Abort -1  historz 'rr ss' aabb"
	assert_equals "Quotes exe 5" "$(try echo '$(echo "aaaa")')" "aaaa"
	assert_equals "Quotes exe 6" "$(try echo '$(echo '"'"'aaaa'"'"')')" "aaaa"
	assert_equals "Quotes exe 5" "$(try alias a b \&\& echo '"$(alias | grep a)"')" "a  b"
}

test_process_substitution() {
	DD=dd
	if [ -f /usr/local/bin/gdd ] ; then
		DD=gdd
	fi
	assert_equals "Basic <()" "$(try cat \<\(echo abc\))" "abc"
	assert_equals "Piped <()" "$(try cat \<\(echo abc \| tr a-z A-Z\))" "ABC"
	assert_equals "Chained <()" "$(try cat \<\(echo abc \; echo XYZ\))" "abc XYZ"
	assert_equals "Double <()" "$(try paste \<\(echo abc\) \<\(echo DEF\))" "abc	DEF"
	assert_equals "Double <() with pipes" "$(try paste \<\(echo abc \| tr a-z A-Z\) \<\(echo DEF \| tr A-Z a-z\))" "ABC	def"
	assert_equals "Double <() with chains" "$(try paste \<\(echo abc\;echo rst\) \<\(echo DEF\;echo XYZ\))" "abc	DEF rst	XYZ"
	assert_equals "Basic >()" "$(try echo abc \| ${DD} status=none of=\>\(cat\>z\)\;cat z)" "abc"
	assert_equals "Basic >() with pipe" "$(try echo abc \| ${DD} status=none of=\>\(cat \| tr a-z A-Z \> z\)\;cat z)" "ABC"
	assert_equals "Double >()" "$(try echo abcd \| tee \>\(cat \> ab\) \>\(cat \> cd\) \> /dev/null \;cat ab cd)" "abcd abcd"
	assert_equals "Double >() with pipe" "$(try echo abcd \| tee \>\(cat \| tr a-b A-B \> ab\) \>\(cat \| tr c-d C-D \> cd\) \> /dev/null \;cat ab cd)" "ABcd abCD"
	assert_equals "Basic <() >()" "$(try ${DD} status=none if=\<\(echo abcd\) of=\>\(/bin/cat\>z\)\;cat z)" "abcd"
	assert_equals "Piped <() >()" "$(try ${DD} status=none if=\<\(echo abcd \| tr a-b A-B\) of=\>\(/bin/cat \| tr c-d C-D \>z\)\;cat z)" "ABCD"
}

test_glob() {
	assert_equals "glob 1" "$(try echo \*glob\*)" "test_glob.log"
	assert_equals "glob 1" "$(try echo \\\*)" "*"
	assert_equals "glob 1" "$(try echo \"\*\")" "*"
	assert_equals "glob 1" "$(try echo '\*')" "*"
}

test_script() {
	script=$(make_script)
	assert_equals "no params" "$(${script})" '[0]:"params"'
	script=$(make_script '"${1}" "${2}"')
	assert_equals "explicit params" "$(${script} a\ b c)" '[0]:"params" [1]:"a b" [2]:"c"'
	script=$(make_script '"${*}"')
	assert_equals "star param quotes" "$(${script} a\ b c)" '[0]:"params" [1]:"a b c"'
	script=$(make_script '${*}')
	assert_equals "star param no-quotes" "$(${script} a\ b c)" '[0]:"params" [1]:"a" [2]:"b" [3]:"c"'
	script=$(make_script '${@}')
	assert_equals "at param no-quotes" "$(${script} a\ b c)" '[0]:"params" [1]:"a" [2]:"b" [3]:"c"'
	script=$(make_script '"${@}"')
	assert_equals "at param quotes" "$(${script} a\ b c)" '[0]:"params" [1]:"a b" [2]:"c"'
	assert_equals "at param quotes no arg" "$(${script})" '[0]:"params"'
	script=$(make_script '"H${@}T"')
	assert_equals "at param quotes head tail" "$(${script} a\ b c)" '[0]:"params" [1]:"Ha b" [2]:"cT"'
	assert_equals "at param quotes head tail no arg" "$(${script})" '[0]:"params" [1]:"HT"'
	script=$(make_script '"H${@}M${@}T"')
	assert_equals "at param quotes head mid tail" "$(${script} a\ b c)" '[0]:"params" [1]:"Ha b" [2]:"cMa b" [3]:"cT"'
	script=$(make_script '"=${#}="')
	assert_equals "param count 0" "$(${script})" '[0]:"params" [1]:"=0="'
	assert_equals "param count n > 0" "$(${script} a\ b c)" '[0]:"params" [1]:"=2="'
	cat > "${script}" << EOS
#! ${huginnPath} -qs

# some empty lines

true

#single line
echo -n ""

true

# multi line
echo \\
	-n \\
	""

# error line
cd cd cd
EOS
	assert_equals "error line" "$(${script} 2>&1 || true)" "${script}:18: cd: Too many arguments! Exit 1"
}

test_builtin_cd() {
	assert_equals \
		"Run cd builtin command" \
		"$(try 'pwd;cd /tmp;pwd;cd -;pwd;cd;pwd;cd =2;pwd')" \
		"/tmp/huginn-tests /tmp /tmp/huginn-tests ${HOME} /tmp"
	assert_equals \
		"Run cd too many args" \
		"$(try 'cd a b')" \
		"*standard input*:1: cd: Too many arguments! Exit 1"
}

test_builtin_alias() {
	assert_equals \
		"Run alias builtin command" \
		"$(try 'alias back cd -;alias home cd;alias back;alias;pwd;home;pwd;back;pwd')" \
		"back cd - back  cd - home  cd /tmp/huginn-tests ${HOME} /tmp/huginn-tests"
}

test_builtin_unalias() {
	assert_equals \
		"Run unalias builtin command" \
		"$(try 'pwd;alias pwd echo zzz;pwd;unalias pwd;pwd')" \
		"/tmp/huginn-tests zzz /tmp/huginn-tests"
	assert_equals \
		"Run unalias non-existing" \
		"$(try 'unalias zzz')" \
		""
}

test_builtin_bindkey() {
	assert_equals \
		"Run bindkey builtin command" \
		"$(try 'bindkey --system F3 foo();bindkey --system S-F12 date;bindkey --system S-F12;bindkey --system;bindkey S-F12;bindkey')" \
		"S-F12 date F3     foo() S-F12  date S-F12 date F3     foo() S-F12  date"
	assert_equals \
		"Run bindkey on invalid" \
		"$(try 'bindkey --system SF12 x')" \
		"*standard input*:1: bindkey: invalid key name: SF12 Exit 1"
	assert_equals \
		"Run bindkey non-existing key binding" \
		"$(try 'bindkey --system F3')" \
		"*standard input*:1: bindkey: Unbound key: \`F3\` Exit 1"
	assert_equals \
		"Run bindkey without target" \
		"$(try 'bindkey F3 date')" \
		"*standard input*:1: bindkey: bind target not set Exit 1"
}

test_builtin_setenv() {
	assert_equals \
		"Run setenv builtin command" \
		"$(try 'echo "aa${ZZ}bb";setenv ZZ cc;echo "aa${ZZ}bb"')" \
		"aabb aaccbb"
	assert_equals \
		"Run setenv no param" \
		"$(try 'setenv')" \
		"*standard input*:1: setenv: Missing parameter! Exit 1"
}

test_builtin_unsetenv() {
	assert_equals \
		"Run unsetenv builtin command" \
		"$(try 'echo "aa${ZZ}bb";setenv ZZ cc;echo "aa${ZZ}bb";unsetenv ZZ;echo "aa${ZZ}bb"')" \
		"aabb aaccbb aabb"
	assert_equals \
		"Run unsetenv no param" \
		"$(try 'unsetenv')" \
		"*standard input*:1: unsetenv: Missing parameter! Exit 1"
}

test_builtin_setopt() {
	assert_equals \
		"Run setopt builtin command" \
		"$(try 'setopt ignore_filenames \\*~')" \
		""
	assert_equals \
		"Run setopt no param" \
		"$(try 'setopt')" \
		"*standard input*:1: setopt: Missing parameter! Exit 1"
	assert_equals \
		"Run setopt no param" \
		"$(try 'setopt blah')" \
		"*standard input*:1: setopt: unknown option: blah! Exit 1"
	assert_equals \
		"Run setopt with bad bool" \
		"$(try 'setopt trace zoom')" \
		"*standard input*:1: not a boolean value: zoom Exit 1"
	assert_equals \
		"Test tracing" \
		"$(try 'pwd;setopt trace on;pwd;setopt trace off;pwd')" \
		"/tmp/huginn-tests + pwd /tmp/huginn-tests + setopt trace off /tmp/huginn-tests"
	assert_equals \
		"Test prefix_commands" \
		"$(try 'alias P pwd;setopt prefix_commands env;env P')" \
		"/tmp/huginn-tests"
}

test_builtin_source() {
	srcDir="${tmpDir}/source"
	mkdir -p "${srcDir}"
	script="${srcDir}/script"
	echo "pwd" >> "${script}"
	assert_equals "Run 'source' builtin" "$(try source "${script}")" "/tmp/huginn-tests"
	echo "echo 'hgn'" >> "${script}"
	assert_equals "Run 'source' builtin" "$(try source "${script}")" "/tmp/huginn-tests hgn"
	echo "cd /tmp /tmp" >> "${script}"
	assert_equals "Run 'source' builtin" "$(try source "${script}")" "/tmp/huginn-tests/source/script:3: cd: Too many arguments! Exit 1 /tmp/huginn-tests hgn"
	echo 'echo "${HGNSH_SOURCE}"' >> "${script}"
	assert_equals "Run 'source' builtin" "$(try source "${script}")" "/tmp/huginn-tests/source/script:3: cd: Too many arguments! Exit 1 /tmp/huginn-tests hgn /tmp/huginn-tests/source/script"
	echo "source ${script}" >> "${script}"
	assert_equals "Run 'source' builtin" "$(try source "${script}")" "/tmp/huginn-tests/source/script:3: cd: Too many arguments! Exit 1 /tmp/huginn-tests/source/script:5: Recursive \`source\` of '/tmp/huginn-tests/source/script' script detected. Exit 1 /tmp/huginn-tests hgn /tmp/huginn-tests/source/script"
}

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
	assert_equals "Run single pipe" "$(try find "${spDir}" -print '|' xargs -I {} basename {} '|' sort '|' tr 'a-z' 'A-Z' '|' grep -E [[:digit:]])" "789UWV DEF012 JK456MN"
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

test_stdout_and_stderr_through_pipe() {
	assert_equals "Pass stdout only through pipe" "$(try ${inouterr} abc def '|' tr a-z A-Z)" 'def ABC'
	assert_equals "Pass stdout and stderr through pipe" "$(try ${inouterr} abc def '|&' tr a-z A-Z)" 'ABC DEF'
}

test_error_redirection_with_pipe_stdout_to_file() {
	sorDir="${tmpDir}/sor"
	mkdir -p "${sorDir}"
	sorFile="${sorDir}/out.txt"
	assert_equals "Output was captured" "$(try ${inouterr} abc def '>' ${sorFile} '!& | tr a-z A-Z')" "DEF"
	assert_equals "Run single output redirection" "$(cat ${sorFile})" 'abc'
}

test_jobs_background() {
	assert_equals \
		"Run job in background till it finishes" \
		"$(try 'sleep 1.4&jobs;sleep .5;jobs;sleep .5 ; jobs;sleep .5 ; jobs ; jobs ; jobs')" \
		"[1] Done sleep 1.4 [1] Running   sleep 1.4 [1] Running   sleep 1.4 [1] Running   sleep 1.4"
	assert_equals \
		"Run job in background and bring it to foreground" \
		"$(try 'sleep 1.4&jobs;sleep .5;fg; jobs')" \
		"sleep 1.4 [1] Running   sleep 1.4"
	assert_equals \
		"Try to run a background job in command substitution" \
		"$(try 'echo $(sleep 1.4&)')" \
		"code: \`echo \$(sleep 1.4&) || true\` *standard input*:1: Background jobs in command substitution are forbidden."
	assert_equals \
		"Try to foreground invalid job number" \
		"$(try 'sleep 1.4&jobs;sleep .5;fg 2; jobs ; fg ; jobs')" \
		"sleep 1.4 *standard input*:1: fg: Invalid job number! 2 Exit 1 [1] Running   sleep 1.4 [1] Running   sleep 1.4"
	assert_equals \
		"Try to background with no jobs" \
		"$(try 'bg')" \
		"*standard input*:1: bg: No current job! Exit 1"
	assert_equals \
		"Try to foreground with no jobs" \
		"$(try 'fg')" \
		"*standard input*:1: fg: No current job! Exit 1"
}

test_short_cirquit_and() {
	assert_equals "Run false and cmd" "$(try 'false && echo fail')" '*standard input*:1: Exit 1'
	assert_equals "Run true and cmd" "$(try 'echo test && echo ok')" 'test ok'
}

test_short_cirquit_or() {
	assert_equals "Run false or cmd" "$(try 'false || echo ok')" '*standard input*:1: Exit 1 ok'
	assert_equals "Run true or cmd" "$(try 'echo ok || echo fail')" 'ok'
}

test_ambiguous_redirect() {
	assert_equals "Run ambiguous redirection" "$(try 'echo word > a > b')" 'code: `echo word > a > b || true` *standard input*:1: Ambiguous output redirect.'
}

test_repeated_stdout_redirection() {
	assert_equals "Run repeared stdout redirection" "$(try 'echo word > zzz !& > b')" 'code: `echo word > zzz !& > b || true` *standard input*:1: Output stream is already redirected, use >& to combine error and output streams.'
}

test_missing_stdout_redirection() {
	assert_equals "Run repeared stdout redirection" "$(try 'echo word !&')" 'code: `echo word !& || true` *standard input*:1: Output stream is not redirected, use >& or |& to combine error and output streams.'
}

test_pipe_invalid_null_command_front() {
	assert_equals "Run null command in front of pipe" "$(try '|grep a')" 'code: `|grep a || true` *standard input*:1: Invalid null command.'
}

test_pipe_invalid_null_command_back() {
	assert_equals "Run null command at the end of pipe" "$(try 'ls|')" 'code: `ls| || true` *standard input*:1: Invalid null command.'
}

test_pipe_to_redir_invalid_null_command(){
	assert_equals "Run redirect after pipe without command between" "$(try 'ls | > a')" 'code: `ls | > a || true` *standard input*:1: Invalid null command.'
}

test_pipe_after_redir(){
	assert_equals "Run pipe after redirect without file between" "$(try 'ls > | cat')" 'code: `ls > | cat || true` *standard input*:1: Missing name or redirect.'
}

test_bool_invalid_null_command_front() {
	assert_equals "Run null command in front of pipe" "$(try '&& true')" 'code: `&& true || true` *standard input*:1: Invalid null command.'
}

test_bool_invalid_null_command_back() {
	assert_equals "Run null command at the end of pipe" "$(try 'true &&')" 'code: `true && || true` *standard input*:1: Invalid null command.'
}

test_pipe_huginn_to_system() {
	assert_equals \
		"Run pipe from huginn to system" \
		"$(try '
\import Algorithms as algo
\import Text as text
print(text.join(algo.materialize(algo.map(algo.range(3),text.cardinal),list), "\134n" ) + "\134n" )\; | tr a-z A-Z')" \
		'ZERO ONE TWO'
}

test_pipe_system_to_huginn() {
	assert_equals \
		"Run pipe from system to huginn" \
		"$(try 'seq 5 7 | while ( ( l = input() ) != none ) { print( "{}\134n".format(number(l)!) )\; }')" \
		'120 720 5040'
}

test_command_substitution_from_huginn() {
	assert_equals \
		"Substitute from Huginn" \
		"$(try '
\import Text as text
echo $(text.ordinal(7\)) | tr a-z A-Z')" \
		'SEVENTH'
}

test_command_substitution_from_system() {
	assert_equals "Substitute from system" "$(try 'basename $(pwd) | tr a-z A-Z')" 'HUGINN-TESTS'
}

test_environment_variables() {
	export ENV_VAR="envVal"
	export OTHER_ENV="failed?"
	export DIRECT="_direct_"
	assert_equals "Enviromnent variables expansion" "$(try 'echo "Env: ${ENV_VAR}" '"'\${OTHER_ENV}' \${DIRECT}")" 'Env: envVal ${OTHER_ENV} _direct_'
}

test_brace_expansion_word() {
	assert_equals "Brace expansion" "$(try 'echo prefix-{infix,other,}-suffix')" 'prefix-infix-suffix prefix-other-suffix prefix--suffix'
	assert_equals "Brace expansion quoted" "$(try 'echo '"'"'{aa,bb}'"'"'')" '{aa,bb}'
}

test_brace_expansion_number() {
	assert_equals "Brace expansion" "$(try 'echo prefix-{1..5}')" 'prefix-1 prefix-2 prefix-3 prefix-4 prefix-5'
	assert_equals "Brace expansion quoted" "$(try 'echo '"'"'{1..5}'"'"'')" '{1..5}'
	assert_equals "Brace expansion" "$(try 'echo a{1..10..3}')" 'a1 a4 a7 a10'
	assert_equals "Brace expansion" "$(try 'echo a{1..10..-3}')" 'a1 a4 a7 a10'
	assert_equals "Brace expansion" "$(try 'echo a{10..1..3}')" 'a10 a7 a4 a1'
	assert_equals "Brace expansion" "$(try 'echo a{10..1..-3}')" 'a10 a7 a4 a1'
}

test_brace_expansion_rosetta_code() {
	assert_equals "Brace expansion It{{em,alic}iz,erat}e{d,},_please." "$(try 'echo It{{em,alic}iz,erat}e{d,},_please.')" 'Itemized,_please. Itemize,_please. Italicized,_please. Italicize,_please. Iterated,_please. Iterate,_please.'
	assert_equals "Brace expansion {,{,gotta_have{_,\,_again\,_}}more_}cowbell!" "$(try 'echo {,{,gotta_have{_,\,_again\,_}}more_}cowbell!')" 'cowbell! more_cowbell! gotta_have_more_cowbell! gotta_have,_again,_more_cowbell!'
	assert_equals "Brace expansion {{a,b}\\,}z" "$(try 'echo {{a,b}\,}z')" '{a,}z {b,}z'
	assert_equals "Brace expansion {}}_some_}{,{\\{_edge,_edge}_\,}{_cases,_{here}_\\\\\}" "$(try 'echo {}}_some_}{,{\\{_edge,_edge}_\,}{_cases,_{here}_\\\\\}')" '{}}_some_}{,{\_edge_,}{_cases,_{here}_\\} {}}_some_}{,{\_edge_,}{_cases,_{here}_\\}'
}

run_tests "${1:-.}"

