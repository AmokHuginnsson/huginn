cmake_minimum_required( VERSION 3.4 )
set( CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../yaal/_aux/cmake ${CMAKE_MODULE_PATH} )
project( huginn CXX )

set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "huginn - programming language with no quirks, so simple every child can master it.")
set( CPACK_WIX_UPGRADE_GUID "436f6465-5374-6174-696f-6e2068756769" )
set( CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "." )

include( lib )
include( common )

if ( CMAKE_HOST_WIN32 )
	add_definitions( /D_HAS_ITERATOR_DEBUGGING=0 )
endif()

use_cxx_compiler_flag( -Wno-missing-noreturn )
find_package( Yaal COMPONENTS hcore tools dbwrapper hconsole hdata REQUIRED )
find_library( READLINE readline )

include_directories( . )
configure_file( ../yaal/_aux/cmake/client-config.hxx ${TARGET_PATH}/config.hxx )

file( GLOB SRCS ./src/*.cxx )
file( GLOB HDRS ./src/*.hxx )
add_executable( ${PROJECT_NAME} ${SRCS} ${HDRS} ../yaal/_aux/cmake/client-config.hxx )
set_target_properties(
	${PROJECT_NAME} PROPERTIES
	LINKER_LANGUAGE CXX
	OUTPUT_NAME 1exec
	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${TARGET_PATH}/huginn
	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${TARGET_PATH}/huginn
	RUNTIME_OUTPUT_DIRECTORY_RELEASE ${TARGET_PATH}/huginn
	RUNTIME_OUTPUT_DIRECTORY_COV ${TARGET_PATH}/huginn
	RUNTIME_OUTPUT_DIRECTORY_PROF ${TARGET_PATH}/huginn
)
if ( CMAKE_HOST_WIN32 )
	add_definitions( -D_WIN32 -DUSE_READLINE_STATIC )
	target_link_libraries( ${PROJECT_NAME} ${Yaal_LIBRARIES} ${READLINE} libintl )

	file( GLOB makedef_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/makedef.vc*proj )
	include_external_msproject( makedef ${makedef_PROJ} )

	file( GLOB msvcxx_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/msvcxx.vc*proj )
	include_external_msproject( msvcxx ${msvcxx_PROJ} )

	file( GLOB hcore_static_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/hcore-static.vc*proj )
	include_external_msproject( hcore-static ${hcore_static_PROJ} msvcxx )
	file( GLOB hcore_def_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/hcore-def.vc*proj )
	include_external_msproject( hcore-def ${hcore_def_PROJ} makedef )
	file( GLOB hcore_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/hcore.vc*proj )
	include_external_msproject( hcore ${hcore_PROJ} hcore-def  )

	file( GLOB tools_static_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/tools-static.vc*proj )
	include_external_msproject( tools-static ${tools_static_PROJ} )
	file( GLOB tools_def_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/tools-def.vc*proj )
	include_external_msproject( tools-def ${tools_def_PROJ} hcore )
	file( GLOB tools_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/tools.vc*proj )
	include_external_msproject( tools ${tools_PROJ} )

	file( GLOB dbwrapper_static_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/dbwrapper-static.vc*proj )
	include_external_msproject( dbwrapper-static ${dbwrapper_static_PROJ} )
	file( GLOB dbwrapper_def_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/dbwrapper-def.vc*proj )
	include_external_msproject( dbwrapper-def ${dbwrapper_def_PROJ} tools )
	file( GLOB dbwrapper_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/dbwrapper.vc*proj )
	include_external_msproject( dbwrapper ${dbwrapper_PROJ} )

	file( GLOB hconsole_static_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/hconsole-static.vc*proj )
	include_external_msproject( hconsole-static ${hconsole_static_PROJ} )
	file( GLOB hconsole_def_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/hconsole-def.vc*proj )
	include_external_msproject( hconsole-def ${hconsole_def_PROJ} dbwrapper tools )
	file( GLOB hconsole_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/hconsole.vc*proj )
	include_external_msproject( hconsole ${hconsole_PROJ} )

	file( GLOB hdata_static_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/hdata-static.vc*proj )
	include_external_msproject( hdata-static ${hdata_static_PROJ} )
	file( GLOB hdata_def_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/hdata-def.vc*proj )
	include_external_msproject( hdata-def ${hdata_def_PROJ} hconsole )
	file( GLOB hdata_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/hdata.vc*proj )
	include_external_msproject( hdata ${hdata_PROJ} )

	file( GLOB sqlite3_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/sqlite3.vc*proj )
	include_external_msproject( sqlite3 ${sqlite3_PROJ} )

	file( GLOB headers_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/headers.vc*proj )
	include_external_msproject( headers ${headers_PROJ} )

	file( GLOB mkheaders_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/mkheaders.vc*proj )
	include_external_msproject( mkheaders ${mkheaders_PROJ} )

	file( GLOB update_commit_id_PROJ ${CMAKE_HOME_DIRECTORY}/../yaal/build/${CMAKE_BUILD_TYPE}/update-commit-id.vc*proj )
	include_external_msproject( update-commit-id ${update_commit_id_PROJ} )

	file( RELATIVE_PATH TARGET_PATH_RELATIVE ${CMAKE_HOME_DIRECTORY} ${TARGET_PATH} )
	set( INSTALLED_HEADER "${CMAKE_INSTALL_PREFIX}/include/yaal/yaal.hxx" )
	set( YAAL_TARGET_PATH "${CMAKE_HOME_DIRECTORY}/../yaal/${TARGET_PATH_RELATIVE}" )
	set( GENERATED_HEADERS "${YAAL_TARGET_PATH}/include/yaal" )
#	message( "Info:\n"
#		"\tCMAKE_HOME_DIRECTORY=${CMAKE_HOME_DIRECTORY}\n"
#		"\tCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}\n"
#		"\tTARGET_PATH=${TARGET_PATH}\n"
#		"\tTARGET_PATH_RELATIVE=${TARGET_PATH_RELATIVE}\n"
#		"\tINSTALLED_HEADER=${INSTALLED_HEADER}\n"
#		"\tYAAL_TARGET_PATH=${YAAL_TARGET_PATH}\n"
#		"\tGENERATED_HEADERS=${GENERATED_HEADERS}" )

	add_custom_command( OUTPUT ${INSTALLED_HEADER}
		COMMAND cmd /c "( robocopy /e /np /njh /ndl /njs ${GENERATED_HEADERS} ${CMAKE_INSTALL_PREFIX}/include/yaal ) ^& IF %ERRORLEVEL% LEQ 1 exit /b 0"
		COMMAND cmd /c "( robocopy /xx /np /njh /ndl /njs ${YAAL_TARGET_PATH} ${CMAKE_INSTALL_PREFIX}/lib *.lib ) ^& IF %ERRORLEVEL% LEQ 1 exit /b 0"
		COMMAND cmd /c "( robocopy /xx /np /njh /ndl /njs ${YAAL_TARGET_PATH} ${CMAKE_INSTALL_PREFIX}/bin *.dll ) ^& IF %ERRORLEVEL% LEQ 1 exit /b 0"
		DEPENDS ${GENERATED_HEADERS}/yaal.hxx headers hdata hdata-static hconsole hconsole-static dbwrapper dbwrapper-static tools tools-static hcore hcore-static msvcxx
		VERBATIM )
	add_custom_target( install_yaal DEPENDS ${INSTALLED_HEADER} )

	add_dependencies( huginn install_yaal )

	install( PROGRAMS ${TARGET_PATH}/${PROJECT_NAME}/1exec.exe DESTINATION "." RENAME ${PROJECT_NAME}.exe )
	set( EXTRA_DEPENDENCIES yaal_hcore.dll yaal_tools.dll yaal_dbwrapper.dll yaal_hconsole.dll yaal_sqlite3_driver.dll history5.dll iconv.dll libcrypto-1_1.dll libexslt.dll libiconv-2.dll libintl-2.dll libwinpthread.dll libxml2.dll libxslt.dll readline5.dll pcre.dll sqlite3.dll libssl-1_1.dll zlib1.dll )
	prepend( EXTRA_DEPENDENCIES ${CMAKE_INSTALL_PREFIX}/bin/ ${EXTRA_DEPENDENCIES} )
	install( FILES ${EXTRA_DEPENDENCIES} DESTINATION "." )
	install( FILES huginnrc DESTINATION "." RENAME ".huginnrc" )
	install( FILES ${CMAKE_INSTALL_PREFIX}/etc/yaalrc DESTINATION "." RENAME ".yaalrc" )
else()
	add_dependencies( ${PROJECT_NAME} commit_id )
	target_link_libraries( ${PROJECT_NAME} ${Yaal_LIBRARIES} ${READLINE} )
endif()

