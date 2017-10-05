/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  settings.cxx - this file is integral part of `huginn' project.

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

#include <yaal/hcore/base.hxx>
#include <yaal/tools/tools.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "settings.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;

namespace huginn {

OSettingObserver settingsObserver;

OSettingObserver::OSettingObserver( void )
	: _maxCallStackSize( _huginnMaxCallStack_ ) {
}

void apply_setting( yaal::tools::HHuginn& huginn_, yaal::hcore::HString const& setting_ ) {
	M_PROLOG
	HString setting( setting_ );
	setting.trim();
	int long sepIdx( setting.find( '='_ycp ) );
	if ( sepIdx != HString::npos ) {
		HString name( setting.left( sepIdx ) );
		HString value( setting.mid( sepIdx + 1 ) );
		name.trim();
		value.trim();
		if ( name == "max_call_stack_size" ) {
			huginn_.set_max_call_stack_size( settingsObserver._maxCallStackSize = lexical_cast<int>( value ) );
		} else if ( name == "default_imports" ) {
			setup._noDefaultImports = ! lexical_cast<bool>( value );
		} else if ( name == "error_context" ) {
			if ( value == "hidden" ) {
				setup._hideErrorContext = true;
			} else if ( value == "visible" ) {
				setup._hideErrorContext = false;
			} else {
				throw HRuntimeException( "unknown error_context setting: "_ys.append( value ) );
			}
		} else {
			throw HRuntimeException( "unknown setting: "_ys.append( name ) );
		}
	} else {
		throw HRuntimeException( "unknown setting: "_ys.append( setting ) );
	}
	return;
	M_EPILOG
}

rt_settings_t rt_settings( void ) {
	rt_settings_t rts( {
		{ "max_call_stack_size", to_string( settingsObserver._maxCallStackSize ) },
		{ "default_imports", lexical_cast<HString>( ! setup._noDefaultImports ) },
		{ "error_context", ( setup._hideErrorContext ? "hidden" : "visible" ) }
	} );
	return ( rts );
}

}

