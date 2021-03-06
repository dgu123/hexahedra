cmake_minimum_required (VERSION 2.8.3)
set(EXE hexahedra-server)
set(LIBNAME hexaserver)

file(GLOB SOURCE_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.cpp" "*/*.cpp" "../../libs/luabind/*.cpp" "../../libs/clew/clew.c" "linenoise.c")
list(REMOVE_ITEM SOURCE_FILES main.cpp)
file(GLOB HEADER_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.hpp" "*/*.hpp")

source_group(include FILES ${HEADER_FILES})
source_group(source  FILES ${SOURCE_FILES})

add_library(${LIBNAME} ${SOURCE_FILES} ${HEADER_FILES})

include_directories(../.. ../../libs)
link_directories(..)

set(ADD_DL dl)
set(BOOST_THREAD_SUFFIX "")
set(RC_OBJ_FILE "")
if(WIN32)
  find_package(SFML 2 REQUIRED COMPONENTS main)
  set(ADD_DL "")
  set(ADDITIONAL_BOOST_LIBS date_time regex)
  set(RC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../install/hexahedra-server.rc)
  if(MINGW)
      set(BOOST_THREAD_SUFFIX "_win32")
      set(RC_OBJ_FILE ${CMAKE_CURRENT_BINARY_DIR}/server_win_rc.o)
      add_custom_command(OUTPUT ${RC_OBJ_FILE}
                         COMMAND ${CMAKE_RC_COMPILER} -I{CMAKE_CURRENT_SOURCE_DIR}
                         -i${RC_FILE} -o${RC_OBJ_FILE}
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  else()
     set(RC_OBJ_FILE ${RC_FILE})
  endif()
endif()

add_executable(${EXE} WIN32 main.cpp ${RC_OBJ_FILE} ${HEADER_FILES})

find_package(Boost ${REQUIRED_BOOST_VERSION} REQUIRED COMPONENTS chrono iostreams program_options filesystem system signals thread${BOOST_THREAD_SUFFIX} ${ADDITIONAL_BOOST_LIBS})
include_directories(${Boost_INCLUDE_DIRS} ${LEVELDB_INCLUDE_DIR})
target_link_libraries(${LIBNAME} ${Boost_LIBRARIES} ${SFML_LIBRARIES})

find_package(LuaJIT)
if(LUAJIT_FOUND)
  include_directories(${LUAJIT_INCLUDE_DIR})
  target_link_libraries(${LIBNAME} ${LUAJIT_LIBRARIES})
else(LUAJIT_FOUND)
  find_package(Lua51)
  if(LUA51_FOUND)
    message("LuaJIT not found, using Lua instead.")
    include_directories(${LUA_INCLUDE_DIR})
    target_link_libraries(${LIBNAME} ${LUA_LIBRARY})
  else(LUA51_FOUND)
    message(SEND_ERROR "The server requires either LuaJIT or Lua")
  endif(LUA51_FOUND)
endif(LUAJIT_FOUND)

set(LIBS ENet ES HexaNoise ZLIB GLM PNG)
foreach (LIB ${LIBS})
    find_package(${LIB} REQUIRED)
    string(TOUPPER ${LIB} ULIB)
    include_directories(${${ULIB}_INCLUDE_DIR})
    include_directories(${${ULIB}_INCLUDE_DIRS})
    target_link_libraries(${LIBNAME} ${${ULIB}_LIBRARY})
    target_link_libraries(${LIBNAME} ${${ULIB}_LIBRARIES})
endforeach()

set(DL "")
if(WIN32)
    target_link_libraries(${EXE} ws2_32 winmm)
else()
    set(DL "dl")
endif()

target_link_libraries(${EXE} hexaserver hexacommon ${DL})

# Installation
install(TARGETS ${EXE} DESTINATION "${BINDIR}")

