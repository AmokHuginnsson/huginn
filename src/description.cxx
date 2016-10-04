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
	, _symbolMap()
	, _methodMap()
	, _streamCache() {
	return;
}

void HDescription::clear( void ) {
	M_PROLOG
	_symbols.clear();
	_symbolMap.clear();
	_methodMap.clear();
	_streamCache.clear();
	return;
	M_EPILOG
}

void HDescription::prepare( HHuginn const& huginn_ ) {
	M_PROLOG
	_methodMap.clear();
	_symbolMap.clear();
	_symbols.clear();
	_streamCache.reset();
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
		int long sepIdx( line.find( ':' ) );
		if ( sepIdx != HString::npos ) {
			type.assign( line, 0, sepIdx );
			item.assign( line, sepIdx + 1 );
			item.trim();
			if ( type == "package" ) {
				sepIdx = item.find( '=' );
				if ( sepIdx != HString::npos ) {
					alias.assign( item, 0, sepIdx );
					alias.trim();
					package.assign( item, sepIdx + 1 );
					package.trim();
					_symbols.push_back( alias );
					_symbols.push_back( package );
					_symbolMap.insert( make_pair( alias, package ) );
				} else {
					log( LOG_LEVEL::ERROR ) << "Huginn: Invalid package specification." << endl;
				}
			} else if ( type == "class" ) {
				sepIdx = item.find( '{' );
				if ( sepIdx != HString::npos ) {
					name.assign( item, 0, sepIdx );
					name.trim();
					item.shift_left( sepIdx + 1 );
					item.trim_right( "}" );
					sepIdx = name.find( ':' );
					if ( sepIdx != HString::npos ) {
						base.assign( name, sepIdx + 1 );
						base.trim();
						name.erase( sepIdx );
						name.trim();
						_symbols.push_back( base );
					}
					_symbols.push_back( name );
					words_t& classMethods( _methodMap[name] );
					while ( ! item.is_empty() ) {
						sepIdx = item.find( ',' );
						if ( sepIdx != HString::npos ) {
							method.assign( item, 0, sepIdx );
							item.shift_left( sepIdx + 1 );
						} else {
							method.assign( item );
							item.clear();
						}
						method.trim();
						_symbols.push_back( method );
						classMethods.push_back( method );
					}
				} else {
					log( LOG_LEVEL::ERROR ) << "Huginn: Invalid class specification." << endl;
				}
			} else if ( type == "function" ) {
				if ( ! item.is_empty() && ( item.front() != '*' ) ) {
					_symbols.push_back( item );
				}
			}
		}
	}
	sort( _symbols.begin(), _symbols.end() );
	_symbols.erase( unique( _symbols.begin(), _symbols.end() ), _symbols.end() );
	return;
	M_EPILOG
}

HDescription::words_t const& HDescription::methods( yaal::hcore::HString const& symbol_ ) {
	M_PROLOG
	words_t const* w( &symbols() );
	symbol_map_t::const_iterator it( _symbolMap.find( symbol_ ) );
	if ( it != _symbolMap.end() ) {
		method_map_t::const_iterator mit( _methodMap.find( it->second ) );
		if ( mit != _methodMap.end() ) {
			w = &( mit->second );
		}
	}
	return ( *w );
	M_EPILOG
}

HDescription::words_t const& HDescription::symbols( void ) const {
	return ( _symbols );
}

}

