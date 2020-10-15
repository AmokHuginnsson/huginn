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
	yaal::hcore::HString _formatted;
public:
	HFormatter( void )
		: _engine( make_engine() )
		, _indentLevel( 0 )
		, _newlines( 0 )
		, _formatted() {
	}
	yaal::hcore::HString const& reformat( yaal::hcore::HString const& raw_ ) {
		if ( ! _engine( raw_ ) ) {
			cerr << _engine.error_position() << endl;
			cerr << _engine.error_messages()[ 0 ] << endl;
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
	}
	void end_comment( void ) {
		append( "*/" );
		newline();
	}
	void do_identifier( yaal::hcore::HString const& word_ ) {
		append( word_ );
	}
	void do_string_literal( yaal::hcore::HString const& literal_ ) {
		append( '"' ).append( literal_ ).append( '"' );
	}
	void do_character_literal( code_point_t c ) {
		append( "'" ).append( c ).append( "'" );
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
		HRule open( characters( "{([", HCharacter::action_character_t( hcore::call( &HFormatter::do_open, this, _1 ) ) ) );
		HRule close( characters( "])}", HCharacter::action_character_t( hcore::call( &HFormatter::do_close, this, _1 ) ) ) );
		HRule oper( characters( "+\\-*/%!|\\^?:=<>.@~,;", HCharacter::action_character_t( hcore::call( &HFormatter::do_oper, this, _1 ) ) ) );
		HRule identifier( regex( "[^ \\t\\r\\n\\[\\](){}+*/%!|\\^?:=<>,.;@~-]+", HStringLiteral::action_string_t( hcore::call( &HFormatter::do_identifier, this, _1 ) ) ) );
		HRule startComment( e_p::string( "/*", e_p::HString::action_t( hcore::call( &HFormatter::start_comment, this ) ) ) );
		HRule endComment( e_p::string( "*/", e_p::HString::action_t( hcore::call( &HFormatter::end_comment, this ) ) ) );
		HRule lexemes(
			startComment
			| endComment
			| open
			| close
			| oper
			| string_literal[HStringLiteral::action_string_t( hcore::call( &HFormatter::do_string_literal, this, _1 ) )]
			| character_literal[HCharacterLiteral::action_character_t( hcore::call( &HFormatter::do_character_literal, this, _1 ) )]
			| identifier
		);
		return ( *lexemes );
	}
};

int reformat( char const* script_ ) {
	buffer_t source( ::huginn::load( script_ ) );
	HFormatter formatter;
	cout << formatter.reformat( source.data() ) << flush;
	return ( 0 );
}

}

