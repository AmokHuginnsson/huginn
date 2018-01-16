/* Read huginn/LICENSE.md file for copyright and licensing information. */

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
	{ "\\Omega", "Ω" },
	{ "\\cent", "¢" },
	{ "\\pound", "£" },
	{ "\\copy", "©" },
	{ "\\curren", "¤" },
	{ "\\yen", "¥" },
	{ "\\reg", "®" },
	{ "\\deg", "°" },
	{ "\\micro", "µ" },
	{ "\\root", "√" },
	{ "\\times", "×" },
	{ "\\divide", "÷" },
	{ "\\euro", "€" },
	{ "\\smile", "☺" },
	{ "\\forall", "∀" },
	{ "\\exist", "∃" },
	{ "\\emptyset", "∅" },
	{ "\\int", "∫" },
	{ "\\nsum", "∑" },
	{ "\\nprod", "∏" },
	{ "\\plusmn", "±" },
	{ "\\inf", "∞" },
	{ "\\qed", "∎" },
	{ "\\and", "⋀" },
	{ "\\or", "⋁" },
	{ "\\not", "¬" },
	{ "\\xor", "⊕" },
	{ "\\isin", "∈" },
	{ "\\isnotin", "∉" },
	{ "\\compose", "∘" }
} );
static symbolic_names_t _symbolicNames_( _symbolicNamesToSymbols_.get_size() );
static bool prepare_symbol_names( void ) {
	volatile bool dummy( false );
	transform( _symbolicNamesToSymbols_.begin(), _symbolicNamesToSymbols_.end(), _symbolicNames_.begin(), select1st<symbolic_names_to_symbols_t::value_type>() );
	sort( _symbolicNames_.begin(), _symbolicNames_.end() );
	return ( dummy );
}
static bool const dummy( prepare_symbol_names() );

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

