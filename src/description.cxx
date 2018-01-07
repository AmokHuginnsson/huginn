/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  description.cxx - this file is integral part of `huginn' project.

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

#include <yaal/hcore/hlog.hxx>
#include <yaal/tools/hstringstream.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "description.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;

namespace huginn {

HDescription::HDescription( void )
	: _symbols()
	, _classes()
	, _functions()
	, _methodMap()
	, _docs()
	, _streamCache() {
	return;
}

void HDescription::clear( void ) {
	M_PROLOG
	_symbols.clear();
	_classes.clear();
	_functions.clear();
	_methodMap.clear();
	_docs.clear();
	_streamCache.clear();
	return;
	M_EPILOG
}

void HDescription::prepare( HHuginn const& huginn_ ) {
	M_PROLOG
	clear();
	/* scope for debugLevel */ {
		HScopedValueReplacement<int> debugLevel( _debugLevel_, 0 );
		huginn_.dump_vm_state( _streamCache );
	}
	HString line;
	HString type;
	HString item;
	HString alias;
	HString package;
	HString name;
	HString base;
	HString method;
	while ( getline( _streamCache, line ).good() ) {
		line.trim();
		int long sepIdx( line.find( ':'_ycp ) );
		if ( sepIdx != HString::npos ) {
			type.assign( line, 0, sepIdx );
			item.assign( line, sepIdx + 1 );
			item.trim();
			if ( type == "package" ) {
				sepIdx = item.find( '='_ycp );
				if ( sepIdx != HString::npos ) {
					alias.assign( item, 0, sepIdx );
					alias.trim();
					package.assign( item, sepIdx + 1 );
					package.trim();
					_symbols.push_back( alias );
				} else {
					hcore::log( LOG_LEVEL::ERROR ) << "Huginn: Invalid package specification." << endl;
				}
			} else if ( type == "class" ) {
				sepIdx = item.find( '{'_ycp );
				if ( sepIdx != HString::npos ) {
					name.assign( item, 0, sepIdx );
					name.trim();
					item.shift_left( sepIdx + 1 );
					item.trim_right( "}" );
					sepIdx = name.find( ':'_ycp );
					if ( sepIdx != HString::npos ) {
						base.assign( name, sepIdx + 1 );
						base.trim();
						name.erase( sepIdx );
						name.trim();
						_symbols.push_back( base );
					}
					_classes.push_back( name );
					words_t& classMethods( _methodMap[name] );
					while ( ! item.is_empty() ) {
						sepIdx = item.find( ','_ycp );
						if ( sepIdx != HString::npos ) {
							method.assign( item, 0, sepIdx );
							item.shift_left( sepIdx + 1 );
						} else {
							method.assign( item );
							item.clear();
						}
						method.trim();
						classMethods.push_back( method );
					}
				} else {
					hcore::log( LOG_LEVEL::ERROR ) << "Huginn: Invalid class specification." << endl;
				}
			} else if ( type == "function" ) {
				if ( ! item.is_empty() && ( item.front() != '*' ) ) {
					_symbols.push_back( item );
					if ( _methodMap.count( item ) == 0 ) {
						_functions.push_back( item );
					}
				}
			}
		}
	}
	sort( _symbols.begin(), _symbols.end() );
	_symbols.erase( unique( _symbols.begin(), _symbols.end() ), _symbols.end() );
	_streamCache.clear();
	huginn_.dump_docs( _streamCache );
	while ( getline( _streamCache, line ).good() ) {
		int long sepIdx( line.find( ':'_ycp ) );
		if ( sepIdx == HString::npos ) {
			throw HRuntimeException( "malformed data from yaal" );
		}
		name = line.substr( 0, sepIdx );
		item = line.substr( sepIdx + 1 );
		sepIdx = name.find( '.'_ycp );
		if ( sepIdx != HString::npos ) {
			method = name.substr( sepIdx + 1 );
		} else {
			method = name;
		}
		item.trim().trim( "-" ).trim();
		line = item;
		if ( _methodMap.count( name ) == 0 ) {
			if ( item.find( method ) == 0 ) {
				item.shift_left( method.get_length() );
				item.trim().trim( "-" ).trim();
			}
			if ( ! item.is_empty() && ( item.front() == '(' ) ) {
				sepIdx = item.find( ')'_ycp );
				if ( sepIdx != HString::npos ) {
					item.insert( sepIdx + 1, "**" );
					line.assign( "**" ).append( method ).append( item );
				}
			} else {
				line.assign( "**" ).append( method ).append( "()** - " ).append( item );
			}
		}
		_docs.insert( make_pair( name, line ) );
	}
	_symbols.push_back( "true" );
	_symbols.push_back( "false" );
	_symbols.push_back( "none" );
	return;
	M_EPILOG
}

void HDescription::note_locals( yaal::tools::HIntrospecteeInterface::variable_views_t const& variableView_ ) {
	M_PROLOG
	for ( HIntrospecteeInterface::HVariableView const& vv : variableView_ ) {
		_symbols.push_back( vv.name() );
	}
	sort( _symbols.begin(), _symbols.end() );
	return;
	M_EPILOG
}

HDescription::words_t const& HDescription::methods( yaal::hcore::HString const& symbol_ ) {
	M_PROLOG
	static words_t const empty;
	method_map_t::const_iterator it( _methodMap.find( symbol_ ) );
	return ( it != _methodMap.end() ? it->second : empty );
	M_EPILOG
}

HDescription::words_t const& HDescription::symbols( void ) const {
	return ( _symbols );
}

HDescription::words_t const& HDescription::classes( void ) const {
	return ( _classes );
}

HDescription::words_t const& HDescription::functions( void ) const {
	return ( _functions );
}

yaal::hcore::HString HDescription::doc( yaal::hcore::HString const& name_, yaal::hcore::HString const& sub_ ) const {
	HString item( ! sub_.is_empty() ? name_ + "." + sub_ : name_ );
	symbol_map_t::const_iterator it( _docs.find( item ) );
	return ( it != _docs.end() ? it->second : "" );
}

HDescription::SYMBOL_KIND HDescription::symbol_kind( yaal::hcore::HString const& name_ ) const {
	M_PROLOG
	SYMBOL_KIND sk( SYMBOL_KIND::UNKNOWN );
	if ( find( _classes.begin(), _classes.end(), name_ ) != _classes.end() ) {
		sk = SYMBOL_KIND::CLASS;
	} else if ( find( _functions.begin(), _functions.end(), name_ ) != _functions.end() ) {
		sk = SYMBOL_KIND::FUNCTION;
	}
	return ( sk );
	M_EPILOG
}

}

