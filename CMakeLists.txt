cmake_minimum_required( VERSION 3.4 )
set( CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../yaal/_aux/cmake ${CMAKE_MODULE_PATH} )
project( huginn CXX )
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
set_target_properties( ${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX OUTPUT_NAME 1exec RUNTIME_OUTPUT_DIRECTORY_DEBUG ${TARGET_PATH}/huginn RUNTIME_OUTPUT_DIRECTORY_DEBUG ${TARGET_PATH}/huginn RUNTIME_OUTPUT_DIRECTORY_RELEASE ${TARGET_PATH}/huginn RUNTIME_OUTPUT_DIRECTORY_COV ${TARGET_PATH}/huginn RUNTIME_OUTPUT_DIRECTORY_PROF ${TARGET_PATH}/huginn )
if ( CMAKE_HOST_WIN32 )
	set_source_files_properties( ${CHILD_SRCS} COMPILE_FLAGS "/FIyaal/fix.hxx" )
	target_link_libraries( ${PROJECT_NAME} ${yaal_LIBRARIES} libintl )

	file( GLOB makedef_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/makedef.vc*proj )
	include_external_msproject( makedef ${makedef_PROJ} )

	file( GLOB msvcxx_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/msvcxx.vc*proj )
	include_external_msproject( msvcxx ${msvcxx_PROJ} )

	file( GLOB hcore_static_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/hcore-static.vc*proj )
	include_external_msproject( hcore-static ${hcore_static_PROJ} msvcxx )
	file( GLOB hcore_def_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/hcore-def.vc*proj )
	include_external_msproject( hcore-def ${hcore_def_PROJ} makedef )
	file( GLOB hcore_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/hcore.vc*proj )
	include_external_msproject( hcore ${hcore_PROJ} hcore-def  )

	file( GLOB tools_static_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/tools-static.vc*proj )
	include_external_msproject( tools-static ${tools_static_PROJ} )
	file( GLOB tools_def_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/tools-def.vc*proj )
	include_external_msproject( tools-def ${tools_def_PROJ} hcore )
	file( GLOB tools_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/tools.vc*proj )
	include_external_msproject( tools ${tools_PROJ} )

	file( GLOB dbwrapper_static_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/dbwrapper-static.vc*proj )
	include_external_msproject( dbwrapper-static ${dbwrapper_static_PROJ} )
	file( GLOB dbwrapper_def_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/dbwrapper-def.vc*proj )
	include_external_msproject( dbwrapper-def ${dbwrapper_def_PROJ} tools )
	file( GLOB dbwrapper_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/dbwrapper.vc*proj )
	include_external_msproject( dbwrapper ${dbwrapper_PROJ} )

	file( GLOB hconsole_static_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/hconsole-static.vc*proj )
	include_external_msproject( hconsole-static ${hconsole_static_PROJ} )
	file( GLOB hconsole_def_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/hconsole-def.vc*proj )
	include_external_msproject( hconsole-def ${hconsole_def_PROJ} dbwrapper tools )
	file( GLOB hconsole_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/hconsole.vc*proj )
	include_external_msproject( hconsole ${hconsole_PROJ} )

	file( GLOB hdata_static_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/hdata-static.vc*proj )
	include_external_msproject( hdata-static ${hdata_static_PROJ} )
	file( GLOB hdata_def_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/hdata-def.vc*proj )
	include_external_msproject( hdata-def ${hdata_def_PROJ} hconsole )
	file( GLOB hdata_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/hdata.vc*proj )
	include_external_msproject( hdata ${hdata_PROJ} )

	file( GLOB sqlite3_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/sqlite3.vc*proj )
	include_external_msproject( sqlite3 ${sqlite3_PROJ} )

	file( GLOB headers_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/headers.vc*proj )
	include_external_msproject( headers ${headers_PROJ} )

	file( GLOB mkheaders_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/mkheaders.vc*proj )
	include_external_msproject( mkheaders ${mkheaders_PROJ} )

	file( GLOB update_commit_id_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/update-commit-id.vc*proj )
	include_external_msproject( update-commit-id ${update_commit_id_PROJ} )

	file( GLOB INSTALL_PROJ RELATIVE ${CMAKE_HOME_DIRECTORY} ../yaal/INSTALL.vc*proj )
	include_external_msproject( INSTALL ${INSTALL_PROJ} hdata mkheaders )
	add_dependencies( huginn INSTALL )
else()
	add_dependencies( ${PROJECT_NAME} commit_id )
	target_link_libraries( ${PROJECT_NAME} ${Yaal_LIBRARIES} ${READLINE} )
endif()

