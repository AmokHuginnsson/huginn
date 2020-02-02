" Vim syntax file
" Language:		hgnsh
" Maintainer:		Marcin Konarski <amok@codestation.org>
" Last Change:		2020-01-28
" Version:		1
" URL:		https://huginn.org/
" This file is derived from Charles E. Campbell sh/bash syntax file for vim.
" This file includes many ideas from Eric Brunet (eric.brunet@ens.fr)

" quit when a syntax file was already loaded {{{1
if exists("b:current_syntax")
  finish
endif

" set up default g:sh_fold_enabled {{{1
" ================================
if !exists("g:sh_fold_enabled")
 let g:sh_fold_enabled= 0
endif
if !exists("s:sh_fold_heredoc")
 let s:sh_fold_heredoc  = and(g:sh_fold_enabled,2)
endif
if !exists("s:sh_fold_ifdofor")
 let s:sh_fold_ifdofor  = and(g:sh_fold_enabled,4)
endif
if g:sh_fold_enabled && &fdm == "manual"
 " Given that	the	user provided g:sh_fold_enabled
 " 	AND	g:sh_fold_enabled is manual (usual default)
 " 	implies	a desire for syntax-based folding
 setl fdm=syntax
endif

" set up the syntax-highlighting iskeyword
exe "syn iskeyword ".&iskeyword.",-,:,."

" Set up folding commands for shell {{{1
" =================================
if s:sh_fold_heredoc
 com! -nargs=* ShFoldHereDoc <args> fold
else
 com! -nargs=* ShFoldHereDoc <args>
endif

" sh syntax is case sensitive {{{1
syn case match

" Clusters: contains=@... clusters {{{1
"==================================
syn cluster shErrorList	contains=shCurlyError,shParenError,shTestError,shOK
syn cluster shArithParenList	contains=shArithmetic,shComment,shDeref,shDerefSimple,shEcho,shEscape,shNumber,shOperator,shPosnParm,shExSingleQuote,shExDoubleQuote,shHereString,shRedir,shChain,shSingleQuote,shDoubleQuote,shStatement,hgnshBuiltin,shVariable,shAlias,shTest,shCtrlSeq,shSpecial,shParen,hgnshSpecialVariables,hgnshStatement,hgnshCommands,hgnshFunctions
syn cluster shArithList	contains=@shArithParenList,shParenError
syn cluster shCommandSubList	contains=shAlias,shArithmetic,shCmdParenRegion,shCtrlSeq,shDeref,shDerefSimple,shDoubleQuote,shEcho,shEscape,shExDoubleQuote,shExpr,shExSingleQuote,shHereDoc,shNumber,shOperator,shOption,shPosnParm,shHereString,shRedir,shChain,shSingleQuote,shSpecial,shStatement,hgnshBuiltin,shSubSh,shTest,shVariable
syn cluster shCurlyList	contains=shNumber,shComma,shDeref,shDerefSimple,shDerefSpecial
syn cluster shDblQuoteList	contains=shCommandSub,shDeref,shDerefSimple,shEscape,shPosnParm,shCtrlSeq,shSpecial
syn cluster shDerefList	contains=shDeref,shDerefSimple,shDerefVar,shDerefSpecial,shDerefWordError
syn cluster shDerefVarList	contains=shDerefOff,shDerefOp,shDerefVarArray,shDerefOpError
syn cluster shEchoList	contains=shArithmetic,shCommandSub,shDeref,shDerefSimple,shEscape,shExpr,shExSingleQuote,shExDoubleQuote,shSingleQuote,shDoubleQuote,shCtrlSeq,shEchoQuote
syn cluster shExprList1	contains=shCharClass,shNumber,shOperator,shExSingleQuote,shExDoubleQuote,shSingleQuote,shDoubleQuote,shExpr,shDblBrace,shDeref,shDerefSimple,shCtrlSeq
syn cluster shExprList2	contains=@shExprList1,shTest
syn cluster shHereBeginList	contains=@shCommandSubList
syn cluster shHereList	contains=shBeginHere,shHerePayload
syn cluster shHereListDQ	contains=shBeginHere,@shDblQuoteList,shHerePayload
syn cluster shIdList	contains=shCommandSub,shWrapLineOperator,shSetOption,shDeref,shDerefSimple,shHereString,shRedir,shChain,shExSingleQuote,shExDoubleQuote,shSingleQuote,shDoubleQuote,shExpr,shCtrlSeq,shStringSpecial,shAtExpr
syn cluster shSubShList	contains=@shCommandSubList,shColon,shCommandSub,shComment,shEcho,shExpr,shHereString,shRedir,shChain,shSetList,shSource,shStatement,hgnshBuiltin,shVariable,shCtrlSeq,shOperator
syn cluster shTestList	contains=shCharClass,shCommandSub,shCtrlSeq,shDeref,shDerefSimple,shDoubleQuote,shExDoubleQuote,shExpr,shExSingleQuote,shNumber,shOperator,shSingleQuote,shTest,shTestOpr
syn cluster shNoZSList	contains=shSpecialNoZS

