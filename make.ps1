param (
	[Parameter(Mandatory=$True)] [string[]]$target,
	[Parameter(Mandatory=$False)] [string]$prefix = "$pwd/build/windows",
	[Parameter(Mandatory=$False)] [string]$EXTRA_FLAGS,
	[Parameter(Mandatory=$False)] [string]$BUILD_ID,
	[Parameter(Mandatory=$False)] [switch]$auto_setup
)

function make_absolute( [string]$path ) {
	if ( -Not( [System.IO.Path]::IsPathRooted( $path ) ) ) {
		$path = [IO.Path]::GetFullPath( [IO.Path]::Combine( ( ($pwd).Path ), ( $path ) ) )
	}
	return $path.Replace( "\", "/" )
}

function purge {
	Write-Host -NoNewline "Purging... "
	Remove-Item "build" -Recurse -ErrorAction Ignore
	Write-Host "done."
}

function build( [string]$config, [string]$extraFlags ) {
	New-Item -ItemType Directory -Force -Path "build/$config" > $null
	Push-Location "build/$config"
	cscript ../../configure.js PROJECT_ROOT=../../ BUILD_TYPE=$config $extraFlags
	cmake --build . --config $config
	Pop-Location
}

function install( [string]$config ) {
	Push-Location "build/$config"
	cmake --build . --target install --config $config
	Pop-Location
}

function debug( [string]$extraFlags = $EXTRA_FLAGS ) {
	build "debug" $extraFlags
}

function release( [string]$extraFlags = $EXTRA_FLAGS ) {
	build "release" $extraFlags
}

function install-debug {
	debug
	install "debug"
}

function install-release {
	release
	install "release"
}

function package {
	debug "BUILD_PACKAGE=1"
	release "BUILD_PACKAGE=1"
	$packagePath = make_absolute( "build/msi" )
	Push-Location "build/release"
	cpack -B $packagePath
	Pop-Location
}

function bundle {
	package
	$version = ""
	Select-String `
		-ErrorAction Ignore `
		-Path "Makefile.mk.in" `
		-Pattern "VERSION\s*=\s*(\d+)" `
	| ForEach-Object {
		if ( $version -ne "" ) {
			$version += "."
		}
		$version += "$($_.Matches.groups[1])"
	}
	$bundlePath = "build/huginn-deploy/windows/$version"
	Remove-Item "build/huginn-deploy" -Recurse -ErrorAction Ignore
	New-Item -ItemType Directory -Force -Path "$bundlePath" > $null
	$sys = "win32"
	$tag = $version
	if ( $BUILD_ID -ne "" ) {
		$tag += "-$BUILD_ID"
	}
	Move-Item `
		-Path build/msi/huginn-$version-$sys.msi `
		-Destination "$bundlePath/huginn-$tag-$sys.msi" `
		-Force
	Compress-Archive -Path build/huginn-deploy -DestinationPath build/huginn-deploy.zip -Force
}

function auto_setup( $parameters ) {
	if ( $parameters.ContainsKey( "EXTRA_FLAGS" ) ) {
		throw "You cannot specify EXTRA_FLAGS in auto_setup mode!"
	}
	if ( -Not( Test-Path( "$prefix/bin/yaal_tools.dll" ) ) ) {
		New-Item -ItemType Directory -Force -Path "build/cache" > $null
		$out = "build/cache/yaal.msi"
		if ( -Not( Test-Path( $out ) ) ) {
			$yaalPackage = (
				( Invoke-WebRequest -Uri "https://codestation.org/windows/" ).Links | Where-Object { $_.href -like "yaal*" }
			).href
			$EXTRA_FLAGS="EXTRA_FLAGS=YAAL_AUTO_SANITY"
			$uri = "https://codestation.org/windows/$yaalPackage"
			Invoke-WebRequest -Uri $uri -OutFile $out
		}
		get-wmiobject Win32_Product | Where-Object { $_.Name -like "yaal" } | ForEach-Object {
			$yaalGUID = $_.IdentifyingNumber
			Start-Process msiexec -ArgumentList "/uninstall $yaalGUID /q" -wait
		}
		$installRoot = $prefix.Replace( "/", "\\" )
		Start-Process $out -ArgumentList "INSTALL_ROOT=$installRoot /q" -wait
	}
	if ( -Not( Test-Path( "$prefix/bin/replxx.dll" ) ) ) {
		New-Item -ItemType Directory -Force -Path "build/cache" > $null
		$out = "build/cache/replxx.zip"
		if ( -Not( Test-Path( $out ) ) ) {
				$EXTRA_FLAGS="EXTRA_FLAGS=YAAL_AUTO_SANITY"
				$uri = "https://codestation.org/download/replxx.zip"
				Invoke-WebRequest -Uri $uri -OutFile $out
		}
		Expand-Archive -LiteralPath $out -DestinationPath "$prefix/" -Force
	}
}

foreach ( $t in $target ) {
	if (
		( $t -ne "debug" ) -and
		( $t -ne "release" ) -and
		( $t -ne "install-debug" ) -and
		( $t -ne "install-release" ) -and
		( $t -ne "purge" ) -and
		( $t -ne "package" ) -and
		( $t -ne "bundle" )
	) {
		Write-Error "Unknown target: ``$t``"
		exit 1
	}
}

try {
	$origEnvPath=$env:Path
	$stackSize = ( Get-Location -Stack ).Count
	Push-Location $PSScriptRoot
	if ( Test-Path( "local.js" ) ) {
		Select-String -ErrorAction Ignore -Path "local.js" -Pattern "PREFIX\s*=\s*[`"]([^`"]+)[`"]" | ForEach-Object {
			$prefix = make_absolute( "$($_.Matches.groups[1])" )
		}
	} elseif ( $auto_setup ) {
		$prefix = make_absolute( $prefix )
		$local_js = (
			"PREFIX = `"$prefix`";`n" +
			"SYSCONFDIR = `"$prefix/etc`";`n" +
			"DATADIR = `"$prefix/share`";`n" +
			"LOCALSTATEDIR = `"$prefix/var`";`n" +
			"EXTRA_INCLUDE_PATH = `"$prefix/include`";`n" +
			"EXTRA_LIBRARY_PATH = `"$prefix/lib`";`n" +
			"VERBOSE = 1;`n" +
			"FAST = 1;`n"
		)
		$Utf8NoBomEncoding = New-Object System.Text.UTF8Encoding $False
		[System.IO.File]::WriteAllText( "$pwd/local.js", $local_js, $Utf8NoBomEncoding )
	}
	if ( $auto_setup ) {
		auto_setup $PSBoundParameters
	}
	$env:Path = ( $env:Path.Split( ';')  | Where-Object { -Not( $_.ToLower().Contains( "cygwin" ) ) } ) -join ';'
	$env:Path = "$prefix\bin;$env:Path"
	foreach ( $t in $target ) {
		&$t
	}
} catch {
	Write-Error "$_"
} finally {
	while ( ( Get-Location -Stack ).Count -gt $stackSize ) {
		Pop-Location
	}
	$env:Path=$origEnvPath
}

