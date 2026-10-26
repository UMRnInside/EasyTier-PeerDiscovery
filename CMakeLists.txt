cmake_minimum_required(VERSION 3.16)
project(easytier-peerdisc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ARGON2 REQUIRED IMPORTED_TARGET libargon2)

# Uthash is a header-only library: expose its include dir only.
set(UTHASH_DIR ${CMAKE_SOURCE_DIR}/3rdparty/uthash/src)

set(DHT_SRCS ${CMAKE_SOURCE_DIR}/3rdparty/dht/dht.c)
set(DHT_INCLUDES ${CMAKE_SOURCE_DIR}/3rdparty/dht)

add_executable(easytier-peerdisc
    ${DHT_SRCS}
    src/main.c
    src/args.c
    src/dht_runtime.c
    src/dht_state.c
    src/infohash.c
    src/discovery.c
    src/httpd.c
    src/endpoint.c
    src/easytier_rpc.c
)

add_executable(et-secret-hash
    src/secret_hash.c
    src/infohash.c
)

target_include_directories(easytier-peerdisc PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${DHT_INCLUDES}
    ${UTHASH_DIR}
)

target_link_libraries(easytier-peerdisc PRIVATE OpenSSL::SSL OpenSSL::Crypto PkgConfig::ARGON2)
if(WIN32)
    target_link_libraries(easytier-peerdisc PRIVATE shell32 ole32 uuid)
endif()

target_include_directories(et-secret-hash PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(et-secret-hash PRIVATE OpenSSL::Crypto PkgConfig::ARGON2)
target_compile_features(et-secret-hash PRIVATE c_std_11)

# Keep the submodule pristine; compile dht.c as part of our target.
set_target_properties(easytier-peerdisc PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

install(TARGETS easytier-peerdisc et-secret-hash RUNTIME DESTINATION bin)

enable_testing()

add_executable(peerdisc_tests
    tests/test_units.c
    src/infohash.c
    src/endpoint.c
    ${CMAKE_SOURCE_DIR}/3rdparty/unity/src/unity.c
)
target_include_directories(peerdisc_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3rdparty/unity/src
    ${UTHASH_DIR}
)
target_link_libraries(peerdisc_tests PRIVATE OpenSSL::SSL OpenSSL::Crypto PkgConfig::ARGON2)
target_compile_features(peerdisc_tests PRIVATE c_std_11)

add_test(NAME core COMMAND peerdisc_tests)
