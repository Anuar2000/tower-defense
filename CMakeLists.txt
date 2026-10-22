cmake_minimum_required(VERSION 3.20)
project(tower_defense LANGUAGES C CXX)

# C++20: кестелердегі .max_hp = 30 сияқты аталған өрістер және std::format үшін
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT game)
# Windows: C++ runtime exe ішіне кіреді, басқа компьютерде қосымша DLL керек болмайды
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# --- GLFW және GLM: CMake оларды GitHub-тан өзі жүктейді (курстағыдай) ---
include(FetchContent)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE)  # қосымша пакеттер керек болмасын

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4 GIT_SHALLOW TRUE)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glfw glm)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_executable(game
    src/main.cpp
    src/render.cpp
    src/image.cpp
    src/assets.cpp
    src/tower.cpp
    src/enemies.cpp
    src/turret.cpp
    src/waves.cpp
    src/skills.cpp
    src/shop.cpp
    src/menu.cpp
    src/sounds.cpp
    src/libs.c
    external/glad/src/gl.c
)

target_include_directories(game PRIVATE
    external/glad/include
    external/stb
    external/miniaudio
)

target_link_libraries(game PRIVATE OpenGL::GL glfw glm::glm Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX)
    target_link_libraries(game PRIVATE m)
endif()
if(MSVC)
    target_compile_options(game PRIVATE /utf-8)  # орысша жазулар UTF-8 күйінде қалсын
    # Қара консоль терезесі ашылмасын, бірақ main() сол күйінде қалады
    set_target_properties(game PROPERTIES WIN32_EXECUTABLE ON)
    target_link_options(game PRIVATE /ENTRY:mainCRTStartup)
endif()

# Ассеттер exe-нің жанындағы assets папкасынан оқылады (main.cpp exe папкасына көшеді)
target_compile_definitions(game PRIVATE ASSET_DIR="assets")
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:game>/assets
)
