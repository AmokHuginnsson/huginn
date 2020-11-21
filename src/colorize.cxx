/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstring>

#include <yaal/hcore/hhashmap.hxx>
#include <yaal/hcore/hregex.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/hfsitem.hxx>
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/color.hxx>
#include <yaal/tools/huginn/helper.hxx>
M_VCSID( "$Id: " __ID__ " $" )
#include "colorize.hxx"
#include "systemshell.hxx"
#include "shell/util.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::filesystem;

namespace yaal { namespace hcore {
template<>
hash<::huginn::GROUP>::hash_value_type hash<::huginn::GROUP>::operator () ( ::huginn::GROUP const& val_ ) const {
	return ( static_cast<hash_value_type>( val_ ) );
}
} }

namespace huginn {

namespace {

typedef yaal::hcore::HHashMap<yaal::hcore::HString, scheme_t const*> schemes_t;

string::tokens_t _keywords_ = {
	"assert", "break", "case", "catch", "class", "constructor", "continue", "default", "destructor", "else",
	"enum", "for", "if", "return", "super", "switch", "throw", "try", "while", "this"
};
string::tokens_t _builtins_ = {
	"blob", "boolean", "character", "copy", "deque", "dict", "heap", "integer", "list", "lookup",
	"number", "observe", "order", "real", "set", "size", "string", "tuple", "type", "use"
};
string::tokens_t _literals_ = { "false", "none", "true" };
string::tokens_t _import_ = { "import", "as", "from" };
string::tokens_t _builtinSymbols_ = { "√", "∑", "∏" };

scheme_t const _schemeDarkBG_ = {
	{ GROUP::KEYWORDS, COLOR::FG_YELLOW },
	{ GROUP::BUILTINS, COLOR::FG_BRIGHTGREEN },
	{ GROUP::CLASSES, COLOR::FG_BROWN },
	{ GROUP::ENUMS, COLOR::FG_CYAN },
	{ GROUP::FIELDS, COLOR::FG_BRIGHTBLUE },
	{ GROUP::ARGUMENTS, COLOR::FG_GREEN },
	{ GROUP::GLOBALS, COLOR::FG_BRIGHTRED },
	{ GROUP::LITERALS, COLOR::FG_BRIGHTMAGENTA },
	{ GROUP::COMMENTS, COLOR::FG_BRIGHTCYAN },
	{ GROUP::IMPORT, COLOR::FG_BRIGHTBLUE },
	{ GROUP::OPERATORS, COLOR::FG_WHITE },
	{ GROUP::SHELL_BUILTINS, COLOR::FG_RED },
	{ GROUP::ALIASES, COLOR::FG_CYAN },
	{ GROUP::EXECUTABLES, COLOR::FG_BRIGHTGREEN },
	{ GROUP::DIRECTORIES, COLOR::FG_BRIGHTBLUE },
	{ GROUP::SYMBOLIC_LINKS, COLOR::FG_BRIGHTCYAN },
	{ GROUP::FIFOS, COLOR::FG_BROWN },
	{ GROUP::DEVICES, COLOR::FG_YELLOW },
	{ GROUP::SOCKETS, COLOR::FG_BRIGHTMAGENTA },
	{ GROUP::SUID, COLOR::FG_BRIGHTRED },
	{ GROUP::LOCAL_HOST, COLOR::FG_GREEN },
	{ GROUP::REMOTE_HOST, COLOR::FG_YELLOW },
	{ GROUP::ARCHIVES, COLOR::FG_BRIGHTRED },
	{ GROUP::MEDIA, COLOR::FG_BRIGHTMAGENTA },
	{ GROUP::SWITCHES, COLOR::FG_BRIGHTRED },
	{ GROUP::ENVIRONMENT, COLOR::FG_CYAN },
	{ GROUP::ENV_STR, COLOR::FG_BRIGHTBLUE },
	{ GROUP::PIPES, COLOR::FG_YELLOW },
	{ GROUP::SUBSTITUTION, COLOR::FG_WHITE },
	{ GROUP::ESCAPE, COLOR::FG_BRIGHTRED },
	{ GROUP::PROMPT, COLOR::FG_BLUE },
	{ GROUP::PROMPT_MARK, COLOR::FG_BRIGHTBLUE },
	{ GROUP::HINT, COLOR::FG_GRAY }
};

scheme_t const _schemeBrightBG_ = {
	{ GROUP::KEYWORDS, COLOR::FG_BROWN },
	{ GROUP::BUILTINS, COLOR::FG_GREEN },
	{ GROUP::CLASSES, COLOR::FG_BRIGHTRED },
	{ GROUP::ENUMS, COLOR::FG_BRIGHTCYAN },
	{ GROUP::FIELDS, COLOR::FG_BLUE },
	{ GROUP::ARGUMENTS, COLOR::FG_BRIGHTGREEN },
	{ GROUP::GLOBALS, COLOR::FG_RED },
	{ GROUP::LITERALS, COLOR::FG_MAGENTA },
	{ GROUP::COMMENTS, COLOR::FG_CYAN },
	{ GROUP::IMPORT, COLOR::FG_BLUE },
	{ GROUP::OPERATORS, COLOR::FG_BLACK },
	{ GROUP::SHELL_BUILTINS, COLOR::FG_BRIGHTRED },
	{ GROUP::ALIASES, COLOR::FG_BRIGHTCYAN },
	{ GROUP::EXECUTABLES, COLOR::FG_GREEN },
	{ GROUP::DIRECTORIES, COLOR::FG_BLUE },
	{ GROUP::SYMBOLIC_LINKS, COLOR::FG_CYAN },
	{ GROUP::FIFOS, COLOR::FG_YELLOW },
	{ GROUP::DEVICES, COLOR::FG_BROWN },
	{ GROUP::SOCKETS, COLOR::FG_MAGENTA },
	{ GROUP::SUID, COLOR::FG_RED },
	{ GROUP::LOCAL_HOST, COLOR::FG_BRIGHTGREEN },
	{ GROUP::REMOTE_HOST, COLOR::FG_BROWN },
	{ GROUP::ARCHIVES, COLOR::FG_RED },
	{ GROUP::MEDIA, COLOR::FG_MAGENTA },
	{ GROUP::SWITCHES, COLOR::FG_RED },
	{ GROUP::ENVIRONMENT, COLOR::FG_BRIGHTCYAN },
	{ GROUP::ENV_STR, COLOR::FG_BLUE },
	{ GROUP::PIPES, COLOR::FG_BROWN },
	{ GROUP::SUBSTITUTION, COLOR::FG_BLACK },
	{ GROUP::ESCAPE, COLOR::FG_RED },
	{ GROUP::PROMPT, COLOR::FG_BRIGHTBLUE },
	{ GROUP::PROMPT_MARK, COLOR::FG_BLUE },
	{ GROUP::HINT, COLOR::FG_LIGHTGRAY }
};

schemes_t _schemes_ = {
	{ "dark-background", &_schemeDarkBG_ },
	{ "bright-background", &_schemeBrightBG_ }
};

scheme_t const* _scheme_( &_schemeDarkBG_ );

typedef yaal::hcore::HPointer<yaal::hcore::HRegex> regex_t;
typedef yaal::hcore::HHashMap<yaal::hcore::HString, regex_t> matchers_t;
matchers_t _regex_ = {
	{ "numbers", make_pointer<HRegex>( "(\\b0[bB][01]+|\\b0[oO]?[0-7]+|\\b0[xX][0-9a-fA-F]+|(\\$?\\b[0-9]+\\.|\\$?\\.[0-9]+|\\$?\\b[0-9]+\\.[0-9]+|\\$?\\b[0-9]+)([eE][-+]?[0-9]+)?)\\b" ) },
	{ "classes", make_pointer<HRegex>( "\\b[A-Z][a-zA-Z]*\\b" ) },
	{ "enums", make_pointer<HRegex>( "\\b[A-Z][A-Z0-9_]*[A-Z0-9]\\b" ) },
	{ "fields", make_pointer<HRegex>( "\\b_[a-zA-Z0-9]+\\b" ) },
	{ "arguments", make_pointer<HRegex>( "\\b[a-zA-Z0-9]+_\\b" ) },
	{ "globals", make_pointer<HRegex>( "\\b_[a-zA-Z0-9]+_\\b" ) },
	{ "operators", make_pointer<HRegex>( "[\\+\\*/%\\^\\(\\){}\\-=<>\\[\\]!&:|@\\?\\.,;⋀⋁⊕¬≠≤≥∈∉]" ) },
	{ "escape", make_pointer<HRegex>( "(\\\\([\\\\abfnrtv\"']|x[a-fA-F0-9]{2,4}|u[a-fA-F0-9]{4}|U[a-fA-F0-9]{8}|[0-7]{1,3})|{:?[0-9]*})" ) },
	{ "keywords", make_pointer<HRegex>( "\\b(" + string::join( _keywords_, "|" ) + ")\\b" ) },
	{ "import", make_pointer<HRegex>( "\\b(" + string::join( _import_, "|" ) + ")\\b" ) },
	{ "builtins", make_pointer<HRegex>( "\\b(" + string::join( _builtins_, "|" ) + ")\\b|\\B(" + string::join( _builtinSymbols_, "|" ) + ")\\B" ) },
	{ "literals", make_pointer<HRegex>( "\\b(" + string::join( _literals_, "|" ) + ")\\b" ) },
	{ "switches", make_pointer<HRegex>( "(?<=\\s)--?\\b[a-zA-Z0-9-]+\\b" ) },
	{ "environment", make_pointer<HRegex>( "\\${\\b[a-zA-Z0-9]+\\b}" ) },
	{ "pipes", make_pointer<HRegex>( "[<>&|!;]" ) },
	{ "substitution", make_pointer<HRegex>( "[$<>]\\(|(?<!\\\\)\\)" ) }
};

class HColorizer {
public:
	enum class LANGUAGE {
		HUGINN,
		SHELL
	};
private:
	bool _inComment;
	bool _inSingleLineComment;
	bool _inLiteralString;
	bool _inLiteralChar;
	bool _wasInComment;
	bool _wasInSingleLineComment;
	bool _wasInLiteralString;
	bool _wasInLiteralChar;
	yaal::hcore::HUTF8String _source;
	colors_t& _colors;
	HShell const* _shell;
	LANGUAGE _language;
public:
	HColorizer( yaal::hcore::HUTF8String const& source_, colors_t& colors_, HShell const* shell_ = nullptr )
		: _inComment( false )
		, _inSingleLineComment( false )
		, _inLiteralString( false )
		, _inLiteralChar( false )
		, _wasInComment( false )
		, _wasInSingleLineComment( false )
		, _wasInLiteralString( false )
		, _wasInLiteralChar( false )
		, _source( source_ )
		, _colors( colors_ )
		, _shell( shell_ )
		, _language( shell_ ? LANGUAGE::SHELL : LANGUAGE::HUGINN ) {
		M_PROLOG
		_colors.resize( _source.character_count() );
		fill( _colors.begin(), _colors.end(), COLOR::ATTR_DEFAULT );
		return;
		M_EPILOG
	}
	void colorize( void );
private:
	void colorize_huginn( void );
	void colorize_shell( void );
	void paint( int, int, yaal::tools::COLOR::color_t );
	void paint( HRegex&, int, yaal::hcore::HUTF8String::const_iterator, yaal::hcore::HUTF8String::const_iterator, yaal::tools::COLOR::color_t );
	/*! \brief Colorize part of string if state of quotes/comments has changed.
	 */
	int colorize_buffer( int, yaal::hcore::HUTF8String::const_iterator, yaal::hcore::HUTF8String::const_iterator );
	void colorize_lines( int, yaal::hcore::HUTF8String::const_iterator, yaal::hcore::HUTF8String::const_iterator );
	void colorize_string( int, yaal::hcore::HUTF8String::const_iterator, yaal::hcore::HUTF8String::const_iterator );
	void colorize_words( int, yaal::hcore::HUTF8String::const_iterator, yaal::hcore::HUTF8String::const_iterator );
private:
	HColorizer( HColorizer const& ) = delete;
	HColorizer& operator = ( HColorizer const& ) = delete;
};

void HColorizer::paint( int start_, int len_, yaal::tools::COLOR::color_t color_ ) {
	M_PROLOG
	fill_n( _colors.begin() + start_, len_, color_ );
	return;
	M_EPILOG
}

void HColorizer::paint( HRegex& regex_, int offset_, yaal::hcore::HUTF8String::const_iterator it_, yaal::hcore::HUTF8String::const_iterator end_, yaal::tools::COLOR::color_t color_ ) {
	M_PROLOG
	int long len( end_ - it_ );
	for ( HRegex::HMatch const& m : regex_.matches( HUTF8String( it_, end_ ) ) ) {
		if ( m.start() >= len ) {
			break;
		}
		paint( offset_ + m.start(), m.size(), color_ );
	}
	return;
	M_EPILOG
}

void HColorizer::colorize_words( int offset_, yaal::hcore::HUTF8String::const_iterator begin_, yaal::hcore::HUTF8String::const_iterator end_ ) {
	M_PROLOG
	bool shellBreak( false );
	bool escape( false );
	yaal::hcore::HUTF8String::const_iterator head( begin_ );
	yaal::hcore::HUTF8String::const_iterator tail( begin_ );
	for ( ; head != end_; ++ head ) {
		if ( escape ) {
			escape = false;
			continue;
		}
		if ( *head == '\\' ) {
			escape = true;
			continue;
		}
		if (
			character_class<CHARACTER_CLASS::WHITESPACE>().has( *head )
			|| ( is_ascii( *head ) && strchr( "$<>&|();", static_cast<int>( (*head).get() ) ) )
		) {
			if ( shellBreak ) {
				paint( offset_ + static_cast<int>( tail - begin_ ), static_cast<int>( head - tail ), file_color( HUTF8String( tail, head ), static_cast<HSystemShell const*>( _shell ) ) );
			}
			shellBreak = false;
		} else {
			if ( ! shellBreak ) {
				tail = head;
			}
			shellBreak = true;
		}
	}
	if ( shellBreak ) {
		paint( offset_ + static_cast<int>( tail - begin_ ), static_cast<int>( head - tail ), file_color( HUTF8String( tail, head ), static_cast<HSystemShell const*>( _shell ) ) );
	}
	return;
	M_EPILOG
}

void HColorizer::colorize_lines( int offset_, yaal::hcore::HUTF8String::const_iterator it_, yaal::hcore::HUTF8String::const_iterator end_ ) {
	M_PROLOG
	if ( _language == LANGUAGE::HUGINN ) {
		paint( *_regex_.at( "operators" ), offset_, it_, end_, _scheme_->at( GROUP::OPERATORS ) );
		paint( *_regex_.at( "numbers" ), offset_, it_, end_, _scheme_->at( GROUP::LITERALS ) );
		paint( *_regex_.at( "keywords" ), offset_, it_, end_, _scheme_->at( GROUP::KEYWORDS ) );
		paint( *_regex_.at( "builtins" ), offset_, it_, end_, _scheme_->at( GROUP::BUILTINS ) );
		paint( *_regex_.at( "literals" ), offset_, it_, end_, _scheme_->at( GROUP::LITERALS ) );
		paint( *_regex_.at( "import" ), offset_, it_, end_, _scheme_->at( GROUP::IMPORT ) );
		paint( *_regex_.at( "classes" ), offset_, it_, end_, _scheme_->at( GROUP::CLASSES ) );
		paint( *_regex_.at( "enums" ), offset_, it_, end_, _scheme_->at( GROUP::ENUMS ) );
		paint( *_regex_.at( "fields" ), offset_, it_, end_, _scheme_->at( GROUP::FIELDS ) );
		paint( *_regex_.at( "arguments" ), offset_, it_, end_, _scheme_->at( GROUP::ARGUMENTS ) );
		paint( *_regex_.at( "globals" ), offset_, it_, end_, _scheme_->at( GROUP::GLOBALS ) );
	} else if ( _language == LANGUAGE::SHELL ) {
		colorize_words( offset_, it_, end_ );
		paint( *_regex_.at( "switches" ), offset_, it_, end_, _scheme_->at( GROUP::SWITCHES ) );
		paint( *_regex_.at( "pipes" ), offset_, it_, end_, _scheme_->at( GROUP::PIPES ) );
		paint( *_regex_.at( "substitution" ), offset_, it_, end_, _scheme_->at( GROUP::SUBSTITUTION ) );
	}
	return;
	M_EPILOG
}

void HColorizer::colorize_string( int offset_, yaal::hcore::HUTF8String::const_iterator it_, yaal::hcore::HUTF8String::const_iterator end_ ) {
	M_PROLOG
	paint( offset_, static_cast<int>( end_ - it_ ), _scheme_->at( GROUP::LITERALS ) );
	paint( *_regex_.at( "escape" ), offset_, it_, end_, _scheme_->at( GROUP::ESCAPE ) );
	paint( *_regex_.at( "environment" ), offset_, it_, end_, _scheme_->at( GROUP::ENV_STR ) );
	return;
	M_EPILOG
}

int HColorizer::colorize_buffer( int offset_, yaal::hcore::HUTF8String::const_iterator it_, yaal::hcore::HUTF8String::const_iterator end_ ) {
	M_PROLOG
	int len( 0 );
	if ( _inComment == ! _wasInComment ) {
		len = static_cast<int>( end_ - it_ );
		if ( _inComment ) {
			colorize_lines( offset_, it_, end_ - 2 );
			len -= 2;
		} else {
			paint( offset_, len, _scheme_->at( GROUP::COMMENTS ) );
		}
	} else if ( _inSingleLineComment == ! _wasInSingleLineComment ) {
		len = static_cast<int>( end_ - it_ );
		if ( _inSingleLineComment ) {
			int backtrack( _shell ? 1 : 2 );
			colorize_lines( offset_, it_, end_ - backtrack );
			len -= backtrack;
		} else {
			paint( offset_, len, _scheme_->at( GROUP::COMMENTS ) );
		}
	}
	if ( _inLiteralString == ! _wasInLiteralString ) {
		len = static_cast<int>( end_ - it_ );
		if ( _inLiteralString ) {
			colorize_lines( offset_, it_, end_ - 1 );
			-- len;
		} else {
			colorize_string( offset_, it_, end_ );
		}
	} else if ( _inLiteralChar == ! _wasInLiteralChar ) {
		len = static_cast<int>( end_ - it_ );
		if ( _inLiteralChar ) {
			colorize_lines( offset_, it_, end_ - 1 );
			-- len;
		} else {
			colorize_string( offset_, it_, end_ );
		}
	}
	return ( len );
	M_EPILOG
}

void HColorizer::colorize( void ) {
	M_PROLOG
	switch ( _language ) {
		case ( LANGUAGE::HUGINN ): colorize_huginn(); break;
		case ( LANGUAGE::SHELL ):  colorize_shell();  break;
	}
	return;
	M_EPILOG
}

void HColorizer::colorize_huginn( void ) {
	M_PROLOG
	bool commentFirst( false );
	bool escape( false );
	HUTF8String::const_iterator it( _source.begin() );
	HUTF8String::const_iterator end( _source.begin() );
	int offset( 0 );
	for ( code_point_t c : _source ) {
		++ end;
		if ( escape ) {
			escape = false;
			continue;
		}
		if ( c == '\\' ) {
			escape = true;
			continue;
		}
		if ( ! ( _inComment || _inSingleLineComment || _inLiteralString || _inLiteralChar || commentFirst ) ) {
			if ( c == '"' ) {
				_inLiteralString = true;
			} else if ( c == '\'' ) {
				_inLiteralChar = true;
			} else if ( c == '/' ) {
				commentFirst = true;
				continue;
			}
		} else if ( commentFirst && ( c == '*' ) ) {
			_inComment = true;
		} else if ( ! escape && ( _inComment || _inLiteralString || _inLiteralChar ) ) {
			if ( _inLiteralString && ( c == '"' ) ) {
				_inLiteralString = false;
			} else if ( _inLiteralChar && ( c == '\'' ) ) {
				_inLiteralChar = false;
			} else if ( commentFirst && ( c == '/' ) ) {
				_inComment = false;
			} else if ( _inComment && ( c == '*' ) ) {
				commentFirst = true;
				continue;
			}
		} else if ( _inSingleLineComment && ( c == '\n' ) ) {
			_inSingleLineComment = false;
		} else if ( commentFirst && ( c == '/' ) ) {
			_inSingleLineComment = true;
		}
		int o( colorize_buffer( offset, it, end ) );
		it += o;
		offset += o;
		_wasInComment = _inComment;
		_wasInSingleLineComment = _inSingleLineComment;
		_wasInLiteralString = _inLiteralString;
		_wasInLiteralChar = _inLiteralChar;
		commentFirst = false;
	}
	_inComment = false;
	_inSingleLineComment = false;
	_inLiteralString = false;
	_inLiteralChar = false;
	int o( colorize_buffer( offset, it, _source.end() ) );
	it += o;
	offset += o;
	if ( offset != _source.character_count() ) {
		colorize_lines( offset, it, _source.end() );
	}
	return;
	M_EPILOG
}

void HColorizer::colorize_shell( void ) {
	M_PROLOG
	bool escape( false );
	HUTF8String::const_iterator it( _source.begin() );
	HUTF8String::const_iterator end( _source.begin() );
	int offset( 0 );
	for ( code_point_t c : _source ) {
		++ end;
		if ( escape ) {
			escape = false;
			continue;
		}
		if ( c == '\\' ) {
			escape = true;
			continue;
		}
		if ( ! ( _inSingleLineComment || _inLiteralString || _inLiteralChar ) ) {
			if ( c == '"' ) {
				_inLiteralString = true;
			} else if ( c == '\'' ) {
				_inLiteralChar = true;
			} else if ( c == '#' ) {
				_inSingleLineComment = true;
			}
		} else if ( ! escape && ( _inLiteralString || _inLiteralChar ) ) {
			if ( _inLiteralString && ( c == '"' ) ) {
				_inLiteralString = false;
			} else if ( _inLiteralChar && ( c == '\'' ) ) {
				_inLiteralChar = false;
			}
		} else if ( _inSingleLineComment && ( c == '\n' ) ) {
			_inSingleLineComment = false;
		}
		int o( colorize_buffer( offset, it, end ) );
		it += o;
		offset += o;
		_wasInSingleLineComment = _inSingleLineComment;
		_wasInLiteralString = _inLiteralString;
		_wasInLiteralChar = _inLiteralChar;
	}
	_inSingleLineComment = false;
	_inLiteralString = false;
	_inLiteralChar = false;
	int o( colorize_buffer( offset, it, _source.end() ) );
	it += o;
	offset += o;
	if ( offset != _source.character_count() ) {
		colorize_lines( offset, it, _source.end() );
	}
	return;
	M_EPILOG
}

}

void colorize( yaal::hcore::HString const& source_, colors_t& colors_, HShell const* shell_ ) {
	M_PROLOG
	static int const TIME_LEN( sizeof ( "//time" ) - 1 );
	HRegex time( "^\\s*//time[0-9]*\\s" );
	if ( ! shell_ && time.matches( source_ ) ) {
		HString::size_type start( source_.find_other_than( character_class<CHARACTER_CLASS::WHITESPACE>().data() ) );
		HString::size_type pos( source_.find_one_of( character_class<CHARACTER_CLASS::WHITESPACE>().data(), start ) );
		if ( pos == HString::npos ) {
			return;
		}
		HString source( source_ );
		source.shift_left( pos + 1 );
		HColorizer colorizer( source, colors_, shell_ );
		colorizer.colorize();
		COLOR::color_t comment( _scheme_->at( GROUP::COMMENTS ) );
		COLOR::color_t literal( _scheme_->at( GROUP::LITERALS ) );
		colors_.insert( colors_.begin(), pos - start - TIME_LEN + 1, literal );
		colors_.insert( colors_.begin(), TIME_LEN + start, comment );
	} else {
		HColorizer colorizer( source_, colors_, shell_ );
		colorizer.colorize();
	}
	return;
	M_EPILOG
}

yaal::hcore::HString colorize( yaal::hcore::HString const& source_, HShell const* shell_ ) {
	M_PROLOG
	if ( setup._noColor ) {
		return ( source_ );
	}
	colors_t colors;
	colorize( source_, colors, shell_ );
	M_ASSERT( colors.get_size() == source_.get_length() );
	HString colorized;
	COLOR::color_t col( COLOR::ATTR_DEFAULT );
	int i( 0 );
	for ( code_point_t c : source_ ) {
		if ( colors[i] != col ) {
			col = colors[i];
			colorized.append( *COLOR::to_ansi( col ) );
		}
		colorized.push_back( c );
		++ i;
	}
	colorized.append( *ansi::reset );
	return ( colorized );
	M_EPILOG
}

yaal::hcore::HString colorize( yaal::tools::HHuginn::HCallSite const& callSite_ ) {
	M_PROLOG
	HString s;
	s
		.append( *ansi::magenta ).append( callSite_.file() )
		.append( *ansi::cyan ).append( ":" )
		.append( *ansi::green ).append( callSite_.line() )
		.append( *ansi::cyan ).append( ":" )
		.append( *ansi::green ).append( callSite_.column() )
		.append( *ansi::cyan ).append( ": " )
		.append( *ansi::reset ).append( callSite_.context() );
	return ( s );
	M_EPILOG
}

yaal::hcore::HString colorize_error( yaal::hcore::HString const& errorMessage_ ) {
	M_PROLOG
	string::tokens_t t( string::split( errorMessage_, ":" ) );
	HString s;
	if ( t.get_size() > 3 ) {
		s
			.append( *ansi::magenta ).append( t[0] )
			.append( *ansi::cyan ).append( ":" )
			.append( *ansi::green ).append( t[1] )
			.append( *ansi::cyan ).append( ":" )
			.append( *ansi::green ).append( t[2] )
			.append( *ansi::cyan ).append( ":" );
		t.erase( t.begin(), t.begin() + 3 );
		s.append( *ansi::brightred ).append( string::join( t, ":" ) ).append( *ansi::reset );
	} else {
		s.assign( errorMessage_ );
	}
	return ( s );
	M_EPILOG
}

hcore::HString colorize( HHuginn::value_t const& value_, HHuginn* huginn_ ) {
	M_PROLOG
	hcore::HString res;
	hcore::HString strRes( code( value_, huginn_ ) );
	if ( ! setup._noColor ) {
		switch ( value_->type_id().get() ) {
			case ( static_cast<int>( HHuginn::TYPE::INTEGER ) ):
			case ( static_cast<int>( HHuginn::TYPE::BOOLEAN ) ):
			case ( static_cast<int>( HHuginn::TYPE::CHARACTER ) ):
			case ( static_cast<int>( HHuginn::TYPE::REAL ) ):
			case ( static_cast<int>( HHuginn::TYPE::NUMBER ) ):
			case ( static_cast<int>( HHuginn::TYPE::STRING ) ):
			case ( static_cast<int>( HHuginn::TYPE::NONE ) ): {
				res.append( ansi_color( GROUP::LITERALS ) );
			} break;
			case ( static_cast<int>( HHuginn::TYPE::FUNCTION_REFERENCE ) ): {
				if ( tools::huginn::is_builtin( strRes ) ) {
					res.append( ansi_color( GROUP::BUILTINS ) );
				} else if ( strRes == "Exception" ) {
					res.append( ansi_color( GROUP::CLASSES ) );
				}
			} break;
			default: {
				strRes = ::huginn::colorize( strRes );
			}
		}
	}
	res.append( strRes );
	if ( ! setup._noColor ) {
		res.append( *ansi::reset );
	}
	return ( res );
	M_EPILOG
}

void set_color_scheme( yaal::hcore::HString const& colorScheme_ ) {
	_scheme_ = _schemes_.at( colorScheme_ );
	setup._colorScheme = colorScheme_;
}

yaal::tools::COLOR::color_t file_color( yaal::tools::filesystem::path_t&& path_, HSystemShell const* shell_, yaal::tools::COLOR::color_t defaultColor_ ) {
	COLOR::color_t c( defaultColor_ );
	denormalize_path( path_, true );
	path_ = unescape_system( yaal::move( path_ ) );
	try {
		filesystem::FILE_TYPE ft( filesystem::file_type( path_ ) );
		switch ( ft ) {
			case ( FILE_TYPE::SYMBOLIC_LINK ):    c = color( GROUP::SYMBOLIC_LINKS ); break;
			case ( FILE_TYPE::DIRECTORY ):        c = color( GROUP::DIRECTORIES );    break;
			case ( FILE_TYPE::FIFO ):             c = color( GROUP::FIFOS );          break;
			case ( FILE_TYPE::BLOCK_DEVICE ):     c = color( GROUP::DEVICES );        break;
			case ( FILE_TYPE::CHARACTER_DEVICE ): c = color( GROUP::DEVICES );        break;
			case ( FILE_TYPE::SOCKET ):           c = color( GROUP::SOCKETS );        break;
			case ( FILE_TYPE::REGULAR ): {
				char const* packers[] = { ".gz", ".tar", ".tgz", ".zip", ".rar", ".7z", ".bz2", ".ace", ".jar", ".deb", ".rpm", nullptr };
				char const* media[] = { ".jpg", ".jpeg", ".png", ".gif", ".xpm", ".svg", ".mpg", ".mpeg", ".mp4", ".avi", ".mkv", ".mp3", ".rm", nullptr };
				COLOR::color_t cs[] = { color( GROUP::ARCHIVES ), color( GROUP::MEDIA ) };
				char const** extSets[] = { packers, media };
				int ci( 0 );
				for ( char const** extSet : extSets ) {
					for ( int i( 0 ); extSet[i] ; ++ i ) {
						if ( path_.ends_with( extSet[i] ) ) {
							c = cs[ci];
							break;
						}
					}
					if ( c != COLOR::ATTR_DEFAULT ) {
						break;
					}
					++ ci;
				}
				if ( ! HFSItem( path_ ).is_executable() ) {
					break;
				}
				if ( is_suid( path_ ) ) {
					c = color( GROUP::SUID );
				} else {
					c = color( GROUP::EXECUTABLES );
				}
			} break;
		}
	} catch ( ... ) {
		HSystemShell::system_commands_t::const_iterator it( shell_->system_commands().find( path_ ) );
		if ( it != shell_->system_commands().end() ) {
			c = color( is_suid( it->second + filesystem::path::SEPARATOR + path_ ) ? GROUP::SUID : GROUP::EXECUTABLES );
		} else if ( shell_->builtins().count( path_ ) > 0 ) {
			c = color( GROUP::SHELL_BUILTINS );
		} else if ( shell_->aliases().count( path_ ) > 0 ) {
			c = color( GROUP::ALIASES );
		}
	}
	return ( c );
}

yaal::tools::COLOR::color_t color( GROUP group_ ) {
	return ( _scheme_->at( group_ ) );
}

char const* ansi_color( GROUP group_ ) {
	return ( *COLOR::to_ansi( color( group_ ) ) );
}

}

