cmake_minimum_required(VERSION 3.14)

if(DEFINED BUILD_VERSION)
    set(PROJECT_VERSION ${BUILD_VERSION})
else()
    set(PROJECT_VERSION 0.0.0)
endif()

project(DreamDownloader
        VERSION ${PROJECT_VERSION}
        )

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
        
set(CMAKE_PREFIX_PATH "${DEVKITPRO};${CMAKE_PREFIX_PATH}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/version.h"
    @ONLY
)

set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/source/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/net/AcbaaWebServer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/net/HttpClient.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/net/HttpRequest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/helpers/BuildIdHelper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/helpers/dmntcht.c"
    )

message("SOURCES: ${SOURCES}")
message("HEADERS: ${HEADERS}")

add_executable(DreamDownloader
    ${SOURCES}
    ${HEADERS}
    )

target_include_directories(DreamDownloader
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

find_package(PkgConfig REQUIRED)
pkg_check_modules(libcurl REQUIRED IMPORTED_TARGET libcurl)

target_link_libraries(DreamDownloader
    PRIVATE PkgConfig::libcurl
    )

# Generate symbol list and map information (useful for debugging)
dkp_target_generate_symbol_list(${PROJECT_NAME})
    
nx_generate_nacp(DreamDownloader
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/DreamDownloader.nacp"
    NAME "DreamDownloader"
    AUTHOR "Ixaruz"
    VERSION ${PROJECT_VERSION}
    )

nx_create_nro(DreamDownloader
    VERSION ${PROJECT_VERSION}
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/DreamDownloader.nro"
    NACP "${CMAKE_CURRENT_BINARY_DIR}/DreamDownloader.nacp"
    # ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg"
    # ROMFS "${CMAKE_CURRENT_SOURCE_DIR}/romfs"
    )
