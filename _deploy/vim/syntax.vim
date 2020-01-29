" Vim syntax highlighting for Huginn language.
" Language:    Huginn
" Maintainer:  Marcin Konarski <amok@codestation.org>
" Last change: 2020-01-29

set cindent

syn keyword hgnKeyword assert break case catch class constructor continue default destructor else enum for if return super switch this throw try while
syn keyword hgnBuildin boolean character integer number real string tuple list deque dict lookup order set heap blob type size copy observe use
syn match hgnBuildin "[∑∏√]"
syn keyword hgnLiteral false none true
syn keyword hgnInclude import as from

" Literals
syn match hgnNumber display "$\=\<\d\+\>"
syn match hgnNumber display "\<0x\x\+\>"
syn match hgnNumber display "$\=\<\d\+\.\d*\>"
syn match hgnNumber display "$\=\.\d\+\>"
syn match hgnSpecialCharacter display @\\["'\\abfnrtv]@ contained containedin=hgnString
syn region hgnString display start=@"@ end=@"@ contains=hgnSpecialCharacter,@Spell
syn region hgnCharacter display start=@'@ end=@'@ contains=hgnSpecialCharacter

" Operators
syn match hgnOperator display "[<=>!^~&|;:,.+*%/@\?(){}\[\]¬⋀⋁⊕∈∉-]"

" Comments
syn region hgnPrepocessor matchgroup=hgnPrepocessor display start=@^#@ end=@$@ skip=@\\$@ contains=@Spell
syn region hgnCommentMultiline matchgroup=hgnComment display start=@/\*@ end=@\*/@ skip=@\\$@ contains=@Spell
syn region hgnComment display start=@//@ end=@$@ oneline contains=@Spell

" Errors
syn region hgnTernary transparent start=@\\?@ skip=@$@ end=@:@
syn match hgnTernaryError display ";" contained containedin=hgnTernary
syn match hgnCommentError display "/\*"me=e-1 contained containedin=hgnCommentMultiline

" {{{

syn match huginnClass display /\<[A-Z][a-zA-Z]*[a-z][a-zA-Z]*\>/
syn match huginnFieldVariable display /\<_[a-zA-Z0-9]\+\>/ containedin=cBitField
syn match huginnArgumentVariable display /\<[a-zA-Z0-9]\+_\>/
syn match huginnGlobalVariable display /\<_[a-zA-Z0-9]\+_\>/
syn match huginnBits display /\<[A-Z][A-Z0-9_]*\>/

" }}}

hi def link hgnNumber hgnLiteral
hi def link hgnString hgnLiteral
hi def link hgnCharacter hgnLiteral
hi def link hgnCommentMultiline hgnComment
hi def link hgnCommentSingleLine hgnComment
hi def link hgnTernaryError hgnError
hi def link hgnCommentError hgnError
hi def link hgnInclude hgnPrepocessor

hi hgnKeyword cterm=NONE ctermfg=yellow gui=NONE guifg=yellow
hi hgnBuildin cterm=NONE ctermfg=green gui=NONE guifg=green
hi hgnPrepocessor cterm=NONE ctermfg=blue gui=NONE guifg=blue
hi hgnSpecialCharacter cterm=NONE ctermfg=red gui=NONE guifg=red
hi hgnLiteral cterm=NONE ctermfg=magenta gui=NONE guifg=magenta
hi hgnOperator cterm=NONE ctermfg=white gui=NONE guifg=white
hi hgnComment cterm=NONE ctermfg=cyan gui=NONE guifg=cyan
hi hgnError cterm=NONE ctermfg=white ctermbg=red gui=NONE guifg=white guibg=red

" colors {{{
hi huginnClass cterm=NONE ctermfg=3 gui=NONE guifg=#c08040
hi huginnFieldVariable cterm=NONE ctermfg=blue gui=NONE guifg=#8080ff
hi huginnArgumentVariable cterm=NONE ctermfg=darkgreen gui=NONE guifg=#50b060
hi huginnGlobalVariable cterm=NONE ctermfg=red gui=NONE guifg=red
hi huginnBits cterm=NONE ctermfg=darkcyan gui=NONE guifg=darkcyan
" }}}

" vim:ft=vim
