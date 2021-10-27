#! /bin/sh
export target=${TARGET:-debug}
exec ./build/${target}/huginn/1exec -E "${0}" "${@}"
#! huginn

import Text as text;
import RegularExpressions as re;
import OperatingSystem as os;
import FileSystem as fs;

reformat( raw_ ) {
	reformatter = os.spawn( "./build/{}/huginn/1exec".format( os.env( "target" ) ), ["--reformat"], false );
	rawStream = text.stream( raw_ );
	rawStream.pump_to( reformatter.in() );
	reformatter.in().close();
	formatted = reformatter.out().read_string( 10000 );
	reformatter.wait();
	return ( formatted );
}

run_test( raw_, expected_ ) {
	actual = reformat( raw_ );
	if ( actual == expected_ ) {
		actual = reformat( actual );
	}
	success = actual == expected_;
	print( success ? "." : "F" );
	if ( ! success && ( os.env( "verbose" ) != none ) ) {
		print( "\nactual:\n------------------------------------------------------------------------\n" );
		print( actual );
		print( "\n------------------------------------------------------------------------\n" );
	}
	return ( success );
}

run_suite( path_ ) {
	rawRe = re.compile( "^# raw " );
	expectedRe = re.compile( "^# expected$" );
	inRaw = false;
	inExpected = false;
	raw = "";
	expected = "";
	name = "";
	gotTest = false;
	failures = [];
	suiteName = path_[20:];
	lineNo = 0;
	currLine = 1;
	print( "{}: ".format( suiteName ) );
	for ( line : fs.open( path_, fs.OPEN_MODE.READ ) ) {
		if ( ! inRaw && rawRe.match( line ).matched() ) {
			if ( gotTest && ! run_test( raw, expected ) ) {
				failures.push( "{}:{}:{}".format( suiteName, lineNo, name ) );
			}
			raw = "";
			expected = "";
			inExpected = false;
			inRaw = true;
			name = line.strip()[6:];
			lineNo = copy( currLine );
		} else if ( ! inExpected && expectedRe.match( line ).matched() ) {
			gotTest = true;
			inExpected = true;
			inRaw = false;
		} else if ( inRaw ) {
			raw += line;
		} else if ( inExpected ) {
			expected += line;
		}
		currLine += 1;
	}
	if ( gotTest && ! run_test( raw, expected ) ) {
		failures.push( "{}:{}:{}".format( suiteName, lineNo, name ) );
	}
	print( "\n" );
	return ( failures );
}

main( argv_ ) {
	failures = [];
	globPattern = "./tests/data/reformat/*{}*.hgn".format( size( argv_ ) > 1 ? argv_[1] : "" );
	for ( p : fs.glob( globPattern ) ) {
		failures += run_suite( p );
	}
	failureCount = size( failures );
	if ( failureCount > 0 ) {
		print( "\nFailed tests:\n" );
		for ( f : failures ) {
			print( "{}\n".format( f ) );
		}
	}
	return ( failureCount );
}

