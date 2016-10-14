"""
		pygments.lexers.huginn
		~~~~~~~~~~~~~~~~~~~~~~

		Lexer for the Huginn language.

		:copyright: Copyright 2016 Marcin Konarski
		:license: BSD, see LICENSE for details.
"""

from pygments.lexer import RegexLexer, include, words
from pygments.token import Text, Comment, Operator, Keyword, Name, String, Number, Punctuation, Error, Generic
from pygments.style import Style
from pygments.formatters import Terminal256Formatter
from pygments.styles import get_style_by_name
from copy import deepcopy

Terminal256Formatter.__initOrig__ = Terminal256Formatter.__init__
HuginnStyle = deepcopy( get_style_by_name( "vim" ).styles )
HuginnStyle[Keyword] = "#ff0"
HuginnStyle[Keyword.Reserved] = "#0f0"
HuginnStyle[Keyword.Constant] = "#f0f"
HuginnStyle[Name.Variable.Field] = "#44f"
HuginnStyle[Name.Variable.Argument] = "#0c0"
HuginnStyle[Operator] = "#fff"
HuginnStyle[Punctuation] = "#fff"
HuginnStyle[String] = "#f0f"
HuginnStyle[Number] = "#f0f"
HuginnStyle[Comment] = "#0ff"
HuginnStyle[Name.Class.Instance] = "#b60"
HuginnStyle[String.Escape] = "#f00"

class Huginn( Style ):
	default_style = ""
	styles = HuginnStyle

def Terminal256FormatterInit( self_, **options ):
	options["style"] = Huginn
	self_.__initOrig__( **options )

Terminal256Formatter.__init__ = Terminal256FormatterInit

__all__ = [ "HuginnLexer" ]


class HuginnLexer( RegexLexer ):
	"""
		For `Huginn <http://http://codestation.org/?h-action=menu-project&menu=submenu-project&page=&project=huginn>`_ source.
	"""
	name = "Huginn"
	aliases = [ "huginn" ]
	filenames = [ "*.hgn" ]
	mimetypes = [ "text/x-huginn" ]
	pygments_style = "Huginn"
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
			(words( ( "if", "else", "for", "while", "switch", "case", "default", "break", "continue", "class", "super", "this", "constructor", "destructor", "assert", "return", "throw", "try", "catch" ), prefix=r"\b", suffix=r"\b" ), Keyword),
			(words( ( "integer", "string", "number", "real", "character", "boolean", "list", "deque", "dict", "order", "lookup", "set", "size", "type", "copy", "observe", "use" ), prefix=r"\b", suffix=r"\b" ), Keyword.Reserved),
			(words( ( "true", "false", "none" ), prefix=r"\b", suffix=r"\b" ), Keyword.Constant),
			(r'L?"', String, 'string'),
			(r"L?'(\d\.|\\[0-7]{1,3}|\\x[a-fA-F0-9]{1,2}|[^\\\'\n])'", String.Char),
			(r'-?(\d+\.\d*|\.\d+)', Number.Float),
			(r'\$-?(\d+\.\d*|\.\d+|\d+)', Number.Float),
			(r'0[xX][0-9A-Fa-f]+', Number.Hex),
			(r'0[oO][0-7]+', Number.Oct),
			(r'0[bB][01]+', Number.Bin),
			(r'-?\d+', Number.Integer),
			(r'[A-Z]\w*', Name.Class.Instance),
			(r'\b_\w+\b', Name.Variable.Field),
			(r'\b\w+_\b', Name.Variable.Argument),
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
