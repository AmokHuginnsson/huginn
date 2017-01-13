var fs = new ActiveXObject( "Scripting.FileSystemObject" );
eval( fs.openTextFile( fs.getParentFolderName( fs.getFile( WScript.scriptFullName ) ) + "/../yaal/configure.js", 1 ).readAll() );
