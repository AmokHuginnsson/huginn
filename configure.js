var fs = new ActiveXObject( "Scripting.FileSystemObject" );
var envYaalSourcePath = WScript.createObject( "WScript.Shell" ).environment( "Process" )( "YAAL_SOURCE_PATH" );
var yaalSourcePath = fs.getParentFolderName( fs.getFile( WScript.scriptFullName ) ) + "/../yaal";
eval( fs.openTextFile( ( envYaalSourcePath != "" ? envYaalSourcePath : yaalSourcePath ) + "/configure.js", 1 ).readAll() );
