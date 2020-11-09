/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/tools/huginn/helper.hxx>
#include <yaal/tools/executingparser.hxx>
#include <yaal/tools/stringalgo.hxx>

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

inline bool is_statement( yaal::hcore::HString const& token_ ) {
	return (
		( token_ != "constructor" )
		&& ( token_ != "destructor" )
		&& ( token_ != "assert" )
		&& is_keyword( token_ )
	);
}

class HFormatter {
private:
	enum class STATE {
		NORMAL,
		COMMENT,
		SINGLE_LINE_COMMENT,
		BRACKETS,
		PARENTHESES,
		SCOPE,
		IF,
		IF_BODY,
		WHILE,
		WHILE_BODY,
		FOR,
		FOR_BODY,
		SWITCH,
		SWITCH_BODY,
		CASE,
		CASE_BODY,
		TRY,
		TRY_BODY,
		CATCH,
		CATCH_BODY,
		LAMBDA,
		EXPRESSION,
		TERNARY,
		STICKY
	};
	typedef HStack<STATE> scopes_t;
	HExecutingParser _engine;
	int _maxLineLenght;
	scopes_t _scopes;
	int _indentLevel;
	int _expressionLevel;
	typedef yaal::hcore::HList<yaal::hcore::HString> tokens_t;
	tokens_t _line;
	yaal::hcore::HString _formatted;
public:
	HFormatter( void )
		: _engine( make_engine() )
		, _maxLineLenght( 128 )
		, _scopes()
		, _indentLevel( 0 )
		, _expressionLevel( 0 )
		, _line()
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
	STATE state( void ) const {
		return ( ! _scopes.is_empty() ? _scopes.top() : STATE::NORMAL );
	}
	bool in_comment( void ) const {
		STATE s( state() );
		return ( ( s == STATE::COMMENT ) || ( s == STATE::SINGLE_LINE_COMMENT ) );
	}
	bool is_direct_scope( STATE s_ ) const {
		return (
			( s_ == STATE::NORMAL )
			|| ( s_ == STATE::STICKY )
			|| ( s_ == STATE::SCOPE )
			|| ( s_ == STATE::IF_BODY )
			|| ( s_ == STATE::FOR_BODY )
			|| ( s_ == STATE::WHILE_BODY )
			|| ( s_ == STATE::SWITCH_BODY )
			|| ( s_ == STATE::CASE_BODY )
			|| ( s_ == STATE::TRY_BODY )
			|| ( s_ == STATE::CATCH_BODY )
		);
	}
	int arg_count( tokens_t::const_iterator it_, tokens_t::const_iterator end_ ) {
		int argCount( 1 );
		int nestLevel( 0 );
		while ( it_ != end_ ) {
			code_point_t c( it_->front() );
			if ( ( c == '[' ) || ( c == '(' ) || ( c == '{' ) ) {
				++ nestLevel;
			} else if ( ( c == ']' ) || ( c == ')' ) || ( c == '}' ) ) {
				++ nestLevel;
			} else if ( ( nestLevel <= 1 ) && ( c == ',' ) ) {
				++ argCount;
				break;
			}
			++ it_;
		}
		return ( argCount );
	}
	void commit( void ) {
		_formatted.append( _indentLevel, '\t'_ycp );
		typedef yaal::hcore::HStack<int> arg_count_t;
		hcore::HString prev;
		bool inCase( false );
		char const noSpaceMinusOp[][3] = {
			"(", "{", "[", "?", ":", "+", "*", "/", "%", "^", "=", "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "%=", "^=", "||", "&&", "^^"
		};
		int hadNoSpaceMinusOp( 100000 );
		scopes_t state;
		state.push( STATE::NORMAL );
		arg_count_t argCount;
		int brackets( 0 );
		int parentheses( 0 );
		int scope( 0 );
		int ternary( 0 );
		hcore::HString line;
		for ( tokens_t::const_iterator it( _line.begin() ), endIt( _line.end() ); it != endIt; ++ it ) {
			hcore::HString const& tok( *it );
			STATE s( state.top() );
			bool hasPunctation( prev.find_one_of( character_class<CHARACTER_CLASS::PUNCTATION>().data() ) != hcore::HString::npos );
			bool isStatement( is_statement( prev ) );
			if (
				( find( begin( noSpaceMinusOp ), end( noSpaceMinusOp ), prev ) != end( noSpaceMinusOp ) )
			) {
				hadNoSpaceMinusOp = 0;
			}
			inCase = inCase || ( tok == "case" ) || ( tok == "default" );
			if (
				! in_comment()
				&& ! prev.is_empty()
				&& ( ( ( tok != "(" ) && ( tok != "[" ) ) || isStatement || hasPunctation )
				&& ( tok != "," )
				&& ( tok != ";" )
				&& ( ( tok != ":" ) || ( ( s != STATE::BRACKETS ) && ( s != STATE::SCOPE ) ) )
				&& ( ( prev != ":" ) || ( s != STATE::BRACKETS ) )
				&& ( prev != "." )
				&& ( prev != "@" )
				&& ( tok != "." )
				&& ( ! inCase || ( tok != ":" ) )
				&& ( ( prev != "[" ) || ( argCount.top() > 1 ) )
				&& ( ( tok != "]" ) || ( argCount.top() > 1 ) )
				&& ( ( prev != ")" ) || ( tok != "[" ) )
				&& ( ( prev != "]" ) || ( tok != "[" ) )
				&& ( ( prev != "]" ) || ( tok != "(" ) )
				&& ( ( prev != "(" ) || ( tok != ")" ) )
				&& ( ( prev != "[" ) || ( tok != "]" ) )
				&& ( ( prev != "{" ) || ( tok != "}" ) )
				&& ( ( hadNoSpaceMinusOp != 1 ) || ( prev != "-" ) )
			) {
				line.append( " " );
			}
			line.append( tok );
			++ hadNoSpaceMinusOp;
			if ( tok == ":" ) {
				inCase = false;
			}
			prev.assign( tok );
			if ( tok == "[" ) {
				state.push( STATE::BRACKETS );
				++ brackets;
				argCount.push( arg_count( it, endIt ) );
			} else if ( tok == "]" ) {
				M_ASSERT( s == STATE::BRACKETS );
				state.pop();
				-- brackets;
				argCount.pop();
			} else if ( tok == "(" ) {
				state.push( STATE::PARENTHESES );
				++ parentheses;
			} else if ( tok == ")" ) {
				M_ASSERT( s == STATE::PARENTHESES );
				state.pop();
				-- parentheses;
			} else if ( tok == "{" ) {
				state.push( STATE::SCOPE );
				++ scope;
			} else if ( ( tok == "}" ) && ( s != STATE::NORMAL ) ) {
				M_ASSERT( s == STATE::SCOPE );
				state.pop();
				-- scope;
			} else if ( tok == "?" ) {
				state.push( STATE::TERNARY );
				++ ternary;
			} else if ( ( tok == ":" ) && ( s == STATE::TERNARY ) ) {
				state.pop();
				-- ternary;
			}
		}
		_line.clear();
		_formatted.append( line );
	}
	void newline( int count_ = 1 ) {
		if ( _expressionLevel > 0 ) {
			return;
		}
		commit();
		_formatted.append( count_, '\n'_ycp );
	}
	void unstick( void ) {
		if ( state() != STATE::STICKY ) {
			return;
		}
		newline();
		_scopes.pop();
	}
	void do_open( code_point_t c_ ) {
		unstick();
		STATE s( state() );
		bool statementScope( false );
		if ( ! in_comment() ) {
			if ( c_ == '(' ) {
				s = STATE::PARENTHESES;
			} else if ( c_ == '[' ) {
				s = STATE::BRACKETS;
			} else if ( s == STATE::IF ) {
				s = STATE::IF_BODY;
				statementScope = true;
			} else if ( s == STATE::CASE ) {
				s = STATE::CASE_BODY;
				statementScope = true;
			} else if ( s == STATE::TRY ) {
				s = STATE::TRY_BODY;
				statementScope = true;
			} else if ( c_ == '{' ) {
				s = STATE::SCOPE;
			}
			if ( statementScope ) {
				_scopes.pop();
			}
			_scopes.push( s );
		}
		append( c_ );
		if ( ( c_ == '(' ) || ( c_ == '[' ) ) {
			++ _expressionLevel;
		}
		if ( ( c_ == '{' ) && ( _expressionLevel == 0 ) ) {
			newline();
			++ _indentLevel;
		}
	}
	void do_close( code_point_t c ) {
		unstick();
		if ( ! in_comment() ) {
			if ( _scopes.is_empty() || ( matching( _scopes.top() ) != c ) ) {
				cerr << _formatted << endl;
				cerr << tools::string::join( _line, " " );
				throw HRuntimeException( "Syntax error" );
			}
			STATE state( _scopes.top() );
			_scopes.pop();
			if ( ( state == STATE::IF_BODY ) || ( state == STATE::CASE_BODY ) || ( state == STATE::TRY_BODY ) ) {
				_scopes.push( STATE::STICKY );
			}
		}
		if ( ( c == ')' ) || ( c == ']' ) ) {
			-- _expressionLevel;
			M_ASSERT( _expressionLevel >= 0 );
		}
		if ( ( c == '}' ) && ( _expressionLevel == 0 ) ) {
			-- _indentLevel;
			M_ASSERT( _indentLevel >= 0 );
			append( c );
			STATE s( state() );
			if ( ( s != STATE::STICKY ) && ( s != STATE::EXPRESSION ) ) {
				newline( _indentLevel ? 1 : 2 );
			}
		} else {
			append( c );
		}
	}
	void do_oper( code_point_t c ) {
		unstick();
		append( c );
		if ( ( c == '=' ) && ( _expressionLevel == 0 ) ) {
			_scopes.push( STATE::EXPRESSION );
			++ _expressionLevel;
		}
		if ( c == ';' ) {
			if ( state() == STATE::EXPRESSION ) {
				_scopes.pop();
				-- _expressionLevel;
			}
			if ( is_direct_scope( state() ) ) {
				newline();
			}
		}
	}
	void start_single_line_comment( void ) {
		unstick();
		append( "//" );
		_scopes.push( STATE::SINGLE_LINE_COMMENT );
	}
	void start_comment( void ) {
		unstick();
		append( "/*" );
		_scopes.push( STATE::COMMENT );
	}
	void end_comment( void ) {
		commit();
		_scopes.pop();
		append( "*/" );
		newline();
	}
	void do_identifier( yaal::hcore::HString const& word_ ) {
		if ( state() == STATE::STICKY ) {
			if ( ( word_ != "else" ) && ( word_ != "break" ) && ( word_ != "catch" ) ) {
				newline();
			}
			_scopes.pop();
		}
		if ( ! in_comment() ) {
			if ( word_ == "case" ) {
				_scopes.push( STATE::CASE );
			} else if ( word_ == "if" ) {
				_scopes.push( STATE::IF );
			} else if ( word_ == "try" ) {
				_scopes.push( STATE::TRY );
			}
		}
		append( word_ );
	}
	void do_string_literal( yaal::hcore::HString const& literal_ ) {
		unstick();
		append( "\""_ys.append( literal_ ).append( '"' ) );
	}
	void do_character_literal( yaal::hcore::HString const& literal_ ) {
		unstick();
		append( "'"_ys.append( literal_ ).append( "'" ) );
	}
	void do_white( yaal::hcore::HString const& white_ ) {
		if ( in_comment() ) {
			if ( ( state() == STATE::SINGLE_LINE_COMMENT ) && ( white_.find( '\n'_ycp ) != hcore::HString::npos ) ) {
				newline();
				_scopes.pop();
			} else {
				append( white_ );
			}
		}
	}
	template<typename T>
	HFormatter& append( T const& tok_ ) {
		_line.push_back( tok_ );
		return ( *this );
	}
private:
	HRule make_engine( void ) {
		namespace e_p = yaal::tools::executing_parser;
		HRule white( regex( "[[:space:]]+", e_p::HRegex::action_string_t( hcore::call( &HFormatter::do_white, this, _1 ) ), false ) );
		HRule open( characters( "{([", e_p::HCharacter::action_character_t( hcore::call( &HFormatter::do_open, this, _1 ) ) ) );
		HRule close( characters( "])}", e_p::HCharacter::action_character_t( hcore::call( &HFormatter::do_close, this, _1 ) ) ) );
		HRule longOper(
			e_p::constant(
				{ "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "%=", "^=", "&&", "||", "^^" },
				e_p::HString::action_string_t( hcore::call( &HFormatter::do_identifier, this, _1 ) )
			)
		);
		HRule oper( characters( "+\\-*/%!|\\^?:=<>.@~,;", e_p::HCharacter::action_character_t( hcore::call( &HFormatter::do_oper, this, _1 ) ) ) );
		HRule identifier( regex( "[^ \\t\\r\\n\\[\\](){}+*/%!|\\^?:=<>,.;@~'\"-]+", HStringLiteral::action_string_t( hcore::call( &HFormatter::do_identifier, this, _1 ) ) ) );
		HRule startSingleLineComment( e_p::string( "//", e_p::HString::action_t( hcore::call( &HFormatter::start_single_line_comment, this ) ) ) );
		HRule startComment( e_p::string( "/*", e_p::HString::action_t( hcore::call( &HFormatter::start_comment, this ) ) ) );
		HRule endComment( e_p::string( "*/", e_p::HString::action_t( hcore::call( &HFormatter::end_comment, this ) ) ) );
		HRule lexemes(
			white
			| startSingleLineComment
			| startComment
			| endComment
			| open
			| close
			| longOper
			| oper
			| string_literal( HStringLiteral::SEMANTIC::RAW )[HStringLiteral::action_string_t( hcore::call( &HFormatter::do_string_literal, this, _1 ) )]
			| character_literal( HCharacterLiteral::SEMANTIC::RAW )[HCharacterLiteral::action_string_t( hcore::call( &HFormatter::do_character_literal, this, _1 ) )]
			| identifier
		);
		return ( *lexemes );
	}
	static code_point_t matching( STATE state_ ) {
		switch ( state_ ) {
			case ( STATE::IF ):          return ( ')'_ycp );
			case ( STATE::CASE ):        return ( ')'_ycp );
			case ( STATE::TRY ):         return ( '}'_ycp );
			case ( STATE::IF_BODY ):     return ( '}'_ycp );
			case ( STATE::CASE_BODY ):   return ( '}'_ycp );
			case ( STATE::TRY_BODY ):    return ( '}'_ycp );
			case ( STATE::SCOPE ):       return ( '}'_ycp );
			case ( STATE::BRACKETS ):    return ( ']'_ycp );
			case ( STATE::PARENTHESES ): return ( ')'_ycp );
			case ( STATE::TERNARY ):     return ( ':'_ycp );
			default: {
				M_ASSERT( !"Bad code path!"[0] );
			}
		}
		return ( '\0'_ycp );
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

