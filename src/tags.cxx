/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/huginn/runtime.hxx>
#include <yaal/tools/huginn/thread.hxx>
#include <yaal/tools/huginn/objectfactory.hxx>
#include <yaal/tools/huginn/helper.hxx>
#include <yaal/tools/hmemory.hxx>

M_VCSID( "$Id: " __ID__ " $" )
#include "tags.hxx"
#include "huginn.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace huginn {

namespace {

HHuginn::value_t dummy_repl( tools::huginn::HThread*, HHuginn::value_t*, HHuginn::values_t&, int ) {
	return ( HHuginn::value_t() );
}

inline char const* symbol_kind_name( HHuginn::SYMBOL_KIND symbolKind_ ) {
	char const* symbolKindName( "invalid" );
	switch ( symbolKind_ ) {
		case ( HHuginn::SYMBOL_KIND::CLASS ):    symbolKindName = "class";    break;
		case ( HHuginn::SYMBOL_KIND::ENUM ):     symbolKindName = "enum";     break;
		case ( HHuginn::SYMBOL_KIND::FIELD ):    symbolKindName = "field";   break;
		case ( HHuginn::SYMBOL_KIND::FUNCTION ): symbolKindName = "function"; break;
		case ( HHuginn::SYMBOL_KIND::METHOD ):   symbolKindName = "method";   break;
		case ( HHuginn::SYMBOL_KIND::PACKAGE ):  symbolKindName = "import";   break;
		case ( HHuginn::SYMBOL_KIND::VARIABLE ): symbolKindName = "variable"; break;
		case ( HHuginn::SYMBOL_KIND::UNKNOWN ):  symbolKindName = "unknown";  break;
	}
	return ( symbolKindName );
}

char const* symbol_kind_name_short( HHuginn::SYMBOL_KIND symbolKind_ ) {
	char const* symbolKindName( "x" );
	switch ( symbolKind_ ) {
		case ( HHuginn::SYMBOL_KIND::CLASS ):    symbolKindName = "c"; break;
		case ( HHuginn::SYMBOL_KIND::ENUM ):     symbolKindName = "e"; break;
		case ( HHuginn::SYMBOL_KIND::FIELD ):    symbolKindName = "m"; break;
		case ( HHuginn::SYMBOL_KIND::FUNCTION ): symbolKindName = "f"; break;
		case ( HHuginn::SYMBOL_KIND::METHOD ):   symbolKindName = "m"; break;
		case ( HHuginn::SYMBOL_KIND::PACKAGE ):  symbolKindName = "i"; break;
		case ( HHuginn::SYMBOL_KIND::VARIABLE ): symbolKindName = "v"; break;
		case ( HHuginn::SYMBOL_KIND::UNKNOWN ):  symbolKindName = "u"; break;
	}
	return ( symbolKindName );
}

class HTagger : public yaal::tools::HIntrospectorInterface {
private:
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, HHuginn::SYMBOL_KIND> symbol_kinds_t;
	typedef yaal::hcore::HArray<yaal::hcore::HString> tags_t;
	HHuginn& _huginn;
	symbol_kinds_t _symbolKinds;
	yaal::hcore::HString _code;
	tags_t _tags;
public:
	HTagger( HHuginn& huginn_, char const* code_, int size_ )
		: _huginn( huginn_ )
		, _symbolKinds()
		, _code( code_, size_ )
		, _tags() {
	}
	void dump( void ) {
		sort( _tags.begin(), _tags.end() );
		for ( yaal::hcore::HString const& tag : _tags ) {
			cout << tag << endl;
		}
	}
private:
	virtual void do_introspect( HIntrospecteeInterface& ) override {}
	virtual void do_symbol( symbols_t const& symbols_, HHuginn::SYMBOL_KIND symbolKind_, int fileId_, int position_ ) override {
		int realPosition( _huginn.real_position( fileId_, position_ ) );
		_symbolKinds[symbols_.back()] = symbolKind_;
		int long eol( _code.find( '\n'_ycp, realPosition ) );
		eol = eol != hcore::HString::npos ? eol - realPosition : _code.get_length() - realPosition;
		hcore::HString snip( _code.substr( realPosition, eol ) );
		snip.trim_right( ";,{ \t" );
		hcore::HString tag( symbols_.back() );
		if ( symbolKind_ == HHuginn::SYMBOL_KIND::PACKAGE ) {
			tag.append( "(" ).append( symbols_.front() ).append( ")" );
		}
		tag
			.append( "\t" )
			.append( _huginn.source_name( fileId_ ) )
			.append( "\t/^" )
			.append( snip )
			.append( "$/;\"\tkind:" )
			.append( symbol_kind_name_short( symbolKind_ ) );
		if ( symbols_.get_size() > 1 ) {
			if ( ( symbolKind_ == HHuginn::SYMBOL_KIND::METHOD ) || ( symbolKind_ == HHuginn::SYMBOL_KIND::FIELD ) ) {
				tag.append( "\t" ).append( symbol_kind_name( _symbolKinds.at( symbols_.front() ) ) ).append( ":" );
			} else {
				tag.append( "\timport:" );
			}
			tag.append( symbols_.front() );
		}
		tag.append( "\tline:" ).append( _huginn.get_coordinate( fileId_, position_ ).line() ).append( "\tlanguage:Huginn" );
		if ( ( symbolKind_ == HHuginn::SYMBOL_KIND::METHOD ) || ( symbolKind_ == HHuginn::SYMBOL_KIND::FIELD ) ) {
			tag.append( "\taccess:public" );
		}
		_tags.emplace_back( yaal::move( tag ) );
	}
private:
	HTagger( HTagger const& ) = delete;
	HTagger& operator = ( HTagger const& ) = delete;
};

}

int tags( char const* script_ ) {
	M_PROLOG
	HHuginn::disable_grammar_verification();
	int lineSkip( 0 );
	buffer_t buffer( ::huginn::load( script_, &lineSkip ) );
	HMemory source( make_resource<HMemoryObserver>( buffer.data(), buffer.get_size() ), HMemory::INITIAL_STATE::VALID );
	HHuginn::ptr_t h( make_pointer<HHuginn>() );
	h->load( source, script_, lineSkip );
	h->register_function( "repl", call( &dummy_repl, _1, _2, _3, _4 ), "( [*prompt*] ) - read line of user input potentially prefixing it with *prompt*" );
	h->preprocess();
	int retVal( 0 );
	do {
		if ( ! h->parse() ) {
			retVal = 1;
			break;
		}
		HTagger tagger( *h, buffer.data(), static_cast<int>( buffer.get_size() ) );
		if ( ! h->compile( setup._modulePath, HHuginn::COMPILER::BE_SLOPPY, &tagger ) ) {
			cerr << h->error_message() << endl;
			retVal = 2;
			break;
		}
		tagger.dump();
	} while ( false );
	return ( retVal );
	M_EPILOG
}

}

