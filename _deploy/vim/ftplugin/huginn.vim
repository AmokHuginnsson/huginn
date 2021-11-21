" Huginn language editor settings.

set foldmethod=marker
let b:foldmarker="{,}"
set foldnestmax=5
set cindent
let g:tagbar_type_huginn = {
	\ 'ctagstype': 'huginn',
	\ 'kinds': [
		\ 'e:enumerators:0:1',
		\ 'i:imports:1:0',
		\ 'f:functions:0:1',
		\ 'm:members:0:1',
		\ 'v:variables:0:0',
		\ 'c:classes:0:1'
	\ ],
	\ 'sro': '.',
	\ 'kind2scope': {
		\ 'c': 'class',
		\ 'e': 'enum',
		\ 'm': 'member'
	\ },
	\ 'scope2kind': {
		\ 'class': 'c',
		\ 'enum': 'e',
		\ 'member': 'm'
	\ },
	\ 'ctagsbin': 'huginn',
	\ 'ctagsargs': '--tags'
\ }

