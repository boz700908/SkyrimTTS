# SkyrimNVDA - SKSE accessibility plugin for Skyrim SE/AE/VR
# Vocalizes game menus via NVDA screen reader for blind players

cmake_minimum_required(VERSION 3.21)

project(SkyrimNVDA VERSION 0.1.0 LANGUAGES CXX)

# Deploy to Skyrim Data folder if set
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()

# Deploy to mod manager folder if set
if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
endif()

# Setup SKSE plugin
find_package(CommonLibSSE CONFIG REQUIRED)
add_commonlibsse_plugin(${PROJECT_NAME} SOURCES src/plugin.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

# Include paths: src/ for our code, project root for gfx/ headers
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}"
)

# --- nvdaControllerClient (in lib/) ---
target_link_libraries(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/lib/nvdaControllerClient.lib")

# XInput : pour lire l'etat physique du gamepad (detection LB "colle")
target_link_libraries(${PROJECT_NAME} PRIVATE "Xinput.lib")

# Copy nvdaControllerClient.dll next to the plugin DLL so it can load at runtime
add_custom_command(
    TARGET "${PROJECT_NAME}"
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/lib/nvdaControllerClient.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/nvdaControllerClient.dll"
    VERBATIM
)

# Deploy compiled plugin automatically if OUTPUT_FOLDER is defined
if(DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:${PROJECT_NAME}>"
                "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_SOURCE_DIR}/lib/nvdaControllerClient.dll"
                "${DLL_FOLDER}/nvdaControllerClient.dll"
        VERBATIM
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
                    "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()

    # Deploy AutoWalk ESP + PEX to Skyrim Data folder
    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_SOURCE_DIR}/autowalk/SkyrimTTS_AutoWalk.esp"
                "${OUTPUT_FOLDER}/SkyrimTTS_AutoWalk.esp"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT_FOLDER}/Scripts"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_SOURCE_DIR}/autowalk/SkyrimTTS_AutoWalk.pex"
                "${OUTPUT_FOLDER}/Scripts/SkyrimTTS_AutoWalk.pex"
        VERBATIM
    )
endif()
