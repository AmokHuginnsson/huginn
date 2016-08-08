"""
		pygments.lexers.huginn
		~~~~~~~~~~~~~~~~~~~~~~

		Lexer for the Huginn language.

		:copyright: Copyright 2016 Marcin Konarski
		:license: BSD, see LICENSE for details.
"""

from pygments.lexer import RegexLexer, include, words
from pygments.token import Text, Comment, Operator, Keyword, Name, String, Number, Punctuation

__all__ = ["HuginnLexer"]

class HuginnLexer(RegexLexer):
	"""
		For `Huginn <http://http://codestation.org/?h-action=menu-project&menu=submenu-project&page=&project=huginn>`_ source.
	"""
	name = "Huginn"
	aliases = ["huginn"]
	filenames = ["*.hgn"]
	mimetypes = ["text/x-huginn"]
	tokens = {
		"whitespace": [
			(r"\n", Text),
			(r"\s+", Text),
			(r"\\\n", Text),  # line continuation
			(r"//(\n|(.|\n)*?[^\\]\n)", Comment.Single),
			(r"/(\\\n)?[*](.|\n)*?[*](\\\n)?/", Comment.Multiline),
		],
		"root": [
			include('whitespace'),
			(words(("if", "else", "for", "while", "switch", "case", "default", "break", "continue", "class", "super", "this", "constructor", "destructor"), suffix=r"\b"), Keyword),
			(words(("integer", "string", "number", "real", "character", "boolean", "list", "deque", "dict", "order", "lookup", "set", "size", "type", "copy", "observe", "use"), suffix=r"\b"), Keyword.Reserved),
			(r'L?"', String, 'string'),
			(r'[0-9]*.[0-9]+', Number.Float),
			(r'[0-9]+', Number.Integer),
			(r'0(x|X)[0-9A-Fa-f]+', Number.Hex),
			(r'0(o|O)[0-9A-Fa-f]+', Number.Oct),
			(r'0(b|B)[0-9A-Fa-f]+', Number.Bin),
			(r'[A-Z]\w*', Name.Class.Instance),
			(r'[a-z]\w*', Name),
			(r'_\w+', Name.Variable.Instance),
			(r"L?'(\\.|\\[0-7]{1,3}|\\x[a-fA-F0-9]{1,2}|[^\\\'\n])'", String.Char),
			(r'==|!=|>=|>|<=|<|&&|\|\||\+|-|=|/|\*|%|\^|\+=|-=|\*=|/=|%=|\^=|!|\?|\||:|@', Operator),
			(r'\{|\}|\(|\)|\[|\]|,|\.|;', Punctuation),
			(r'[\r\n\t ]+', Text),
		],
		"string": [
			(r'"', String, '#pop'),
			(r'\\([\\abfnrtv"\']|x[a-fA-F0-9]{2,4}|'
				r'u[a-fA-F0-9]{4}|U[a-fA-F0-9]{8}|[0-7]{1,3})', String.Escape),
			(r'[^\\"\n]+', String),  # all other characters
			(r'\\\n', String),  # line continuation
			(r'\\', String),  # stray backslash
		],
	}
