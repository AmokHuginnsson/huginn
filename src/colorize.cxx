/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

	colorize.cxx - this file is integral part of `huginn' project.

  i.  You may not make any changes in Copyright information.
  ii. You must attach Copyright information to any part of every copy
      of this software.

Copyright:

 You can use this software free of charge and you can redistribute its binary
 package freely but:
  1. You are not allowed to use any part of sources of this software.
  2. You are not allowed to redistribute any part of sources of this software.
  3. You are not allowed to reverse engineer this software.
  4. If you want to distribute a binary package of this software you cannot
     demand any fees for it. You cannot even demand
     a return of cost of the media or distribution (CD for example).
  5. You cannot involve this software in any commercial activity (for example
     as a free add-on to paid software or newspaper).
 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. Use it at your own risk.
*/

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
using namespace yaal::hconsole;

namespace huginn {

namespace {

string::tokens_t _keywords_ = {
	"assert", "break", "case", "catch", "class", "constructor", "default", "destructor", "else", "for",
	"if", "return", "super", "switch", "throw", "try", "while", "this"
};
string::tokens_t _builtins_ = {
	"boolean", "character", "copy", "deque", "dict", "integer", "list", "lookup",
	"number", "observe", "order", "real", "set", "size", "string", "type", "use"
};
string::tokens_t _literals_ = { "false", "none", "true" };
string::tokens_t _import_ = { "import", "as" };

typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::hconsole::COLOR::color_t> scheme_t;
scheme_t _scheme_ = {
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

typedef yaal::hcore::HPointer<yaal::hcore::HRegex> regex_t;
typedef yaal::hcore::HHashMap<yaal::hcore::HString, regex_t> matchers_t;
matchers_t _regex_ = {
	{ "numbers", make_pointer<HRegex>( "(\\$?\\b[0-9]+\\.|\\$?\\,[0-9]+|\\$?\\b[0-9]+\\.[0-9]+|\\b0[bB][01]+|\\b0[oO]?[0-7]+|\\b0[xX][0-9a-fA-F]+|\\$?\\b[0-9]+)\\b" ) },
	{ "classes", make_pointer<HRegex>( "\\b[A-Z][a-zA-Z]*\\b" ) },
	{ "fields", make_pointer<HRegex>( "\\b_[a-zA-Z0-9]+\\b" ) },
	{ "arguments", make_pointer<HRegex>( "\\b[a-zA-Z0-9]+_\\b\\b" ) },
	{ "operators", make_pointer<HRegex>( "[\\+\\*/%\\^\\(\\){}\\-=<>\\[\\]!&:|@\\?\\.,;]" ) },
	{ "escape", make_pointer<HRegex>( "(\\\\.|{:?[0-9]*})" ) },
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
	yaal::hcore::HString const& _source;
	colors_t& _colors;
public:
	HColorizer( yaal::hcore::HString const& source_, colors_t& colors_ )
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
		_colors.resize( _source.get_length() );
		fill( _colors.begin(), _colors.end(), COLOR::ATTR_DEFAULT );
		return;
		M_EPILOG
	}
	void colorize( void );
private:
	void paint( yaal::hcore::HString::const_iterator, yaal::hcore::HString::const_iterator, yaal::hconsole::COLOR::color_t );
	void paint( HRegex&, yaal::hcore::HString::const_iterator, yaal::hcore::HString::const_iterator, yaal::hconsole::COLOR::color_t );
	yaal::hcore::HString::const_iterator colorizeBuffer( yaal::hcore::HString::const_iterator, yaal::hcore::HString::const_iterator );
	void colorizeLines( yaal::hcore::HString::const_iterator, yaal::hcore::HString::const_iterator );
	void colorizeString( yaal::hcore::HString::const_iterator, yaal::hcore::HString::const_iterator );
};

void HColorizer::paint( yaal::hcore::HString::const_iterator it_, yaal::hcore::HString::const_iterator end_, yaal::hconsole::COLOR::color_t color_ ) {
	M_PROLOG
//	clog << "from " << ( it_ - _source.begin() ) << " len " << ( end_ - it_ ) << " col " << static_cast<int>( color_ ) << endl;
	fill_n( _colors.begin() + ( it_ - _source.begin() ), end_ - it_, color_ );
	return;
	M_EPILOG
}

void HColorizer::paint( HRegex& regex_, yaal::hcore::HString::const_iterator it_, yaal::hcore::HString::const_iterator end_, yaal::hconsole::COLOR::color_t color_ ) {
	M_PROLOG
	for ( HRegex::HMatch const& m : regex_.matches( it_ ) ) {
		if ( m.start() >= ( end_ - it_ ) ) {
			break;
		}
		paint( it_ + m.start(), it_ + m.start() + m.size(), color_ );
	}
	return;
	M_EPILOG
}

void HColorizer::colorizeLines( yaal::hcore::HString::const_iterator it_, yaal::hcore::HString::const_iterator end_ ) {
	M_PROLOG
	paint( *_regex_.at( "operators" ), it_, end_, _scheme_.at( "operators" ) );
	paint( *_regex_.at( "numbers" ), it_, end_, _scheme_.at( "literals" ) );
	paint( *_regex_.at( "keywords" ), it_, end_, _scheme_.at( "keywords" ) );
	paint( *_regex_.at( "builtins" ), it_, end_, _scheme_.at( "builtins" ) );
	paint( *_regex_.at( "literals" ), it_, end_, _scheme_.at( "literals" ) );
	paint( *_regex_.at( "import" ), it_, end_, _scheme_.at( "import" ) );
	paint( *_regex_.at( "classes" ), it_, end_, _scheme_.at( "classes" ) );
	paint( *_regex_.at( "fields" ), it_, end_, _scheme_.at( "fields" ) );
	paint( *_regex_.at( "arguments" ), it_, end_, _scheme_.at( "arguments" ) );
	return;
	M_EPILOG
}

void HColorizer::colorizeString( yaal::hcore::HString::const_iterator it_, yaal::hcore::HString::const_iterator end_ ) {
	M_PROLOG
	paint( it_, end_, _scheme_.at( "literals" ) );
	paint( *_regex_.at( "escape" ), it_, end_, _scheme_.at( "escape" ) );
	return;
	M_EPILOG
}

yaal::hcore::HString::const_iterator  HColorizer::colorizeBuffer( yaal::hcore::HString::const_iterator it_, yaal::hcore::HString::const_iterator end_ ) {
	M_PROLOG
	if ( _inComment == ! _wasInComment ) {
		if ( _inComment ) {
			colorizeLines( it_, end_ - 2 );
			it_ = end_ - 2;
		} else {
			paint( it_, end_, _scheme_.at( "comments" ) );
			it_ = end_;
		}
	} else if ( _inSingleLineComment == ! _wasInSingleLineComment ) {
		if ( _inSingleLineComment ) {
			colorizeLines( it_, end_ - 2 );
			it_ = end_ - 2;
		} else {
			paint( it_, end_, _scheme_.at( "comments" ) );
			it_ = end_;
		}
	}
	if ( _inLiteralString == ! _wasInLiteralString ) {
		if ( _inLiteralString ) {
			colorizeLines( it_, end_ - 1 );
			it_ = end_ - 1;
		} else {
			colorizeString( it_, end_ );
			it_ = end_;
		}
	} else if ( _inLiteralChar == ! _wasInLiteralChar ) {
		if ( _inLiteralChar ) {
			colorizeLines( it_, end_ - 1 );
			it_ = end_ - 1;
		} else {
			colorizeString( it_, end_ );
			it_ = end_;
		}
	}
	return ( it_ );
	M_EPILOG
}

void HColorizer::colorize( void ) {
	M_PROLOG
	bool commentFirst( false );
	bool escape( false );
	HString::const_iterator it( _source.begin() );
	HString::const_iterator end( _source.begin() );
	for ( char c : _source ) {
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
		it = colorizeBuffer( it, end );
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
	it = colorizeBuffer( it, _source.end() );
	if ( it != _source.end() ) {
		colorizeLines( it, _source.end() );
	}
	return;
	M_EPILOG
}

}

void colorize( yaal::hcore::HString const& source_, colors_t& colors_ ) {
	M_PROLOG
	HColorizer colorizer( source_, colors_ );
	colorizer.colorize();
	return;
	M_EPILOG
}

yaal::hcore::HString colorize( yaal::hcore::HString const& source_ ) {
	M_PROLOG
	colors_t colors;
	colorize( source_, colors );
	M_ASSERT( colors.get_size() == source_.get_length() );
	HString colorized;
	COLOR::color_t col( COLOR::ATTR_DEFAULT );
	int i( 0 );
	for ( char c : source_ ) {
		if ( colors[i] != col ) {
			col = colors[i];
			colorized.append( *COLOR::to_ansi( col ) );
		}
		colorized.append( c );
		++ i;
	}
	colorized.append( *ansi::reset );
	return ( colorized );
	M_EPILOG
}

}

