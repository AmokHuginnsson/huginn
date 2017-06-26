/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  symbolicnames.cxx - this file is integral part of `huginn' project.

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
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "symbolicnames.hxx"

using namespace yaal;

namespace huginn {

typedef yaal::hcore::HArray<yaal::hcore::HString> symbolic_names_t;
typedef yaal::hcore::HHashMap<yaal::hcore::HString, char const*> symbolic_names_to_symbols_t;
static symbolic_names_to_symbols_t const _symbolicNamesToSymbols_( {
	{ "\\alpha", "α" },
	{ "\\beta", "β" },
	{ "\\gamma", "γ" },
	{ "\\delta", "δ" },
	{ "\\epsilon", "ε" },
	{ "\\zeta", "ζ" },
	{ "\\eta", "η" },
	{ "\\theta", "θ" },
	{ "\\iota", "ι" },
	{ "\\kappa", "κ" },
	{ "\\lambda", "λ" },
	{ "\\mu", "μ" },
	{ "\\nu", "ν" },
	{ "\\xi", "ξ" },
	{ "\\omicron", "ο" },
	{ "\\pi", "π" },
	{ "\\rho", "ρ" },
	{ "\\sigma", "σ" },
	{ "\\tau", "τ" },
	{ "\\upsilon", "υ" },
	{ "\\phi", "φ" },
	{ "\\chi", "χ" },
	{ "\\psi", "ψ" },
	{ "\\omega", "ω" },
	{ "\\Alpha", "Α" },
	{ "\\Beta", "Β" },
	{ "\\Gamma", "Γ" },
	{ "\\Delta", "Δ" },
	{ "\\Epsilon", "Ε" },
	{ "\\Zeta", "Ζ" },
	{ "\\Eta", "Η" },
	{ "\\Theta", "Θ" },
	{ "\\Iota", "ι" },
	{ "\\Kappa", "Κ" },
	{ "\\Lambda", "Λ" },
	{ "\\Mu", "Μ" },
	{ "\\Nu", "Ν" },
	{ "\\Xi", "Ξ" },
	{ "\\Omicron", "Ο" },
	{ "\\Pi", "Π" },
	{ "\\Rho", "Ρ" },
	{ "\\Sigma", "Σ" },
	{ "\\Tau", "Τ" },
	{ "\\Upsilon", "Υ" },
	{ "\\Phi", "Φ" },
	{ "\\Chi", "Χ" },
	{ "\\Psi", "Ψ" },
	{ "\\Omega", "Ω" }
} );
static symbolic_names_t _symbolicNames_( _symbolicNamesToSymbols_.get_size() );
static symbolic_names_t::iterator dummy( transform( _symbolicNamesToSymbols_.begin(), _symbolicNamesToSymbols_.end(), _symbolicNames_.begin(), select1st<symbolic_names_to_symbols_t::value_type>() ) );

char const* symbol_from_name( yaal::hcore::HString const& name_ ) {
	symbolic_names_to_symbols_t::const_iterator it( _symbolicNamesToSymbols_.find( name_ ) );
	return ( it != _symbolicNamesToSymbols_.end() ? it->second : nullptr );
}

symbolic_names_t symbol_name_completions( yaal::hcore::HString const& name_ ) {
	symbolic_names_t sn;
	for ( yaal::hcore::HString const& n : _symbolicNames_ ) {
		if ( n.find( name_ ) == 0 ) {
			sn.push_back( n );
		}
	}
	return ( sn );
}

}

