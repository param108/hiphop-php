OPTION(INFINITE_LOOP_DETECTION "Enable Infinite Loop Detection" ON)
OPTION(INFINITE_RECURSION_DETECTION "Enable Infinite Recursion Detection" ON)
OPTION(REQUEST_TIMEOUT_DETECTION "Enable Timeout Detection" ON)

SET(CMAKE_LIBRARY_PATH /opt/hiphop/gcc4.3.5/lib /opt/hiphop/gcc4.3.5/lib64 ${CM
SET(CMAKE_INCLUDE_PATH /opt/hiphop/gcc4.3.5/include ${CMAKE_INCLUDE_PATH})

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_COMPILER_IS_GNUCC)
	INCLUDE(CheckCSourceCompiles)
	CHECK_C_SOURCE_COMPILES("#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VERSION < 40300
#error Need GCC 4.3.0+
#endif
int main() { return 0; }" HAVE_GCC_43)

	if(NOT HAVE_GCC_43)
		message(FATAL_ERROR "Need at least GCC 4.3")
	endif()

endif()

set(FREEBSD FALSE)
set(LINUX FALSE)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD")
	set(FREEBSD TRUE)	
endif()
	
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	set(LINUX TRUE)
endif()

LIST(APPEND CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")

if(APPLE)
	if(EXISTS "/opt/local/var/macports/")
		LIST (APPEND CMAKE_PREFIX_PATH "/opt/local")
		LIST (APPEND CMAKE_LIBRARY_PATH "/opt/local/lib/x86_64")
	endif()
endif()

include(HPHPFunctions)
include(HPHPFindLibs)

add_definitions(-D_GNU_SOURCE -D_REENTRANT=1 -D_PTHREADS=1)

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
	add_definitions(-DRELEASE=1)
endif()

if(APPLE OR FREEBSD)
	add_definitions(-DSKIP_USER_CHANGE=1)
endif()

# eable the OSS options if we have any
add_definitions(-DHPHP_OSS=1)

set(HPHP_OPT "-g")

set(CMAKE_C_FLAGS "${HPHP_OPT} -w -fPIC")
set(CMAKE_CXX_FLAGS "${HPHP_OPT} -fPIC -fno-omit-frame-pointer -ftemplate-depth-60 -Wall -Woverloaded-virtual -Wno-deprecated -Wno-parentheses -Wno-strict-aliasing -Wno-write-strings -Wno-invalid-offsetof")

include_directories(${HPHP_HOME}/src)
include_directories(${HPHP_HOME}/src/lib/system/gen)
