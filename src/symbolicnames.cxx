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
	{ "\\^0", "⁰" },
	{ "\\^1", "¹" },
	{ "\\^2", "²" },
	{ "\\^3", "³" },
	{ "\\^4", "⁴" },
	{ "\\^5", "⁵" },
	{ "\\^6", "⁶" },
	{ "\\^7", "⁷" },
	{ "\\^8", "⁸" },
	{ "\\^9", "⁹" },
	{ "\\^a", "ᵃ" },
	{ "\\^b", "ᵇ" },
	{ "\\^c", "ᶜ" },
	{ "\\^d", "ᵈ" },
	{ "\\^e", "ᵉ" },
	{ "\\^f", "ᶠ" },
	{ "\\^g", "ᵍ" },
	{ "\\^h", "ʰ" },
	{ "\\^i", "ⁱ" },
	{ "\\^j", "ʲ" },
	{ "\\^k", "ᵏ" },
	{ "\\^l", "ˡ" },
	{ "\\^m", "ᵐ" },
	{ "\\^n", "ⁿ" },
	{ "\\^o", "ᵒ" },
	{ "\\^p", "ᵖ" },
	{ "\\^r", "ʳ" },
	{ "\\^s", "ˢ" },
	{ "\\^t", "ᵗ" },
	{ "\\^u", "ᵘ" },
	{ "\\^v", "ᵛ" },
	{ "\\^w", "ʷ" },
	{ "\\^x", "ˣ" },
	{ "\\^y", "ʸ" },
	{ "\\^z", "ᶻ" },
	{ "\\^A", "ᴬ" },
	{ "\\^B", "ᴮ" },
	{ "\\^D", "ᴰ" },
	{ "\\^E", "ᴱ" },
	{ "\\^G", "ᴳ" },
	{ "\\^H", "ᴴ" },
	{ "\\^I", "ᴵ" },
	{ "\\^J", "ᴶ" },
	{ "\\^K", "ᴷ" },
	{ "\\^L", "ᴸ" },
	{ "\\^M", "ᴹ" },
	{ "\\^N", "ᴺ" },
	{ "\\^O", "ᴼ" },
	{ "\\^P", "ᴾ" },
	{ "\\^R", "ᴿ" },
	{ "\\^T", "ᵀ" },
	{ "\\^U", "ᵁ" },
	{ "\\^V", "ⱽ" },
	{ "\\^W", "ᵂ" },
	{ "\\_0", "₀" },
	{ "\\_1", "₁" },
	{ "\\_2", "₂" },
	{ "\\_3", "₃" },
	{ "\\_4", "₄" },
	{ "\\_5", "₅" },
	{ "\\_6", "₆" },
	{ "\\_7", "₇" },
	{ "\\_8", "₈" },
	{ "\\_9", "₉" },
	{ "\\_a", "ₐ" },
	{ "\\_e", "ₑ" },
	{ "\\_h", "ₕ" },
	{ "\\_i", "ᵢ" },
	{ "\\_j", "ⱼ" },
	{ "\\_k", "ₖ" },
	{ "\\_l", "ₗ" },
	{ "\\_m", "ₘ" },
	{ "\\_n", "ₙ" },
	{ "\\_o", "ₒ" },
	{ "\\_p", "ₚ" },
	{ "\\_r", "ᵣ" },
	{ "\\_s", "ₛ" },
	{ "\\_t", "ₜ" },
	{ "\\_u", "ᵤ" },
	{ "\\_v", "ᵥ" },
	{ "\\_x", "ₓ" },
	{ "\\ne", "≠" },
	{ "\\le", "≤" },
	{ "\\ge", "≥" },
	{ "\\and", "⋀" },
	{ "\\or", "⋁" },
	{ "\\not", "¬" },
	{ "\\xor", "⊕" },
	{ "\\isin", "∈" },
	{ "\\isnotin", "∉" },
	{ "\\times", "×" },
	{ "\\divide", "÷" },
	{ "\\compose", "∘" },
	{ "\\root", "√" },
	{ "\\nsum", "∑" },
	{ "\\nprod", "∏" },
	{ "\\int", "∫" },
	{ "\\emptyset", "∅" },
	{ "\\plusmn", "±" },
	{ "\\inf", "∞" },
	{ "\\deg", "°" },
	{ "\\qed", "∎" },
	{ "\\forall", "∀" },
	{ "\\exist", "∃" },
	{ "\\copy", "©" },
	{ "\\reg", "®" },
	{ "\\micro", "µ" },
	{ "\\curren", "¤" },
	{ "\\cent", "¢" },
	{ "\\pound", "£" },
	{ "\\yen", "¥" },
	{ "\\euro", "€" },
	{ "\\smile", "☺" }
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

