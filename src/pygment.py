"""
		pygments.lexers.huginn
		~~~~~~~~~~~~~~~~~~~~~~

		Lexer for the Huginn language.

		:copyright: Copyright 2016 Marcin Konarski
		:license: BSD, see LICENSE for details.
"""

from pygments.lexer import RegexLexer, include, words
from pygments.token import Text, Comment, Operator, Keyword, Name, String, Number, Punctuation, Error, Generic, Token, Whitespace
from pygments.style import Style
from pygments.formatters import Terminal256Formatter, TerminalFormatter, TerminalTrueColorFormatter
from pygments.styles import get_style_by_name
from copy import deepcopy

HuginnStyleDefiniton = deepcopy( get_style_by_name( "vim" ).styles )
HuginnStyleDefiniton[Keyword] = "#ff0"
HuginnStyleDefiniton[Keyword.Reserved] = "#0f0"
HuginnStyleDefiniton[Keyword.Constant] = "#f0f"
HuginnStyleDefiniton[Name.Variable.Field] = "#44f"
HuginnStyleDefiniton[Name.Variable.Argument] = "#0c0"
HuginnStyleDefiniton[Operator] = "#fff"
HuginnStyleDefiniton[Punctuation] = "#fff"
HuginnStyleDefiniton[String] = "#f0f"
HuginnStyleDefiniton[Number] = "#f0f"
HuginnStyleDefiniton[Comment] = "#0ff"
HuginnStyleDefiniton[Name.Class.Instance] = "#b60"
HuginnStyleDefiniton[Name.Constant] = "#0cc"
HuginnStyleDefiniton[String.Escape] = "#f00"

class HuginnStyle( Style ):
	default_style = ""
	styles = HuginnStyleDefiniton

TerminalTrueColorFormatter.__initOrig__ = TerminalTrueColorFormatter.__init__
def TerminalTrueColorFormatterInit( self_, **options ):
	options["style"] = HuginnStyle
	self_.__initOrig__( **options )
TerminalTrueColorFormatter.__init__ = TerminalTrueColorFormatterInit

Terminal256Formatter.__initOrig__ = Terminal256Formatter.__init__
def Terminal256FormatterInit( self_, **options ):
	options["style"] = HuginnStyle
	self_.__initOrig__( **options )
Terminal256Formatter.__init__ = Terminal256FormatterInit

TerminalFormatter.__initOrig__ = TerminalFormatter.__init__
def TerminalFormatterInit( self_, **options ):
	options["colorscheme"] = {
		Token:                  ('darkgray',    'lightgray'),
		Whitespace:             ('darkgray',    'darkgray'),
		Comment:                ('teal',        'turquoise'),
		Name.Constant:          ('turquoise',   'teal'),
		Keyword:                ('red',         'yellow'),
		Keyword.Constant:       ('purple' ,     'fuchsia'),
		Keyword.Reserved:       ('darkgreen',   'green'),
		Operator:               ('darkgray',    'lightgray'),
		Punctuation:            ('darkgray',    'lightgray'),
		Name.Class.Instance:    ('brown',       'brown'),
		Name.Variable.Field:    ('darkblue',    'blue'),
		Name.Variable.Argument: ('green',       'darkgreen'),
		String:                 ('purple',      'fuchsia'),
		String.Escape:          ('brown',       'red'),
		Number:                 ('purple',      'fuchsia'),
	}
	self_.__initOrig__( **options )
TerminalFormatter.__init__ = TerminalFormatterInit

__all__ = [ "HuginnLexer" ]

class HuginnLexer( RegexLexer ):
	"""
		For `Huginn <https://huginn.org/>`_ source.
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
			(words( ( "assert", "break", "case", "catch", "class", "constructor", "continue", "default", "destructor", "else", "enum", "for", "if", "return", "super", "switch", "this", "throw", "try", "while" ), prefix=r"\b", suffix=r"\b" ), Keyword),
			(words( ( "blob", "boolean", "character", "copy", "deque", "dict", "integer", "list", "lookup", "number", "observe", "order", "real", "set", "size", "string", "tuple", "type", "use" ), prefix=r"\b", suffix=r"\b" ), Keyword.Reserved),
			(words( ( "true", "false", "none" ), prefix=r"\b", suffix=r"\b" ), Keyword.Constant),
			(r'L?"', String, 'string'),
			(r"L?'(\d\.|\\[0-7]{1,3}|\\x[a-fA-F0-9]{1,2}|[^\\\'\n])'", String.Char),
			(r'-?(\d+\.\d*|\.\d+)', Number.Float),
			(r'\$-?(\d+\.\d*|\.\d+|\d+)', Number.Float),
			(r'0[xX][0-9A-Fa-f]+', Number.Hex),
			(r'0[oO][0-7]+', Number.Oct),
			(r'0[bB][01]+', Number.Bin),
			(r'-?\d+', Number.Integer),
			(r'[A-Z][A-Z_]*[A-Z]', Name.Constant),
			(r'[A-Z]\w*', Name.Class.Instance),
			(r'\b_\w+\b', Name.Variable.Field),
			(r'\b\w+_\b', Name.Variable.Argument),
			(r'==|!=|>=|>|<=|<|&&|\|\||\+|-|=|/|\*|%|\^|\+=|-=|\*=|/=|%=|\^=|!|\?|\||:|@|⋀|⋁|⊕|¬', Operator),
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
