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
	"number", "observe", "order", "real", "set", "size", "string", "type", "use"
};
string::tokens_t _literals_ = { "false", "none", "true" };
string::tokens_t _import_ = { "import", "as" };

typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::ansi::HSequence const&> scheme_t;
scheme_t _scheme_ = {
	{ "preprocessors", ansi::brightblue },
	{ "keywords", ansi::yellow },
	{ "builtins", ansi::brightgreen },
	{ "classes", ansi::brown },
	{ "fields", ansi::brightblue },
	{ "arguments", ansi::green },
	{ "literals", ansi::brightmagenta },
	{ "comments", ansi::brightcyan },
	{ "import", ansi::brightblue },
	{ "operators", ansi::white },
	{ "escape", ansi::brightred }
};

typedef yaal::hcore::HPointer<yaal::hcore::HRegex> regex_t;
typedef yaal::hcore::HHashMap<yaal::hcore::HString, regex_t> matchers_t;
matchers_t _regex_ = {
	{ "numbers", make_pointer<HRegex>( "(\\$?\\b[0-9]+\\.|\\$?\\,[0-9]+|\\$?\\b[0-9]+\\.[0-9]+|\\b0[bB][01]+|\\b0[oO]?[0-7]+|\\b0[xX][0-9a-fA-F]+|\\$?\\b[0-9]+)\\b" ) },
	{ "classes", make_pointer<HRegex>( "\\b[A-Z][a-zA-Z]*\\b" ) },
	{ "fields", make_pointer<HRegex>( "\\b_[a-zA-Z0-9]+\\b" ) },
	{ "arguments", make_pointer<HRegex>( "\\b[a-zA-Z0-9]+_\\b\\b" ) },
	{ "operators", make_pointer<HRegex>( "[\\+\\*/%\\^\\(\\){}\\-=<>\\]&:|@\\?\\.,]" ) },
	{ "escape", make_pointer<HRegex>( "(\\\\.|{:?[0-9]*})" ) },
	{ "keywords", make_pointer<HRegex>( "\\b(" + string::join( _keywords_, "|" ) + ")\\b" ) },
	{ "import", make_pointer<HRegex>( "\\b(" + string::join( _import_, "|" ) + ")\\b" ) },
	{ "builtins", make_pointer<HRegex>( "\\b(" + string::join( _builtins_, "|" ) + ")\\b" ) },
	{ "literals", make_pointer<HRegex>( "\\b(" + string::join( _literals_, "|" ) + ")\\b" ) }
};

yaal::hcore::HString replacer( yaal::hcore::HString const& scheme_, yaal::hcore::HString const& match_ ) {
	return ( scheme_ + match_ + *ansi::reset );
}

yaal::hcore::HString colorizeLines( yaal::hcore::HString const& lines_ ) {
	string::tokens_t lines( string::split<>( lines_, "\n" ) );
	HString output = "";
	for ( yaal::hcore::HString line : lines ) {
		line = _regex_.at( "numbers" )->replace( line, call( &replacer, *_scheme_.at( "literals" ), _1 ) );
		line = _regex_.at( "keywords" )->replace( line, call( &replacer, *_scheme_.at( "keywords" ), _1 ) );
		line = _regex_.at( "builtins" )->replace( line, call( &replacer, *_scheme_.at( "builtins" ), _1 ) );
		line = _regex_.at( "literals" )->replace( line, call( &replacer, *_scheme_.at( "literals" ), _1 ) );
		line = _regex_.at( "import" )->replace( line, call( &replacer, *_scheme_.at( "import" ), _1 ) );
		line = _regex_.at( "classes" )->replace( line, call( &replacer, *_scheme_.at( "classes" ), _1 ) );
		line = _regex_.at( "fields" )->replace( line, call( &replacer, *_scheme_.at( "fields" ), _1 ) );
		line = _regex_.at( "arguments" )->replace( line, call( &replacer, *_scheme_.at( "arguments" ), _1 ) );
		line = _regex_.at( "operators" )->replace( line, call( &replacer, *_scheme_.at( "operators" ), _1 ) );
		output += line;
		output.push_back( '\n' );
	}
	output.pop_back();
	return ( output );
}

