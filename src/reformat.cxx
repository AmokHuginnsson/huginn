/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/tools/executingparser.hxx>

M_VCSID( "$Id: " __ID__ " $" )
#include "reformat.hxx"
#include "huginn.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::executing_parser;
using namespace yaal::tools::huginn;

namespace huginn {

class HFormatter {
private:
	HExecutingParser _engine;
	int _indentLevel;
	int _newlines;
	bool _inComment;
	yaal::hcore::HString _formatted;
public:
	HFormatter( void )
		: _engine( make_engine() )
		, _indentLevel( 0 )
		, _newlines( 0 )
		, _inComment( false )
		, _formatted() {
	}
	yaal::hcore::HString const& reformat( yaal::hcore::HString const& raw_ ) {
		if ( ! _engine( raw_ ) ) {
			cerr << _engine.error_position() << endl;
			for ( yaal::hcore::HString const& errMsg : _engine.error_messages() ) {
				cerr << errMsg << endl;
			}
			return ( _formatted );
		}
		_engine();
		if ( ! _formatted.is_empty() && ( _formatted.back() != '\n' ) ) {
			_formatted.push_back( '\n'_ycp );
		}
		return ( _formatted );
	}
private:
	void newline( int count_ = 1 ) {
		_newlines = count_;
	}
	void do_open( code_point_t c ) {
		append( c );
		if ( c == '{' ) {
			newline();
			++ _indentLevel;
		}
	}
	void do_close( code_point_t c ) {
		if ( c == '}' ) {
			-- _indentLevel;
			M_ASSERT( _indentLevel >= 0 );
			append( c );
			newline( _indentLevel ? 1 : 2 );
		} else {
			append( c );
		}
	}
	void do_oper( code_point_t c ) {
		append( c );
		if ( c == ';' ) {
			newline();
		}
	}
	void start_comment( void ) {
		append( "/*" );
		_inComment = true;
	}
	void end_comment( void ) {
		_inComment = false;
		append( "*/" );
		newline();
	}
	void do_identifier( yaal::hcore::HString const& word_ ) {
		append( word_ );
	}
	void do_string_literal( yaal::hcore::HString const& literal_ ) {
		append( '"' ).append( literal_ ).append( '"' );
	}
	void do_character_literal( yaal::hcore::HString const& literal_ ) {
		append( "'" ).append( literal_ ).append( "'" );
	}
	void do_white( yaal::hcore::HString const& white_ ) {
		if ( _inComment ) {
			_formatted.append( white_ );
		}
	}
	template<typename T>
	HFormatter& append( T const& tok_ ) {
		if ( _newlines ) {
			_formatted.append( _newlines, '\n'_ycp );
			_newlines = 0;
			_formatted.append( _indentLevel, '\t'_ycp );
		}
		_formatted.append( tok_ );
		return ( *this );
	}
private:
	HRule make_engine( void ) {
		namespace e_p = yaal::tools::executing_parser;
		HRule white( regex( "[[:space:]]+", e_p::HRegex::action_string_t( hcore::call( &HFormatter::do_white, this, _1 ) ), false ) );
		HRule open( characters( "{([", HCharacter::action_character_t( hcore::call( &HFormatter::do_open, this, _1 ) ) ) );
		HRule close( characters( "])}", HCharacter::action_character_t( hcore::call( &HFormatter::do_close, this, _1 ) ) ) );
		HRule oper( characters( "+\\-*/%!|\\^?:=<>.@~,;", HCharacter::action_character_t( hcore::call( &HFormatter::do_oper, this, _1 ) ) ) );
		HRule identifier( regex( "[^ \\t\\r\\n\\[\\](){}+*/%!|\\^?:=<>,.;@~'\"-]+", HStringLiteral::action_string_t( hcore::call( &HFormatter::do_identifier, this, _1 ) ) ) );
		HRule startComment( e_p::string( "/*", e_p::HString::action_t( hcore::call( &HFormatter::start_comment, this ) ) ) );
		HRule endComment( e_p::string( "*/", e_p::HString::action_t( hcore::call( &HFormatter::end_comment, this ) ) ) );
		HRule lexemes(
			white
			| startComment
			| endComment
			| open
			| close
			| oper
			| string_literal( HStringLiteral::SEMANTIC::RAW )[HStringLiteral::action_string_t( hcore::call( &HFormatter::do_string_literal, this, _1 ) )]
			| character_literal( HCharacterLiteral::SEMANTIC::RAW )[HCharacterLiteral::action_string_t( hcore::call( &HFormatter::do_character_literal, this, _1 ) )]
			| identifier
		);
		return ( *lexemes );
	}
};

int reformat( char const* script_ ) {
	buffer_t source( ::huginn::load( script_ ) );
	HFormatter formatter;
	hcore::HString s( source.data(), source.get_size() );
	cout << formatter.reformat( s ) << flush;
	return ( 0 );
}

}

