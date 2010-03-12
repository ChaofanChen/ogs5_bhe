# - Try to find LIS
# Once done, this will define
#
#  LIS_FOUND
#  LIS_INCLUDE_DIRS
#  LIS_LIBRARIES

if (NOT LIS_FOUND)

	include(LibFindMacros)
	
	find_path( LIS_INCLUDE_DIR
		NAMES lis.h
		PATHS ${CMAKE_SOURCE_DIR}/LIB)

	if ( UNIX )
		find_library(LIS_LIBRARIES
			NAMES lis-32
			PATHS ${CMAKE_SOURCE_DIR}/LIB )	
	else ( UNIX )			
		find_library(LIS_LIBRARIES
			NAMES lis-32
			PATHS ${CMAKE_SOURCE_DIR}/LIB )	
	endif ( UNIX )

	# Set the include dir variables and the libraries and let libfind_process do the rest.
	# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
	if (NOT LIS_LIBRARIES STREQUAL "LIS_LIBRARIES-NOTFOUND" AND NOT LIS_INCLUDE_DIR STREQUAL "LIS_INCLUDE_DIR-NOTFOUND")
		set(LIS_PROCESS_INCLUDES LIS_INCLUDE_DIR)
		set(LIS_PROCESS_LIBS LIS_LIBRARIES)
		libfind_process(LIS)
	else (NOT LIS_LIBRARIES STREQUAL "LIS_LIBRARIES-NOTFOUND" AND NOT LIS_INCLUDE_DIR STREQUAL "LIS_INCLUDE_DIR-NOTFOUND")
		message (STATUS "Warning: LIS not found!")
	endif (NOT LIS_LIBRARIES STREQUAL "LIS_LIBRARIES-NOTFOUND" AND NOT LIS_INCLUDE_DIR STREQUAL "LIS_INCLUDE_DIR-NOTFOUND")
	
endif (NOT LIS_FOUND)