" Echo: {{{1
" ====
" This one is needed INSIDE a CommandSub, so that `echo bla` be correct
syn region shEcho matchgroup=shStatement start="\<echo\>"  skip="\\$" matchgroup=shEchoDelim end="$" matchgroup=NONE end="[<>;&|()`]"me=e-1 end="\d[<>]"me=e-2 end="\s#"me=e-2 contains=@shEchoList skipwhite nextgroup=shQuickComment
syn region shEcho matchgroup=shStatement start="\<print\>" skip="\\$" matchgroup=shEchoDelim end="$" matchgroup=NONE end="[<>;&|()`]"me=e-1 end="\d[<>]"me=e-2 end="\s#"me=e-2 contains=@shEchoList skipwhite nextgroup=shQuickComment
syn match  shEchoQuote contained	'\%(\\\\\)*\\["`'()]'

" This must be after the strings, so that ... \" will be correct
syn region shEmbeddedEcho contained matchgroup=shStatement start="\<print\>" skip="\\$" matchgroup=shEchoDelim end="$" matchgroup=NONE end="[<>;&|`)]"me=e-1 end="\d[<>]"me=e-2 end="\s#"me=e-2 contains=shNumber,shExSingleQuote,shSingleQuote,shDeref,shDerefSimple,shSpecialVar,shOperator,shExDoubleQuote,shDoubleQuote,shCharClass,shCtrlSeq

" Alias: {{{1
" =====
syn match hgnshBuiltin "\<alias\>"
syn region shAlias matchgroup=hgnshBuiltin start="\<alias\>\s\+\(\h[-.,_[:alnum:]]\+\)\@="  skip="\\$" end="\>\|`"
syn region shAlias matchgroup=hgnshBuiltin start="\<alias\>\s\+\(\h[-.,_[:alnum:]]\+=\)\@=" skip="\\$" end="="

" Touch: {{{1
" =====
syn match shTouch	'\<touch\>[^;#]*'	skipwhite nextgroup=shComment contains=shTouchCmd,shDoubleQuote,shSingleQuote,shDeref,shDerefSimple
syn match shTouchCmd	'\<touch\>'		contained

" Error Codes: {{{1
" ============
syn match   shCurlyError "}"
syn match   shParenError ")"
syn match   shTestError "]"

" Options: {{{1
" ====================
syn match   shOption	"\s\zs[-+][-_a-zA-Z#@]\+"
syn match   shOption	"\s\zs--[^ \t$`'"|);]\+"

" File Redirection Highlighted As Operators: {{{1
"===========================================
syn match      shRedir	">"
syn match      shRedir	"!>"
syn match      shRedir	">&"
syn match      shRedir	">>&"
syn match      shRedir	">>"
syn match      shRedir	"!>>"
syn match      shRedir	"<"
syn match      shChain	"&&"
syn match      shChain	"||"

" Operators: {{{1
" ==========
syn match   shOperator	"<<\|>>"		contained
syn match   shOperator	"[!&;|]"		contained
syn match   shOperator	"\[[[^:]\|\]]"		contained
syn match   shOperator	"[-=/*+%]\=="		skipwhite nextgroup=shPattern
syn match   shPattern	"\<\S\+\())\)\@="	contained contains=shExSingleQuote,shSingleQuote,shExDoubleQuote,shDoubleQuote,shDeref

