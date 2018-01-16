/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hhashmap.hxx>
#include <yaal/hcore/hregex.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/ansi.hxx>
M_VCSID( "$Id: " __ID__ " $" )
#include "colorize.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;

namespace huginn {

namespace {

string::tokens_t _keywords_ = {
	"assert", "break", "case", "catch", "class", "constructor", "default", "destructor", "else", "for",
	"if", "return", "super", "switch", "throw", "try", "while", "this"
};
string::tokens_t _builtins_ = {
	"boolean", "character", "copy", "deque", "dict", "integer", "list", "lookup",
	"number", "observe", "order", "real", "set", "size", "string", "tuple", "type", "use"
};
string::tokens_t _literals_ = { "false", "none", "true" };
string::tokens_t _import_ = { "import", "as" };

scheme_t const _schemeDarkBG_ = {
	{ "keywords", COLOR::FG_YELLOW },
	{ "builtins", COLOR::FG_BRIGHTGREEN },
	{ "classes", COLOR::FG_BROWN },
	{ "fields", COLOR::FG_BRIGHTBLUE },
	{ "arguments", COLOR::FG_GREEN },
	{ "literals", COLOR::FG_BRIGHTMAGENTA },
	{ "comments", COLOR::FG_BRIGHTCYAN },
	{ "import", COLOR::FG_BRIGHTBLUE },
	{ "operators", COLOR::FG_WHITE },
	{ "escape", COLOR::FG_BRIGHTRED }
};

scheme_t const _schemeBrightBG_ = {
	{ "keywords", COLOR::FG_BROWN },
	{ "builtins", COLOR::FG_GREEN },
	{ "classes", COLOR::FG_BRIGHTRED },
	{ "fields", COLOR::FG_BLUE },
	{ "arguments", COLOR::FG_BRIGHTGREEN },
	{ "literals", COLOR::FG_MAGENTA },
	{ "comments", COLOR::FG_CYAN },
	{ "import", COLOR::FG_BLUE },
	{ "operators", COLOR::FG_BLACK },
	{ "escape", COLOR::FG_RED }
};

scheme_t const* _scheme_( &_schemeDarkBG_ );

typedef yaal::hcore::HPointer<yaal::hcore::HRegex> regex_t;
typedef yaal::hcore::HHashMap<yaal::hcore::HString, regex_t> matchers_t;
matchers_t _regex_ = {
	{ "numbers", make_pointer<HRegex>( "(\\$?\\b[0-9]+\\.|\\$?\\.[0-9]+|\\$?\\b[0-9]+\\.[0-9]+|\\b0[bB][01]+|\\b0[oO]?[0-7]+|\\b0[xX][0-9a-fA-F]+|\\$?\\b[0-9]+)\\b" ) },
	{ "classes", make_pointer<HRegex>( "\\b[A-Z][a-zA-Z]*\\b" ) },
	{ "fields", make_pointer<HRegex>( "\\b_[a-zA-Z0-9]+\\b" ) },
	{ "arguments", make_pointer<HRegex>( "\\b[a-zA-Z0-9]+_\\b\\b" ) },
	{ "operators", make_pointer<HRegex>( "[\\+\\*/%\\^\\(\\){}\\-=<>\\[\\]!&:|@\\?\\.,;⋀⋁⊕]" ) },
	{ "escape", make_pointer<HRegex>( "(\\\\([\\\\abfnrtv\"']|x[a-fA-F0-9]{2,4}|u[a-fA-F0-9]{4}|U[a-fA-F0-9]{8}|[0-7]{1,3})|{:?[0-9]*})" ) },
	{ "keywords", make_pointer<HRegex>( "\\b(" + string::join( _keywords_, "|" ) + ")\\b" ) },
	{ "import", make_pointer<HRegex>( "\\b(" + string::join( _import_, "|" ) + ")\\b" ) },
	{ "builtins", make_pointer<HRegex>( "\\b(" + string::join( _builtins_, "|" ) + ")\\b" ) },
	{ "literals", make_pointer<HRegex>( "\\b(" + string::join( _literals_, "|" ) + ")\\b" ) }
};

class HColorizer {
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
public:
	HColorizer( yaal::hcore::HUTF8String const& source_, colors_t& colors_ )
		: _inComment( false )
		, _inSingleLineComment( false )
		, _inLiteralString( false )
		, _inLiteralChar( false )
		, _wasInComment( false )
		, _wasInSingleLineComment( false )
		, _wasInLiteralString( false )
		, _wasInLiteralChar( false )
		, _source( source_ )
		, _colors( colors_ ) {
		M_PROLOG
		_colors.resize( _source.character_count() );
		fill( _colors.begin(), _colors.end(), COLOR::ATTR_DEFAULT );
		return;
		M_EPILOG
	}
	void colorize( void );
private:
	void paint( int, int, yaal::tools::COLOR::color_t );
	void paint( HRegex&, int, yaal::hcore::HUTF8String::const_iterator, yaal::hcore::HUTF8String::const_iterator, yaal::tools::COLOR::color_t );
	int colorizeBuffer( int, yaal::hcore::HUTF8String::const_iterator, yaal::hcore::HUTF8String::const_iterator );
	void colorizeLines( int, yaal::hcore::HUTF8String::const_iterator, yaal::hcore::HUTF8String::const_iterator );
	void colorizeString( int, yaal::hcore::HUTF8String::const_iterator, yaal::hcore::HUTF8String::const_iterator );
};

void HColorizer::paint( int start_, int len_, yaal::tools::COLOR::color_t color_ ) {
	M_PROLOG
	fill_n( _colors.begin() + start_, len_, color_ );
	return;
	M_EPILOG
}

void HColorizer::paint( HRegex& regex_, int offset_, yaal::hcore::HUTF8String::const_iterator it_, yaal::hcore::HUTF8String::const_iterator end_, yaal::tools::COLOR::color_t color_ ) {
	M_PROLOG
	for ( HRegex::HMatch const& m : regex_.matches( HUTF8String( it_, end_ ) ) ) {
		if ( m.start() >= ( end_ - it_ ) ) {
			break;
		}
		paint( offset_ + m.start(), m.size(), color_ );
	}
	return;
	M_EPILOG
}

void HColorizer::colorizeLines( int offset_, yaal::hcore::HUTF8String::const_iterator it_, yaal::hcore::HUTF8String::const_iterator end_ ) {
	M_PROLOG
	paint( *_regex_.at( "operators" ), offset_, it_, end_, _scheme_->at( "operators" ) );
	paint( *_regex_.at( "numbers" ), offset_, it_, end_, _scheme_->at( "literals" ) );
	paint( *_regex_.at( "keywords" ), offset_, it_, end_, _scheme_->at( "keywords" ) );
	paint( *_regex_.at( "builtins" ), offset_, it_, end_, _scheme_->at( "builtins" ) );
	paint( *_regex_.at( "literals" ), offset_, it_, end_, _scheme_->at( "literals" ) );
	paint( *_regex_.at( "import" ), offset_, it_, end_, _scheme_->at( "import" ) );
	paint( *_regex_.at( "classes" ), offset_, it_, end_, _scheme_->at( "classes" ) );
	paint( *_regex_.at( "fields" ), offset_, it_, end_, _scheme_->at( "fields" ) );
	paint( *_regex_.at( "arguments" ), offset_, it_, end_, _scheme_->at( "arguments" ) );
	return;
	M_EPILOG
}

void HColorizer::colorizeString( int offset_, yaal::hcore::HUTF8String::const_iterator it_, yaal::hcore::HUTF8String::const_iterator end_ ) {
	M_PROLOG
	paint( offset_, static_cast<int>( end_ - it_ ), _scheme_->at( "literals" ) );
	paint( *_regex_.at( "escape" ), offset_, it_, end_, _scheme_->at( "escape" ) );
	return;
	M_EPILOG
}

int HColorizer::colorizeBuffer( int offset_, yaal::hcore::HUTF8String::const_iterator it_, yaal::hcore::HUTF8String::const_iterator end_ ) {
	M_PROLOG
	int len( 0 );
	if ( _inComment == ! _wasInComment ) {
		len = static_cast<int>( end_ - it_ );
		if ( _inComment ) {
			colorizeLines( offset_, it_, end_ - 2 );
			len -= 2;
		} else {
			paint( offset_, len, _scheme_->at( "comments" ) );
		}
	} else if ( _inSingleLineComment == ! _wasInSingleLineComment ) {
		len = static_cast<int>( end_ - it_ );
		if ( _inSingleLineComment ) {
			colorizeLines( offset_, it_, end_ - 2 );
			len -= 2;
		} else {
			paint( offset_, len, _scheme_->at( "comments" ) );
		}
	}
	if ( _inLiteralString == ! _wasInLiteralString ) {
		len = static_cast<int>( end_ - it_ );
		if ( _inLiteralString ) {
			colorizeLines( offset_, it_, end_ - 1 );
			-- len;
		} else {
			colorizeString( offset_, it_, end_ );
		}
	} else if ( _inLiteralChar == ! _wasInLiteralChar ) {
		len = static_cast<int>( end_ - it_ );
		if ( _inLiteralChar ) {
			colorizeLines( offset_, it_, end_ - 1 );
			-- len;
		} else {
			colorizeString( offset_, it_, end_ );
		}
	}
	return ( len );
	M_EPILOG
}

void HColorizer::colorize( void ) {
	M_PROLOG
	bool commentFirst( false );
	bool escape( false );
	HUTF8String::const_iterator it( _source.begin() );
	HUTF8String::const_iterator end( _source.begin() );
	int offset( 0 );
	for ( code_point_t c : _source ) {
		++ end;
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
			} else if ( c == '\\' ) {
				escape = true;
				continue;
			}
		} else if ( _inSingleLineComment && ( c == '\n' ) ) {
			_inSingleLineComment = false;
		} else if ( commentFirst && ( c == '/' ) ) {
			_inSingleLineComment = true;
		}
		int o( colorizeBuffer( offset, it, end ) );
		it += o;
		offset += o;
		_wasInComment = _inComment;
		_wasInSingleLineComment = _inSingleLineComment;
		_wasInLiteralString = _inLiteralString;
		_wasInLiteralChar = _inLiteralChar;
		commentFirst = false;
		escape = false;
	}
	_inComment = false;
	_inSingleLineComment = false;
	_inLiteralString = false;
	_inLiteralChar = false;
	int o( colorizeBuffer( offset, it, _source.end() ) );
	it += o;
	offset += o;
	if ( offset != _source.character_count() ) {
		colorizeLines( offset, it, _source.end() );
	}
	return;
	M_EPILOG
}

}

void colorize( yaal::hcore::HUTF8String const& source_, colors_t& colors_ ) {
	M_PROLOG
	HColorizer colorizer( source_, colors_ );
	colorizer.colorize();
	return;
	M_EPILOG
}

yaal::hcore::HString colorize( yaal::hcore::HUTF8String const& source_ ) {
	M_PROLOG
	colors_t colors;
	colorize( source_, colors );
	M_ASSERT( colors.get_size() == source_.character_count() );
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

void set_color_scheme( BACKGROUND background_ ) {
	_scheme_ = background_ == BACKGROUND::DARK ? &_schemeDarkBG_ : &_schemeBrightBG_;
}

}

