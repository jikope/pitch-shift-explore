cmake_minimum_required(VERSION 3.20)
project(pitch-shift LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main  # or a specific tag like "release-3.2.0"
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

FetchContent_Declare(
    SDL3_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(SDL3_mixer)

FetchContent_Declare(
    kissfft
    GIT_REPOSITORY https://github.com/mborgerding/kissfft.git
    GIT_TAG        131.2.0
)
FetchContent_MakeAvailable(kissfft)


add_executable(pitch-shift src/main.cpp src/Waveform.cpp)
# target_compile_options(pitch-shift PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
# target_link_options(pitch-shift PRIVATE -fsanitize=address)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer kissfft::kissfft-float)
target_include_directories(${PROJECT_NAME} PRIVATE include)