" Subshells: {{{1
" ==========
syn region shExpr  transparent matchgroup=shExprRegion  start="{" end="}"		contains=@shExprList2 nextgroup=shSpecialNxt
syn region shSubSh transparent matchgroup=shSubShRegion start="[^(]\zs(" end=")"	contains=@shSubShList nextgroup=shSpecialNxt

" Tests: {{{1
"=======
syn region shExpr	matchgroup=shRange start="\[" skip=+\\\\\|\\$\|\[+ end="\]" contains=@shTestList,shSpecial
syn region shTest	transparent matchgroup=shStatement start="\<test\s" skip=+\\\\\|\\$+ matchgroup=NONE end="[;&|!>]"me=e-1 end="$" contains=@shExprList1
syn region shNoQuote	start='\S'	skip='\%(\\\\\)*\\.'	end='\ze\s' end="\ze['"]"	contained contains=shDerefSimple,shDeref
syn match  shAstQuote	contained	'\*\ze"'	nextgroup=shString
syn match  shTestOpr	contained	'[^-+/%]\zs=' skipwhite nextgroup=shTestDoubleQuote,shTestSingleQuote,shTestPattern
syn match  shTestOpr	contained	"<=\|>=\|!=\|==\|=\~\|-.\>\|-\(nt\|ot\|ef\|eq\|ne\|lt\|le\|gt\|ge\)\>\|[!<>]"
syn match  shTestPattern	contained	'\w\+'
syn region shTestDoubleQuote	contained	start='\%(\%(\\\\\)*\\\)\@<!"' skip=+\\\\\|\\"+ end='"'	contains=shDeref,shDerefSimple,shDerefSpecial
syn match  shTestSingleQuote	contained	'\\.'	nextgroup=shTestSingleQuote
syn match  shTestSingleQuote	contained	"'[^']*'"
syn region  shDblBrace matchgroup=Delimiter start="\[\["	skip=+\%(\\\\\)*\\$+ end="\]\]"	contains=@shTestList,shAstQuote,shNoQuote,shComment
syn region  shDblParen matchgroup=Delimiter start="(("	skip=+\%(\\\\\)*\\$+ end="))"	contains=@shTestList,shComment

" Character Class In Range: {{{1
" =========================
syn match   shCharClass	contained	"\[:\(backspace\|escape\|return\|xdigit\|alnum\|alpha\|blank\|cntrl\|digit\|graph\|lower\|print\|punct\|space\|upper\|tab\):\]"

syn region shCurlyIn   contained	matchgroup=Delimiter start="{" end="}" contains=@shCurlyList
syn match  shComma     contained	","

" Misc: {{{1
"======
syn match   shWrapLineOperator "\\$"
syn region  shCommandSub   start="`" skip="\\\\\|\\." end="`"	contains=@shCommandSubList
syn match   shEscape	contained	'\%(^\)\@!\%(\\\\\)*\\.'

" $() and $(()): {{{1
" $(..) is not supported by sh (Bourne shell).  However, apparently
" some systems (HP?) have as their /bin/sh a (link to) Korn shell
" (ie. Posix compliant shell).  /bin/ksh should work for those
" systems too, however, so the following syntax will flag $(..) as
" an Error under /bin/sh.  By consensus of vimdev'ers!
syn region shCommandSub matchgroup=shCmdSubRegion start="\$("  skip='\\\\\|\\.' end=")"  contains=@shCommandSubList
syn region shArithmetic matchgroup=shArithRegion  start="\$((" skip='\\\\\|\\.' end="))" contains=@shArithList
syn region shArithmetic matchgroup=shArithRegion  start="\$\[" skip='\\\\\|\\.' end="\]" contains=@shArithList
syn match  shSkipInitWS contained	"^\s\+"
syn region shCmdParenRegion matchgroup=shCmdSubRegion start="(\ze[^(]" skip='\\\\\|\\.' end=")" contains=@shCommandSubList

