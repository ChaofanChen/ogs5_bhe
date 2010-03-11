# Source files
set( HEADERS
	Vector3.h
	MathTools.h
)

set( SOURCES
	MathTools.cpp
)

# Create the library
add_library( MathLib STATIC ${HEADERS} ${SOURCES} )

set_target_properties(MathLib PROPERTIES LINKER_LANGUAGE CXX)

include_directories(
	../GEO
	.
)