/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/harray.hxx>
#include <yaal/hcore/hstack.hxx>
#include <yaal/hcore/hregex.hxx>
#include <yaal/hcore/math.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "braceexpansion.hxx"
#include "quotes.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::string;

namespace huginn {

class HCyclicString {
public:
	typedef yaal::hcore::HResource<HCyclicString> res_t;
public:
	virtual ~HCyclicString( void ) {}
	bool scan( yaal::hcore::HString& out_, bool next_ ) {
		return ( do_scan( out_, next_ ) );
	}
private:
	virtual bool do_scan( yaal::hcore::HString&, bool ) = 0;
};

class HLitaral : public HCyclicString {
private:
	yaal::hcore::HString _string;
public:
	HLitaral( yaal::hcore::HString const& string_ )
		: _string( string_ ) {
	}
private:
	virtual bool do_scan( yaal::hcore::HString& out_, bool next_ ) override {
		out_.assign( _string );
		return next_;
	}
};

class HAlternative : public HCyclicString {
public:
	typedef yaal::hcore::HArray<res_t> cyclic_strings_t;
private:
	cyclic_strings_t _alternatives;
	int _currentIdx;
public:
	HAlternative( cyclic_strings_t&& alternatives_ )
		: _alternatives( yaal::move( alternatives_ ) )
		, _currentIdx( 0 ) {
		M_ASSERT( _alternatives.get_size() > 1 );
	}
private:
	virtual bool do_scan( yaal::hcore::HString& out_, bool next_ ) override {
		if ( _alternatives[_currentIdx]->scan( out_, next_ ) ) {
			++ _currentIdx;
		}
		bool next( false );
		if ( _currentIdx >= static_cast<int>( _alternatives.get_size() ) ) {
			_currentIdx = 0;
			next = true;
		}
		return next;
	}
};

class HRange : public HCyclicString {
private:
	int _from;
	int _to;
	int _step;
	int _currentIdx;
public:
	HRange( int from_, int to_, int step_ )
		: _from( from_ )
		, _to( to_ )
		, _step( math::abs( step_ ) )
		, _currentIdx( from_ ) {
		if ( _step == 0 ) {
			_step = 1;
		}
		if ( _to < _from ) {
			_step = -_step;
		}
	}
private:
	virtual bool do_scan( yaal::hcore::HString& out_, bool next_ ) override {
		out_.assign( _currentIdx );
		if ( next_ ) {
			_currentIdx += _step;
		}
		bool next( false );
		if ( _currentIdx > _to ) {
			_currentIdx = _from;
			next = true;
		}
		return next;
	}
};

class HConcatenation : public HCyclicString {
public:
	typedef yaal::hcore::HArray<res_t> cyclic_strings_t;
private:
	cyclic_strings_t _parts;
public:
	HConcatenation( cyclic_strings_t&& parts_ )
		: _parts( yaal::move( parts_ ) ) {
		M_ASSERT( _parts.get_size() > 1 );
	}
	virtual bool do_scan( yaal::hcore::HString& out_, bool next_ ) override {
		bool next( next_ );
		HString s;
		out_.clear();
		for ( cyclic_strings_t::reverse_iterator it( _parts.rbegin() ), end( _parts.rend() ); it != end; ++ it ) {
			next = (*it)->scan( s, next );
			out_.insert( 0, s );
		}
		return next;
	}
};

HCyclicString::res_t brace_expander( yaal::hcore::HString const& );
HCyclicString::res_t brace_expander( yaal::hcore::HString const& string_ ) {
	M_PROLOG
	typedef yaal::hcore::HStack<HConcatenation::cyclic_strings_t> parts_t;
	typedef yaal::hcore::HArray<int> positions_t;
	typedef yaal::hcore::HStack<int> braces_t;
	positions_t positions;
	braces_t braces;
	HCyclicString::res_t braceExpander;
	HQuoteObserver qo;
	int len( static_cast<int>( string_.get_length() ) );
	for ( int i( 0 ); i < len; ++ i ) {
		code_point_t cp( string_[i] );
		if ( qo.notice( cp ) ) {
			continue;
		}
		if ( cp == '{' ) {
			braces.push( i );
			continue;
		}
		if ( ( cp == '}' ) && ! braces.is_empty() ) {
			positions.push_back( braces.top() );
			braces.pop();
			continue;
		}
	}
	sort( positions.begin(), positions.end() );
	parts_t parts;
	parts.push( HConcatenation::cyclic_strings_t() );
	HString raw;
	HRegex rangeRE( "(-?\\d+)[.][.](-?\\d+)([.][.]-?(\\d+))?" );
	qo.reset();
	int braceIdx( 0 );
	for ( int i( 0 ); i < len; ++ i ) {
		code_point_t cp( string_[i] );
		if ( qo.notice( cp ) ) {
			raw.push_back( cp );
			continue;
		}
		if ( ( braceIdx < positions.get_size() ) && ( i == positions[braceIdx] ) ) {
			++ braceIdx;
			if ( ! raw.is_empty() ) {
				parts.top().push_back( make_resource<HLitaral>( raw ) );
				raw.clear();
			}
			parts.push( HConcatenation::cyclic_strings_t() );
			parts.push( HConcatenation::cyclic_strings_t() );
			continue;
		}
		if ( ( cp == '}' ) && ( parts.get_size() > 1 ) ) {
			HConcatenation::cyclic_strings_t concatenation( yaal::move( parts.top() ) );
			parts.pop();
			HConcatenation::cyclic_strings_t alternative( yaal::move( parts.top() ) );
			parts.pop();
			if ( concatenation.is_empty() && alternative.is_empty() ) {
				HRegex::groups_t groups( rangeRE.groups( raw ) );
				if ( ! groups.is_empty() ) {
					int from( lexical_cast<int>( raw.substr( groups[1].start(), groups[1].size() ) ) );
					int to( lexical_cast<int>( raw.substr( groups[2].start(), groups[2].size() ) ) );
					int step( groups.get_size() > 3 ? lexical_cast<int>( raw.substr( groups[4].start(), groups[4].size() ) ) : 1 );
					concatenation.push_back( make_resource<HRange>( from, to, step ) );
				} else {
					raw.insert( 0, 1, '{'_ycp ).push_back( '}'_ycp );
					continue;
				}
			} else if ( alternative.is_empty() ) {
				concatenation.insert( concatenation.begin(), make_resource<HLitaral>( "{" ) );
				raw.push_back( '}'_ycp );
				concatenation.push_back( make_resource<HLitaral>( raw ) );
			} else {
				concatenation.push_back( make_resource<HLitaral>( raw ) );
			}
			if ( concatenation.get_size() > 1 ) {
				alternative.push_back( make_resource<HConcatenation>( yaal::move( concatenation ) ) );
			} else {
				alternative.push_back( yaal::move( concatenation.front() ) );
			}
			if ( alternative.get_size() > 1 ) {
				parts.top().push_back( make_resource<HAlternative>( yaal::move( alternative ) ) );
			} else {
				parts.top().push_back( yaal::move( alternative.front() ) );
			}
			raw.clear();
			continue;
		}
		if ( ( cp == ',' ) && ( parts.get_size() > 1 ) ) {
			HConcatenation::cyclic_strings_t concatenation( yaal::move( parts.top() ) );
			parts.pop();
			HConcatenation::cyclic_strings_t& alternative( parts.top() );
			concatenation.push_back( make_resource<HLitaral>( raw ) );
			if ( concatenation.get_size() > 1 ) {
				alternative.push_back( make_resource<HConcatenation>( yaal::move( concatenation ) ) );
			} else {
				alternative.push_back( yaal::move( concatenation.front() ) );
			}
			parts.push( HConcatenation::cyclic_strings_t() );
			raw.clear();
			continue;
		}
		raw.push_back( cp );
	}
	M_ASSERT( parts.get_size() == 1 );
	HConcatenation::cyclic_strings_t concatenation( yaal::move( parts.top() ) );
	if ( concatenation.is_empty() ) {
		braceExpander = make_resource<HLitaral>( raw );
	} else if ( concatenation.get_size() > 1 ) {
		if ( ! raw.is_empty() ) {
			concatenation.push_back( make_resource<HLitaral>( raw ) );
		}
		braceExpander = make_resource<HConcatenation>( yaal::move( concatenation ) );
	} else if ( ! raw.is_empty() ) {
		concatenation.push_back( make_resource<HLitaral>( raw ) );
		braceExpander = make_resource<HConcatenation>( yaal::move( concatenation ) );
	} else {
		braceExpander = yaal::move( concatenation.front() );
	}
	return braceExpander;
	M_EPILOG
}

//
// aa{bb,{cc,dd}ee}ff{gg,hh}ii
// aabbffggii aabbffhhii aacceeffggii aacceeffhhii aaddeeffggii aaddeeffhhii
//

tokens_t brace_expansion( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	tokens_t exploded;
	HCyclicString::res_t braceExpander( brace_expander( str_ ) );
	HString s;
	while( true ) {
		bool last( braceExpander->scan( s, true ) );
		exploded.push_back( s );
		if ( last ) {
			break;
		}
	}
	return exploded;
	M_EPILOG
}

}