syn cluster shCommandSubList add=hgnshSpecialVariables,hgnshStatement
syn keyword hgnshSpecialVariables contained PATH SHELL HOME PWD HGNLVL TERM CC CXX LC_CTYPE LC_COLLATE LC_TIME LESS PAGER EDITOR LANG LANGUAGE
syn keyword hgnshStatement chmod clear complete du egrep expr fgrep find gnufind gnugrep grep less ls mkdir cp mv rm rmdir rpm sed sleep sort strip tail which
syn keyword hgnshFunctions shell.run shell.has shell.source shell.grep shell.head fs.exists
syn keyword hgnshCommands tput dircolors lesspipe

syn match   shSource	"^\.\s"
syn match   shSource	"\s\.\s"
"syn region  shColon	start="^\s*:" end="$" end="\s#"me=e-2 contains=@shColonList
"syn region  shColon	start="^\s*\zs:" end="$" end="\s#"me=e-2

" String And Character Constants: {{{1
"================================
syn match   shNumber	"\<\d\+\>#\="
syn match   shNumber	"\<-\=\.\=\d\+\>#\="
syn match   shCtrlSeq	"\\\d\d\d\|\\[abcfnrtv0]"			contained
syn match   shSpecial	"[^\\]\(\\\\\)*\zs\\\o\o\o\|\\x\x\x\|\\c[^"]\|\\[abefnrtv]"	contained
syn match   shSpecial	"^\(\\\\\)*\zs\\\o\o\o\|\\x\x\x\|\\c[^"]\|\\[abefnrtv]"	contained
syn region  shExSingleQuote	matchgroup=shQuote start=+\$'+ skip=+\\\\\|\\.+ end=+'+	contains=shStringSpecial,shSpecial		nextgroup=shSpecialNxt
syn region  shExDoubleQuote	matchgroup=shQuote start=+\$"+ skip=+\\\\\|\\.\|\\"+ end=+"+	contains=@shDblQuoteList,shStringSpecial,shSpecial	nextgroup=shSpecialNxt
syn region  shSingleQuote	matchgroup=shQuote start=+'+ end=+'+		contains=@Spell
syn region  shDoubleQuote	matchgroup=shQuote start=+\%(\%(\\\\\)*\\\)\@<!"+ skip=+\\"+ end=+"+	contains=@shDblQuoteList,shStringSpecial,@Spell
syn match   shStringSpecial	"[^[:print:] \t]"			contained
syn match   shStringSpecial	"[^\\]\zs\%(\\\\\)*\\[\\"'`$()#]"
syn match   shSpecial	"[^\\]\zs\%(\\\\\)*\\[\\"'`$()#]"		nextgroup=shBkslshSnglQuote,shBkslshDblQuote,@shNoZSList
syn match   shSpecial	"^\%(\\\\\)*\\[\\"'`$()#]"
syn match   shSpecialNoZS	contained	"\%(\\\\\)*\\[\\"'`$()#]"
syn match   shSpecialNxt	contained	"\\[\\"'`$()#]"
syn region  shBkslshSnglQuote	contained	matchgroup=shQuote start=+'+ end=+'+	contains=@Spell
syn region  shBkslshDblQuote	contained	matchgroup=shQuote start=+"+ skip=+\\"+ end=+"+	contains=@shDblQuoteList,shStringSpecial,@Spell

" Comments: {{{1
"==========
syn cluster	shCommentGroup	contains=shTodo,@Spell
syn match	shTodo	contained		"\<\%(COMBAK\|FIXME\|TODO\|XXX\)\ze:\=\>"
syn match	shComment		"^\s*\zs#.*$"	contains=@shCommentGroup
syn match	shComment		"\s\zs#.*$"	contains=@shCommentGroup
syn match	shComment	contained	"#.*$"	contains=@shCommentGroup
syn match	shQuickComment	contained	"#.*$"

