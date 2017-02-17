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

string::tokens_t _preprocessors_ = { "#\\s*define", "#\\s*else", "#\\s*endif", "#\\s*if\\s+.*", "#\\s*ifdef", "#\\s*ifndef", "#\\s*include", "#\\s*pragma", "#\\s*undef" };
string::tokens_t _keywords_ = {
	"break", "case", "catch", "const_cast",
	"default", "delete", "do", "dynamic_cast", "else", "for", "friend",
	"if", "new", "operator", "private", "protected", "public",
	"reinterpret_cast", "return", "sizeof", "static_cast", "switch",
	"throw", "try", "using", "while", "this"
};
string::tokens_t _qualifiers_ = {
	"const", "explicit", "extern", "inline",
	"namespace", "static", "template", "typedef", "typename",
	"virtual", "volatile"
};
string::tokens_t _types_ = { "bool", "char", "class", "double", "enum", "float", "int", "long", "short", "struct", "unsigned", "void", "FILE" };
string::tokens_t _macros_ = { "stderr", "stdin", "stdout", "NULL" };
string::tokens_t _literals_ = { "false", "true" };

typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::ansi::HSequence const&> scheme_t;
scheme_t _scheme_ = {
	{ "preprocessors", ansi::brightblue },
	{ "keywords", ansi::yellow },
	{ "qualifiers", ansi::brightgreen },
	{ "types", ansi::brightgreen },
	{ "macros", ansi::brightmagenta },
	{ "constants", ansi::cyan },
	{ "classes", ansi::brown },
	{ "fields", ansi::brightblue },
	{ "arguments", ansi::green },
	{ "globals", ansi::brightred },
	{ "literals", ansi::brightmagenta },
	{ "comments", ansi::brightcyan },
	{ "operators", ansi::white },
	{ "escape", ansi::brightred }
};

typedef yaal::hcore::HPointer<yaal::hcore::HRegex> regex_t;
typedef yaal::hcore::HHashMap<yaal::hcore::HString, regex_t> matchers_t;
matchers_t _regex_ = {
	{ "literalsDec", make_pointer<HRegex>( "\\b([0-9]+[lL]?)\\b\\b" ) },
	{ "literalsHex", make_pointer<HRegex>( "\\b(0x[0-9a-fA-F]+[lL]?)\\b" ) },
	{ "preprocTriangle", make_pointer<HRegex>( "(<[^>]+>)" ) },
	{ "types", make_pointer<HRegex>( "\\b[a-zA-Z][a-zA-Z0-9_]*_t\\b" ) },
	{ "macros", make_pointer<HRegex>( "\\bM_[A-Z_]+\\b" ) },
	{ "constants", make_pointer<HRegex>( "\\b[A-Z_]+\\b" ) },
	{ "classes", make_pointer<HRegex>( "\\b[HO][A-Z][a-zA-Z]+\\b" ) },
	{ "fields", make_pointer<HRegex>( "\\b_[a-zA-Z0-9]+\\b" ) },
	{ "arguments", make_pointer<HRegex>( "\\b[a-zA-Z0-9]+_\\b\\b" ) },
	{ "globals", make_pointer<HRegex>( "\\b_[a-zA-Z0-9]+_\\b" ) },
	{ "operators", make_pointer<HRegex>( "[\\+\\*/%\\(\\){}\\-=<>\\]&:|?\\.,]" ) },
	{ "escape", make_pointer<HRegex>( "(\\\\.|%[.]?[0-9]*l?[hlL]?[cdfosux])" ) },
	{ "if0", make_pointer<HRegex>( "^\\s*#if\\s+0$\\Z" ) }, //, re.MULTILINE
	{ "endif", make_pointer<HRegex>( "^\\s*#endif$\\Z" ) }, //, re.MULTILINE
	{ "keywords", make_pointer<HRegex>( "\\b(" + string::join( _keywords_, "|" ) + ")\\b" ) },
	{ "qualifiers", make_pointer<HRegex>( "\\b(" + string::join( _qualifiers_, "|" ) + ")\\b" ) },
	{ "builtinMacros", make_pointer<HRegex>( "\\b(" + string::join( _macros_, "|" ) + ")\\b" ) },
	{ "builtinTypes", make_pointer<HRegex>( "\\b(" + string::join( _types_, "|" ) + ")\\b" ) },
	{ "builtinLiterals", make_pointer<HRegex>( "\\b(" + string::join( _literals_, "|" ) + ")\\b" ) },
	{ "preprocessors", make_pointer<HRegex>( "(" + string::join( _preprocessors_, "|" ) + ")\\b" ) }
};

yaal::hcore::HString replacer( yaal::hcore::HString const& scheme_, yaal::hcore::HString const& match_ ) {
	if ( _regex_.at( "preprocessors" )->matches( match_ ) ) {
		return ( match_ );
	}
	return ( scheme_ + match_ + *ansi::reset );
}

