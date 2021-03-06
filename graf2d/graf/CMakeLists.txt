############################################################################
# CMakeLists.txt file for building ROOT graf2d/graf package
# @author Pere Mato, CERN
############################################################################

set(libname Graf)

include_directories(${CMAKE_SOURCE_DIR}/hist/hist/inc)  # This is to avoid a circular dependency graf <--> hist

ROOT_GENERATE_DICTIONARY(G__${libname} MODULE ${libname} *.h LINKDEF LinkDef.h OPTIONS "-writeEmptyRootPCM")


include_directories(${FREETYPE_INCLUDE_DIRS})

ROOT_LINKER_LIBRARY(${libname} *.cxx G__${libname}.cxx LIBRARIES ${FREETYPE_LIBRARIES} ${ZLIB_LIBRARY} mathtext DEPENDENCIES Hist Matrix MathCore RIO)

if(builtin_freetype)
  ROOT_ADD_BUILTIN_DEPENDENCIES(${libname} FREETYPE)
endif()
ROOT_INSTALL_HEADERS()