yaal::hcore::HString colorizeString( yaal::hcore::HString const& string_ ) {
	return ( *_scheme_.at( "literals" ) + _regex_.at( "escape" )->replace( string_, *_scheme_.at( "escape" ) + "$1"_ys + *_scheme_.at( "literals") ) + *ansi::reset );
}

}

yaal::hcore::HString colorize( yaal::hcore::HString const& source_ ) {
	bool inComment( false );
	bool inSingleLineComment( false );
	bool inLiteralString( false );
	bool inLiteralChar( false );
	bool commentFirst( false );
	bool wasInComment( false );
	bool wasInSingleLineComment( false );
	bool wasInLiteralString( false );
	bool wasInLiteralChar( false );
	bool escape( false );
	string::tokens_t source;
	source.emplace_back();
	HString output = "";
	auto colorizeBuffer = [&]() {
		HString out = "";
		if ( inComment == ! wasInComment ) {
			if ( inComment ) {
				out += colorizeLines( source[0].left( source[0].get_length() - 2 ) );
				source[0] = "/*";
			} else {
				out += *_scheme_.at( "comments" ) + source[0] + *ansi::reset;
				source[0] = "";
			}
		} else if ( inSingleLineComment == ! wasInSingleLineComment ) {
			if ( inSingleLineComment ) {
				out += colorizeLines( source[0].left( source[0].get_length() - 2 ) );
				source[0] = "//";
			} else {
				out += *_scheme_.at( "comments" ) + source[0] + *ansi::reset;
				source[0] = "";
			}
		}
		if ( inLiteralString == ! wasInLiteralString ) {
			if ( inLiteralString ) {
				out += colorizeLines( source[0].left( source[0].get_length() - 1 ) );
				source[0] = "\"";
			} else {
				out += colorizeString( source[0] );
				source[0] = "";
			}
		} else if ( inLiteralChar == ! wasInLiteralChar ) {
			if ( inLiteralChar ) {
				out += colorizeLines( source[0].left( source[0].get_length() - 1 ) );
				source[0] = "'";
			} else {
				out += colorizeString( source[0] );
				source[0] = "";
			}
		}
		return ( out );
	};
	for ( char c : source_ ) {
		source[0] += c;
		if ( ! ( inComment || inSingleLineComment || inLiteralString || inLiteralChar || commentFirst ) ) {
			if ( c == '"' ) {
				inLiteralString = true;
			} else if ( c == '\'' ) {
				inLiteralChar = true;
			} else if ( c == '/' ) {
				commentFirst = true;
				continue;
			}
		} else if ( commentFirst && ( c == '*' ) ) {
			inComment = true;
		} else if ( ! escape && ( inComment || inLiteralString || inLiteralChar ) ) {
			if ( inLiteralString && ( c == '"' ) ) {
				inLiteralString = false;
			} else if ( inLiteralChar && ( c == '\'' ) ) {
				inLiteralChar = false;
			} else if ( commentFirst && ( c == '/' ) ) {
				inComment = false;
			} else if ( inComment && ( c == '*' ) ) {
				commentFirst = true;
				continue;
			} else if ( c == '\\' ) {
				escape = true;
				continue;
			}
		} else if ( inSingleLineComment && ( c == '\n' ) ) {
			inSingleLineComment = false;
		} else if ( commentFirst && ( c == '/' ) ) {
			inSingleLineComment = true;
		}
		output += colorizeBuffer();
		wasInComment = inComment;
		wasInSingleLineComment = inSingleLineComment;
		wasInLiteralString = inLiteralString;
		wasInLiteralChar = inLiteralChar;
		commentFirst = false;
		escape = false;
	}
	inComment = false;
	inSingleLineComment = false;
	inLiteralString = false;
	inLiteralChar = false;
	HString last = colorizeBuffer();
	if ( last == "" ) {
		last = colorizeLines( source[0] );
	}
	output += last;
	return ( output );
}

}