" Here Documents: {{{1
" =========================================
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc01 start="<<\s*\\\=\z([^ \t|>]\+\)"		matchgroup=shHereDoc01 end="^\z1\s*$"	contains=@shDblQuoteList
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc02 start="<<\s*\"\z([^ \t|>]\+\)\""		matchgroup=shHereDoc02 end="^\z1\s*$"
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc03 start="<<-\s*\z([^ \t|>]\+\)"		matchgroup=shHereDoc03 end="^\s*\z1\s*$"	contains=@shDblQuoteList
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc04 start="<<-\s*'\z([^']\+\)'"		matchgroup=shHereDoc04 end="^\s*\z1\s*$"
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc05 start="<<\s*'\z([^']\+\)'"		matchgroup=shHereDoc05 end="^\z1\s*$"
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc06 start="<<-\s*\"\z([^ \t|>]\+\)\""		matchgroup=shHereDoc06 end="^\s*\z1\s*$"
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc07 start="<<\s*\\\_$\_s*\z([^ \t|>]\+\)"		matchgroup=shHereDoc07 end="^\z1\s*$"           contains=@shDblQuoteList
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc08 start="<<\s*\\\_$\_s*'\z([^ \t|>]\+\)'"	matchgroup=shHereDoc08 end="^\z1\s*$"
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc09 start="<<\s*\\\_$\_s*\"\z([^ \t|>]\+\)\""	matchgroup=shHereDoc09 end="^\z1\s*$"
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc10 start="<<-\s*\\\_$\_s*\z([^ \t|>]\+\)"	matchgroup=shHereDoc10 end="^\s*\z1\s*$"
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc11 start="<<-\s*\\\_$\_s*\\\z([^ \t|>]\+\)"	matchgroup=shHereDoc11 end="^\s*\z1\s*$"
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc12 start="<<-\s*\\\_$\_s*'\z([^ \t|>]\+\)'"	matchgroup=shHereDoc12 end="^\s*\z1\s*$"
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc13 start="<<-\s*\\\_$\_s*\"\z([^ \t|>]\+\)\""	matchgroup=shHereDoc13 end="^\s*\z1\s*$"
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc14 start="<<\\\z([^ \t|>]\+\)"		matchgroup=shHereDoc14 end="^\z1\s*$"
ShFoldHereDoc syn region shHereDoc matchgroup=shHereDoc15 start="<<-\s*\\\z([^ \t|>]\+\)"		matchgroup=shHereDoc15 end="^\s*\z1\s*$"

" Here Strings: {{{1
" =============
" available for: bash; ksh (really should be ksh93 only) but not if its a posix
syn match shHereString "<<<"	skipwhite	nextgroup=shCmdParenRegion

" Identifiers: {{{1
"=============
syn match  shSetOption	"\s\zs[-+][a-zA-Z0-9]\+\>"	contained
syn match  shVariable	"\<\([bwglsav]:\)\=[a-zA-Z0-9.!@_%+,]*\ze="	nextgroup=shVarAssign
syn match  shVarAssign	"="		contained	nextgroup=shCmdParenRegion,shPattern,shDeref,shDerefSimple,shDoubleQuote,shExDoubleQuote,shSingleQuote,shExSingleQuote
syn region shAtExpr	contained	start="@(" end=")" contains=@shIdList
syn region shSetList oneline matchgroup=shSet start="\<\(setenv\|unsetenv\|setopt\)\>\ze[^/]" end="$"	matchgroup=shSetListDelim end="\ze[}|);&]" matchgroup=NONE end="\ze\s\+#\|="	contains=@shIdList

" Parameter Dereferencing: {{{1
" ========================
if !exists("g:sh_no_error")
 syn match  shDerefWordError	"[^}$[~]"	contained
endif
syn match  shDerefSimple	"\$\%(\h\w*\|\d\)"	nextgroup=@shNoZSList
syn region shDeref	matchgroup=PreProc start="\${" end="}"	contains=@shDerefList,shDerefVarArray
syn match  shDerefSimple	"\$[-#*@!?]"	nextgroup=@shNoZSList
syn match  shDerefSimple	"\$\$"	nextgroup=@shNoZSList
syn match  shDerefSimple	"\${\d}"	nextgroup=@shNoZSList
syn region shDeref	matchgroup=PreProc start="\${##\=" end="}"	contains=@shDerefList	nextgroup=@shSpecialNoZS
syn region shDeref	matchgroup=PreProc start="\${\$\$" end="}"	contains=@shDerefList	nextgroup=@shSpecialNoZS

