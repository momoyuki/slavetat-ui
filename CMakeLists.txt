cmake_minimum_required(VERSION 3.21)
message("Using toolchain file ${CMAKE_TOOLCHAIN_FILE}.")

project(SlaveTatsUI VERSION 0.1.0 DESCRIPTION "SlaveTats Prisma UI bridge plugin" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(CTest)

set(headers
    include/PrismaUI_API.h
    include/SlaveTatsNG_Interface.h
    include/JContainers/jc_interface.h
    include/JContainers/jcontainers_constants.h
    include/jcontainers_mini.h
    src/pch.h
    src/Bridge.h
    src/adapters/PrismaTattooSerializer.h
    src/core/ITattooRuntime.h
    src/core/SlaveTatsService.h
    src/core/TattooModels.h
    src/runtime/SlaveTatsRuntime.h
    src/repository/TattooCatalogLoader.h
    src/repository/TattooCatalogStore.h
    src/repository/TattooRepository.h
    src/repository/TattooSourceParser.h
    src/repository/TattooSourceScanner.h
    src/textures/ExactStreamReader.h
    src/textures/TextureResolver.h
)

set(sources
    src/main.cpp
    src/Bridge.cpp
    src/adapters/PrismaTattooSerializer.cpp
    src/core/SlaveTatsService.cpp
    src/runtime/SlaveTatsRuntime.cpp
    src/repository/TattooCatalogLoader.cpp
    src/repository/TattooCatalogStore.cpp
    src/repository/TattooRepository.cpp
    src/repository/TattooSourceParser.cpp
    src/repository/TattooSourceScanner.cpp
    src/textures/TextureResolver.cpp
)

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME} SOURCES ${headers} ${sources})
add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS "${PROJECT_NAME}")

target_include_directories(${PROJECT_NAME}
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    nlohmann_json::nlohmann_json
    Microsoft::DirectXTex
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

if(BUILD_TESTING)
    add_executable(SlaveTatsUICoreTests
        src/core/SlaveTatsService.cpp
        tests/core/SlaveTatsServiceTests.cpp
    )
    target_include_directories(SlaveTatsUICoreTests PRIVATE src)
    add_test(NAME SlaveTatsUICoreTests COMMAND SlaveTatsUICoreTests)

    add_executable(PrismaTattooSerializerTests
        src/adapters/PrismaTattooSerializer.cpp
        tests/adapters/PrismaTattooSerializerTests.cpp
    )
    target_include_directories(PrismaTattooSerializerTests PRIVATE src)
    add_test(NAME PrismaTattooSerializerTests COMMAND PrismaTattooSerializerTests)

    add_executable(TattooSourceScannerTests
        src/repository/TattooSourceScanner.cpp
        tests/repository/TattooSourceScannerTests.cpp
    )
    target_include_directories(TattooSourceScannerTests PRIVATE src)
    add_test(NAME TattooSourceScannerTests COMMAND TattooSourceScannerTests)

    add_executable(ExactStreamReaderTests
        tests/textures/ExactStreamReaderTests.cpp
    )
    target_include_directories(ExactStreamReaderTests PRIVATE src)
    add_test(NAME ExactStreamReaderTests COMMAND ExactStreamReaderTests)

    add_executable(TattooSourceParserTests
        src/repository/TattooSourceParser.cpp
        tests/repository/TattooSourceParserTests.cpp
    )
    target_include_directories(TattooSourceParserTests PRIVATE src)
    target_link_libraries(TattooSourceParserTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME TattooSourceParserTests COMMAND TattooSourceParserTests)

    add_executable(TattooRepositoryTests
        src/repository/TattooRepository.cpp
        tests/repository/TattooRepositoryTests.cpp
    )
    target_include_directories(TattooRepositoryTests PRIVATE src)
    add_test(NAME TattooRepositoryTests COMMAND TattooRepositoryTests)

    add_executable(TattooCatalogLoaderTests
        src/repository/TattooCatalogLoader.cpp
        src/repository/TattooRepository.cpp
        src/repository/TattooSourceParser.cpp
        src/repository/TattooSourceScanner.cpp
        tests/repository/TattooCatalogLoaderTests.cpp
    )
    target_include_directories(TattooCatalogLoaderTests PRIVATE src)
    target_link_libraries(TattooCatalogLoaderTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME TattooCatalogLoaderTests COMMAND TattooCatalogLoaderTests)

    add_executable(TattooCatalogStoreTests
        src/repository/TattooCatalogLoader.cpp
        src/repository/TattooCatalogStore.cpp
        src/repository/TattooRepository.cpp
        src/repository/TattooSourceParser.cpp
        src/repository/TattooSourceScanner.cpp
        tests/repository/TattooCatalogStoreTests.cpp
    )
    target_include_directories(TattooCatalogStoreTests PRIVATE src)
    target_link_libraries(TattooCatalogStoreTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME TattooCatalogStoreTests COMMAND TattooCatalogStoreTests)

    add_executable(TextureResolverTests
        src/textures/TextureResolver.cpp
        tests/textures/TextureResolverTests.cpp
    )
    target_include_directories(TextureResolverTests PRIVATE src)
    add_test(NAME TextureResolverTests COMMAND TextureResolverTests)
endif()
