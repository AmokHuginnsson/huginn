function make_absolute( [string]$path ) {
	if ( -Not( [System.IO.Path]::IsPathRooted( $path ) ) ) {
		$path = [IO.Path]::GetFullPath( [IO.Path]::Combine( ( ($pwd).Path ), ( $path ) ) )
	}
	return $path.Replace( "\", "/" )
}

Select-String -ErrorAction Ignore -Path "local.js" -Pattern "PREFIX\s=\s[`"]([^`"]+)[`"]" | ForEach-Object {
	$prefix = make_absolute( "$($_.Matches.groups[1])" )
}
$origEnvPath=$env:Path
$env:Path="$prefix\bin;$env:Path"
$env:YAAL_AUTO_SANITY=1
$env:HUGINN_INIT="./src/init.sample"
$env:HUGINN_INIT_SHELL="./src/init.shell.sample"
$env:HUGINN_RC_SHELL="./src/rc.shell.sample"
.\build\debug\huginn\1exec -M packages $args
$env:Path=$origEnvPath