" bash: ${!prefix*} and ${#parameter}: {{{1
" ====================================
syn region shDeref	matchgroup=PreProc start="\${!" end="\*\=}"	contains=@shDerefList,shDerefOff
syn match  shDerefVar	contained	"{\@<=!\h\w*"		nextgroup=@shDerefVarList

syn match  shDerefSpecial	contained	"{\@<=[-*@?0]"		nextgroup=shDerefOp,shDerefOpError
syn match  shDerefSpecial	contained	"\({[#!]\)\@<=[[:alnum:]*@_]\+"	nextgroup=@shDerefVarList,shDerefOp
syn match  shDerefVar	contained	"{\@<=\h\w*"		nextgroup=@shDerefVarList
syn match  shDerefVar	contained	'\d'                            nextgroup=@shDerefVarList

" sh ksh bash : ${var[... ]...}  array reference: {{{1
syn region  shDerefVarArray   contained	matchgroup=shDeref start="\[" end="]"	contains=@shCommandSubList nextgroup=shDerefOp,shDerefOpError

" Special ${parameter OPERATOR word} handling: {{{1
" : ${parameter:-word}    word is default value
" : ${parameter:?word}    display word if parameter is null
" : ${parameter:+word}    use word if parameter is not null, otherwise nothing
syn cluster shDerefPatternList	contains=shDerefPattern,shDerefString
if !exists("g:sh_no_error")
 syn match shDerefOpError	contained	":[[:punct:]]"
endif
syn match  shDerefOp	contained	":\=[-+?]"	nextgroup=@shDerefPatternList
syn match  shDerefPattern	contained	"[^{}]\+"		contains=shDeref,shDerefSimple,shDerefPattern,shDerefString,shCommandSub,shDerefEscape nextgroup=shDerefPattern
syn region shDerefPattern	contained	start="{" end="}"	contains=shDeref,shDerefSimple,shDerefString,shCommandSub nextgroup=shDerefPattern
syn match  shDerefEscape	contained	'\%(\\\\\)*\\.'
syn region shDerefString	contained	matchgroup=shDerefDelim start=+\%(\\\)\@<!'+ end=+'+	contains=shStringSpecial
syn region shDerefString	contained	matchgroup=shDerefDelim start=+\%(\\\)\@<!"+ skip=+\\"+ end=+"+	contains=@shDblQuoteList,shStringSpecial
syn match  shDerefString	contained	"\\["']"	nextgroup=shDerefPattern

" Arithmetic Parenthesized Expressions: {{{1
"syn region shParen matchgroup=shArithRegion start='[^$]\zs(\%(\ze[^(]\|$\)' end=')' contains=@shArithParenList
syn region shParen matchgroup=shArithRegion start='\$\@!(\%(\ze[^(]\|$\)' end=')' contains=@shArithParenList

" Useful sh Keywords: {{{1
" ===================
syn keyword hgnshBuiltin bg bindkey cd dirs eval exec exit fg help history jobs rehash source ulimit umask unalias wait

" Synchronization: {{{1
" ================
if !exists("g:sh_minlines")
 let s:sh_minlines = 200
else
 let s:sh_minlines= g:sh_minlines
endif
if !exists("g:sh_maxlines")
 let s:sh_maxlines = 2*s:sh_minlines
 if s:sh_maxlines < 25
  let s:sh_maxlines= 25
 endif
else
 let s:sh_maxlines= g:sh_maxlines
endif
exec "syn sync minlines=" . s:sh_minlines . " maxlines=" . s:sh_maxlines

