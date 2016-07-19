/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  interactive.cxx - this file is integral part of `huginn' project.

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

#include <yaal/tools/hhuginn.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/memory.hxx>
#include <yaal/tools/hstringstream.hxx>
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/signals.hxx>

#include <readline/readline.h>
#include <readline/history.h>

#include <signal.h>

M_VCSID( "$Id: " __ID__ " $" )
#include "interactive.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace huginn {

class HInteractiveRunner {
public:
	typedef HInteractiveRunner this_type;
	typedef yaal::hcore::HArray<yaal::hcore::HString> lines_t;
	typedef yaal::hcore::HArray<yaal::hcore::HString> words_t;
	enum class LINE_TYPE {
		NONE,
		CODE,
		IMPORT
	};
private:
	lines_t _lines;
	lines_t _imports;
	LINE_TYPE _lastLineType;
	yaal::hcore::HString _lastLine;
	bool _expression;
	bool _interrupted;
	HHuginn::ptr_t _huginn;
	HStringStream _streamCache;
	words_t _wordCache;
	yaal::hcore::HString _source;
public:
	HInteractiveRunner( void )
		: _lines()
		, _imports()
		, _lastLineType( LINE_TYPE::NONE )
		, _lastLine()
		, _expression( false )
		, _interrupted( false )
		, _huginn()
		, _streamCache()
		, _wordCache()
		, _source() {
		if ( ! setup._noDefaultImports ) {
			_imports.emplace_back( "import Mathematics as M;" );
			_imports.emplace_back( "import Algorithms as A;" );
			_imports.emplace_back( "import Text as T;" );
		}
		HSignalService::get_instance().register_handler( SIGINT, call( &HInteractiveRunner::handle_interrupt, this, _1 ) );
		return;
	}
	bool add_line( yaal::hcore::HString const& line_ ) {
		M_PROLOG
		static char const inactive[] = ";\t \r\n\a\b\f\v";
		static HRegex importPattern( "\\s*import\\s+[A-Za-z]+\\s+as\\s+[A-Za-z]+;?" );
		_lastLineType = LINE_TYPE::NONE;
		bool isImport( importPattern.matches( line_ ) );
		_streamCache.clear();

		HString result( line_ );

		bool gotSemi( false );
		result.trim_right( _whiteSpace_.data() );
		while ( ! result.is_empty() && ( result.back() == ';' ) ) {
			result.pop_back();
			result.trim_right( _whiteSpace_.data() );
			gotSemi = true;
		}
		bool gotResult( ! result.is_empty() );

		if ( ! ( gotResult || _lines.is_empty() ) ) {
			result = _lines.back();
			result.trim_right( inactive );
		}

		_expression = ! result.is_empty() && ( result.back() != '}' );

		int lineCount( static_cast<int>( _lines.get_size() ) );

		for ( yaal::hcore::HString const& import : _imports ) {
			_streamCache << import << endl;
		}

		if ( isImport ) {
			gotResult = false;
			if ( ! _lines.is_empty() ) {
				result = _lines.back();
			} else {
				result.clear();
			}
			result.trim_right( inactive );
			if ( result.is_empty() ) {
				_expression = false;
			}
			_streamCache << line_ << ( ( line_.back() != ';' ) ? ";" : "" ) << endl;
		}

		_streamCache << "main() {\n";
		for ( int i( 0 ); i < ( lineCount - ( gotResult ? 0 : 1 ) ); ++ i ) {
			_streamCache << '\t' << _lines[i] << "\n";
		}

		if ( _expression ) {
			_streamCache << "\treturn ( " << result << " );\n}\n";
		} else {
			if ( ! result.is_empty() ) {
				_streamCache << "\t" << result << ( gotSemi ? ";" : "" ) << "\n";
			}
			_streamCache << "\treturn;\n}\n";
		}
		_source = _streamCache.string();
		_huginn = make_pointer<HHuginn>();
		_huginn->load( _streamCache, "*interactive session*" );
		_huginn->preprocess();
		bool ok( _huginn->parse() );
		if ( ! ok ) {
			int importCount( static_cast<int>( _imports.get_size() ) );
			if ( ( lineCount > 0 ) && ( ( _huginn->error_coordinate().line() - 2 ) == ( lineCount + importCount ) ) ) {
				if ( _lines.back().back() != ';' ) {
					_lines.back().push_back( ';' );
					ok = add_line( line_ );
					if ( ok ) {
						_lines.pop_back();
					} else {
						_lines.back().pop_back();
					}
				}
			}
		} else {
			ok = _huginn->compile( HHuginn::COMPILER::BE_SLOPPY );
		}

		if ( ok ) {
			if ( gotResult ) {
				if ( gotSemi ) {
					result.push_back( ';' );
				}
				_lines.push_back( _lastLine = result );
			} else if ( isImport ) {
				_imports.push_back( _lastLine = line_ );
				if ( line_.back() != ';' ) {
					_imports.back().push_back( ';' );
				}
			}
			_lastLineType = isImport ? LINE_TYPE::IMPORT : LINE_TYPE::CODE;
			fill_words();
		} else {
			_lastLine = isImport ? line_ : result;
		}
		return ( ok );
		M_EPILOG
	}
	HHuginn::value_t execute( void ) {
		M_PROLOG
		bool ok( true );
		if ( ! ( ok = _huginn->execute() ) ) {
			if ( _lastLineType == LINE_TYPE::CODE ) {
				_lines.pop_back();
			} else if ( _lastLineType == LINE_TYPE::IMPORT ) {
				_imports.pop_back();
			}
		} else {
			clog << _source;
		}
		if ( _interrupted ) {
			_interrupted = false;
			yaal::_isKilled_ = false;
		}
		return ( ok ? _huginn->result() : HHuginn::value_t() );
		M_EPILOG
	}
	yaal::hcore::HString err( void ) const {
		M_PROLOG
		int lineNo( _huginn->error_coordinate().line() );
		int colNo( _huginn->error_coordinate().column() - ( _expression ? 11 : 1 ) );
		hcore::HString colored( setup._noColor ? _lastLine : _lastLine.left( colNo ) );
		char item( colNo < static_cast<int>( _lastLine.get_length() ) ? _lastLine[colNo] : 0 );
		if ( item && ! setup._noColor ) {
			colored.append( *ansi::bold ).append( item ).append( *ansi::reset ).append( _lastLine.mid( colNo + 1 ) );
		}
		for ( yaal::hcore::HString const& line : _imports ) {
			cout << line << endl;
		}
		if ( lineNo <= static_cast<int>( _imports.get_size() + 1 ) ) {
			cout << colored << ( _lastLine.back() != ';' ? ";" : "" ) << endl;
		}
		cout << "main() {" << endl;
		for ( yaal::hcore::HString const& line : _lines ) {
			cout << line << endl;
		}
		if ( lineNo > static_cast<int>( _imports.get_size() + 1 ) ) {
			if ( _expression ) {
				cout << "\treturn ( " << colored << " );" << endl;
			} else {
				cout << "\t" << colored << "\n\treturn;" << endl;
			}
		}
		cout << "}" << endl;
		return ( _huginn->error_message() );
		M_EPILOG
	}
	int handle_interrupt( int ) {
		yaal::_isKilled_ = true;
		_interrupted = true;
		return ( 1 );
	}
	void fill_words( void ) {
		M_PROLOG
		_wordCache.clear();
		_streamCache.clear();
		_huginn->dump_vm_state( _streamCache );
		HString word;
		_streamCache.read_until( word ); /* drop header */
		while ( _streamCache.read_until( word, _whiteSpace_.data() ) > 0 ) {
			if ( word.is_empty() || ( word.back() == ':' ) || ( word.back() == ')' ) || ( word.front() == '(' ) ) {
				continue;
			}
			_wordCache.push_back( word );
		}
		sort( _wordCache.begin(), _wordCache.end() );
		_wordCache.erase( unique( _wordCache.begin(), _wordCache.end() ), _wordCache.end() );
		return;
		M_EPILOG
	}
	words_t const& words( void ) {
		M_PROLOG
		if ( _wordCache.is_empty() ) {
			_streamCache.clear();
			for ( yaal::hcore::HString const& line : _imports ) {
				_streamCache << line << endl;
			}
			_streamCache << "main() {" << endl;
			for ( yaal::hcore::HString const& line : _lines ) {
				_streamCache << line << endl;
			}
			if ( ! _lines.is_empty() ) {
				if ( ( _lines.back().back() != ';' ) && ( _lines.back().back() != '}' ) ) {
					_streamCache << ';';
				}
			}
			_streamCache << "return;\n}\n" << endl;
			_huginn = make_pointer<HHuginn>();
			_huginn->load( _streamCache, "*interactive session*" );
			_huginn->preprocess();
			if ( _huginn->parse() && _huginn->compile( HHuginn::COMPILER::BE_SLOPPY ) ) {
				fill_words();
			}
		}
		return ( _wordCache );
		M_EPILOG
	}
};

namespace {

HInteractiveRunner* _interactiveRunner_( nullptr );
char* completion_words( char const* prefix_, int state_ ) {
	static int index( 0 );
	rl_completion_suppress_append = 1;
	if ( state_ == 0 ) {
		index = 0;
	}
	HInteractiveRunner::words_t const& words( _interactiveRunner_->words() );
	int len( static_cast<int>( ::strlen( prefix_ ) ) );
	char* p( nullptr );
	if ( len > 0 ) {
		for ( ; index < words.get_size(); ++ index ) {
			if ( strncmp( prefix_, words[index].raw(), static_cast<size_t>( len ) ) == 0 ) {
				p = strdup( words[index].raw() );
				break;
			}
		}
	} else if ( index < words.get_size() ) {
		p = strdup( words[index].c_str() );
	}
	++ index;
	return ( p );
}

}

namespace {
void make_prompt( HString& prompt_, int& no_ ) {
	prompt_.clear();
	if ( ! setup._noColor ) {
		prompt_.assign( *ansi::blue );
	}
	prompt_.append( "huginn[" );
	if ( ! setup._noColor ) {
		prompt_.append( *ansi::brightblue );
	}
	prompt_.append( no_ );
	if ( ! setup._noColor ) {
		prompt_.append( *ansi::blue );
	}
	prompt_.append( "]> " );
	if ( ! setup._noColor ) {
		prompt_.append( *ansi::reset );
	}
	++ no_;
}
}

int interactive_session( void ) {
	M_PROLOG
	HString prompt;
	int lineNo( 0 );
	make_prompt( prompt, lineNo );
	HString line;
	HInteractiveRunner ir;
	_interactiveRunner_ = &ir;
	char* rawLine( nullptr );
	rl_completion_entry_function = completion_words;
	rl_basic_word_break_characters = " \t\n\"\\'`@$><=;|&{(.";
	if ( ! setup._historyPath.is_empty() ) {
		read_history( setup._historyPath.c_str() );
	}
	while ( ( rawLine = readline( prompt.raw() ) ) ) {
		line = rawLine;
		if ( ! line.is_empty() ) {
			add_history( rawLine );
		}
		memory::free0( rawLine );
		if ( ir.add_line( line ) ) {
			HHuginn::value_t res( ir.execute() );
			if ( !! res ) {
				cout << to_string( res ) << endl;
			} else {
				cerr << ir.err() << endl;
			}
		} else {
			cerr << ir.err() << endl;
		}
		make_prompt( prompt, lineNo );
	}
	cout << endl;
	if ( ! setup._historyPath.is_empty() ) {
		write_history( setup._historyPath.c_str() );
	}
	return ( 0 );
	M_EPILOG
}

}