yaal::hcore::HString colorizeLines( yaal::hcore::HString const& lines_ ) {
	string::tokens_t lines( string::split<>( lines_, "\n" ) );
	HString output = "";
	for ( yaal::hcore::HString line : lines ) {
		line.push_back( '\n' );
		for ( yaal::hcore::HString const& preprocessor : _preprocessors_ ) {
			if ( HRegex( preprocessor + "\\b" ).matches( line ) ) {
				line = _regex_.at( "preprocTriangle" )->replace( line, *_scheme_.at( "literals" ) + "$1"_ys + *ansi::reset );
			}
			line = HRegex( "(" + preprocessor + ")\\b" ).replace( line, *_scheme_.at( "preprocessors" ) + "$1"_ys + *ansi::reset );
		}
		line = _regex_.at( "literalsDec" )->replace( line, call( &replacer, *_scheme_.at( "literals" ), _1 ) );
		line = _regex_.at( "literalsHex" )->replace( line, call( &replacer, *_scheme_.at( "literals" ), _1 ) );
		line = _regex_.at( "keywords" )->replace( line, call( &replacer, *_scheme_.at( "keywords" ), _1 ) );
		line = _regex_.at( "qualifiers" )->replace( line, call( &replacer, *_scheme_.at( "qualifiers" ), _1 ) );
		line = _regex_.at( "builtinMacros" )->replace( line, call( &replacer, *_scheme_.at( "macros" ), _1 ) );
		line = _regex_.at( "builtinTypes" )->replace( line, call( &replacer, *_scheme_.at( "types" ), _1 ) );
		line = _regex_.at( "builtinLiterals" )->replace( line, call( &replacer, *_scheme_.at( "literals" ), _1 ) );
		line = _regex_.at( "types" )->replace( line, call( &replacer, *_scheme_.at( "types" ), _1 ) );
		line = _regex_.at( "macros" )->replace( line, call( &replacer, *_scheme_.at( "macros" ), _1 ) );
		line = _regex_.at( "constants" )->replace( line, call( &replacer, *_scheme_.at( "constants" ), _1 ) );
		line = _regex_.at( "classes" )->replace( line, call( &replacer, *_scheme_.at( "classes" ), _1 ) );
		line = _regex_.at( "fields" )->replace( line, call( &replacer, *_scheme_.at( "fields" ), _1 ) );
		line = _regex_.at( "arguments" )->replace( line, call( &replacer, *_scheme_.at( "arguments" ), _1 ) );
		line = _regex_.at( "globals" )->replace( line, call( &replacer, *_scheme_.at( "globals" ), _1 ) );
		line = _regex_.at( "operators" )->replace( line, call( &replacer, *_scheme_.at( "operators" ), _1 ) );
		output += line;
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
	bool inIf0( false );
	bool inLiteralString( false );
	bool inLiteralChar( false );
	bool commentFirst( false );
	bool wasInComment( false );
	bool wasInSingleLineComment( false );
	bool wasInIf0( false );
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
		if ( inIf0 == ! wasInIf0 ) {
			if ( inIf0 ) {
				out += colorizeLines( source[0].left( source[0].get_length() - 5 ) );
				source[0] = "";
			} else {
				out += *_scheme_.at( "preprocessors" ) + "#if 0"_ys + *_scheme_.at( "comments" ) + source[0].left( source[0].get_length() - 6 ) + *_scheme_.at( "preprocessors" ) + "#endif" +  *ansi::reset;
				source[0] = "";
			}
		} else if ( inLiteralString == ! wasInLiteralString ) {
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
		if ( ! ( inComment || inSingleLineComment || inIf0 || inLiteralString || inLiteralChar || commentFirst ) ) {
			if ( c == '"' ) {
				inLiteralString = true;
			} else if ( c == '\'' ) {
				inLiteralChar = true;
			} else if ( c == '/' ) {
				commentFirst = true;
				continue;
			} else if ( _regex_.at( "if0" )->matches( source[0].right( 20 ) ) ) {
				inIf0 = true;
			}
		} else if ( commentFirst && ( c == '*' ) ) {
			inComment = true;
		} else if ( ! escape && ( inComment || inIf0 || inLiteralString || inLiteralChar ) ) {
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
			} else if ( inIf0 && _regex_.at( "endif" )->matches( source[0].right( 20 ) ) ) {
				inIf0 = false;
			}
		} else if ( inSingleLineComment && ( c == '\n' ) ) {
			inSingleLineComment = false;
		} else if ( commentFirst && ( c == '/' ) ) {
			inSingleLineComment = true;
		}
		output += colorizeBuffer();
		wasInComment = inComment;
		wasInSingleLineComment = inSingleLineComment;
		wasInIf0 = inIf0;
		wasInLiteralString = inLiteralString;
		wasInLiteralChar = inLiteralChar;
		commentFirst = false;
		escape = false;
	}
	inComment = false;
	inSingleLineComment = false;
	inIf0 = false;
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