" Default Highlighting: {{{1
" =====================
if !exists("skip_sh_syntax_inits")
 hi def link shArithRegion	shShellVariables
 hi def link shAstQuote	shDoubleQuote
 hi def link shAtExpr	shSetList
 hi def link shBeginHere	shRedir
 hi def link shQuote	shOperator
 hi def link shCmdSubRegion	shShellVariables
 hi def link shColon	shComment
 hi def link shDerefOp	shOperator
 hi def link shDeref	shShellVariables
 hi def link shDerefDelim	shOperator
 hi def link shDerefSimple	shDeref
 hi def link shDerefSpecial	shDeref
 hi def link shDerefString	shDoubleQuote
 hi def link shDerefVar	shDeref
 hi def link shDoubleQuote	shString
 hi def link shEcho	shString
 hi def link shEchoDelim	shOperator
 hi def link shEchoQuote	shString
 hi def link shEmbeddedEcho	shString
 hi def link shEscape	shCommandSub
 hi def link shExDoubleQuote	shDoubleQuote
 hi def link shExSingleQuote	shSingleQuote
 hi def link shHereDoc	shString
 hi def link shHereString	shRedir
 hi def link shHerePayload	shHereDoc
 hi def link shSpecialNxt	shSpecial
 hi def link shNoQuote	shDoubleQuote
 hi def link shOption	shCommandSub
 hi def link shPattern	shString
 hi def link shParen	shArithmetic
 hi def link shPosnParm	shShellVariables
 hi def link shQuickComment	shComment
 hi def link shRange	shOperator
 hi def link shRedir	shOperator
 hi def link shChain	shOperator
 hi def link shSetListDelim	shOperator
 hi def link shSetOption	shOption
 hi def link shSingleQuote	shString
 hi def link shSource	shOperator
 hi def link shStringSpecial	shSpecial
 hi def link shSubShRegion	shOperator
 hi def link shTestOpr	shConditional
 hi def link shTestPattern	shString
 hi def link shTestDoubleQuote	shString
 hi def link shTestSingleQuote	shString
 hi def link shTouchCmd	shStatement
 hi def link shVariable	shSetList
 hi def link shWrapLineOperator	shOperator

 hi def link hgnshSpecialVariables	shShellVariables
 hi def link hgnshStatement		shStatement
 hi def link hgnshCommands		shStatement
 hi def link hgnshFunctions		Type
 hi def link shCharClass		shSpecial
 hi def link shDerefOff		shDerefOp
 hi def link shDerefLen		shDerefOff

 if !exists("g:sh_no_error")
  hi def link shCurlyError		Error
  hi def link shDerefOpError		Error
  hi def link shDerefWordError		Error
  hi def link shParenError		Error
  hi def link shTestError		Error
 endif

 hi def link shArithmetic		Special
 hi def link shCharClass		Identifier
 hi def link shCommandSub		Special
 hi def link shComment		Comment
 hi def link shConditional		Conditional
 hi def link shCtrlSeq		Special
 hi def link shExprRegion		Delimiter
 hi def link shNumber		Number
 hi def link shOperator		Operator
 hi def link shSet		hgnshBuiltin
 hi def link shSetList		Identifier
 hi def link shShellVariables		PreProc
 hi def link shSpecial		Special
 hi def link shSpecialNoZS		shSpecial
 hi def link shStatement		Statement
 hi def link hgnshBuiltin		shBuiltin
 hi def link shString		String
 hi def link shTodo		Todo
 hi def link shAlias		Identifier
 hi def link shHereDoc01		shRedir
 hi def link shHereDoc02		shRedir
 hi def link shHereDoc03		shRedir
 hi def link shHereDoc04		shRedir
 hi def link shHereDoc05		shRedir
 hi def link shHereDoc06		shRedir
 hi def link shHereDoc07		shRedir
 hi def link shHereDoc08		shRedir
 hi def link shHereDoc09		shRedir
 hi def link shHereDoc10		shRedir
 hi def link shHereDoc11		shRedir
 hi def link shHereDoc12		shRedir
 hi def link shHereDoc13		shRedir
 hi def link shHereDoc14		shRedir
 hi def link shHereDoc15		shRedir
endif

hi shBuiltin cterm=NONE ctermfg=red gui=NONE guifg=red
hi Identifier cterm=NONE ctermfg=darkcyan gui=NONE guifg=darkcyan

" Delete shell folding commands {{{1
" =============================
delc ShFoldHereDoc

" Set Current Syntax: {{{1
" ===================
let b:current_syntax = "bash"

" vim: ts=16 fdm=marker
