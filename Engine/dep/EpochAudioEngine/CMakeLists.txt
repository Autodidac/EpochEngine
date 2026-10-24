cmake_minimum_required(VERSION 3.28)
project(EpochAudioEngine VERSION 0.1.0 LANGUAGES CXX)
add_library(epoch_audio_engine STATIC
    src/audio.manager.cpp
    src/audio.mixer.cpp
    src/audio.mixer_contract.cpp
    src/audio.device.cpp
    src/audio.device_contract.cpp
    src/audio.playback_runtime.cpp
    src/audio.playback_runtime_contract.cpp
)
add_library(Epoch::AudioEngine ALIAS epoch_audio_engine)
target_compile_features(epoch_audio_engine PUBLIC cxx_std_23)
set_target_properties(epoch_audio_engine PROPERTIES CXX_EXTENSIONS OFF CXX_SCAN_FOR_MODULES ON POSITION_INDEPENDENT_CODE ON)
target_sources(epoch_audio_engine PUBLIC FILE_SET epoch_audio_engine_modules TYPE CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
    FILES
        modules/audio.types.ixx
        modules/audio.manager.ixx
        modules/audio.mixer.ixx
        modules/audio.device.ixx
        modules/audio.playback_runtime.ixx
)
if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()
option(EPOCH_AUDIO_BUILD_EXAMPLES "Build the standalone offline WAV demonstration" ${PROJECT_IS_TOP_LEVEL})
if(EPOCH_AUDIO_BUILD_EXAMPLES)
    add_executable(epoch_audio_demo examples/audio.render_wav_demo.cpp)
    target_link_libraries(epoch_audio_demo PRIVATE Epoch::AudioEngine)
    if(BUILD_TESTING)
        add_test(NAME epoch_audio_demo_contract COMMAND epoch_audio_demo --self-test)
    endif()
endif()
if(BUILD_TESTING)
    add_executable(epoch_audio_mixer_contract tests/audio.mixer_consumer.cpp)
    target_link_libraries(epoch_audio_mixer_contract PRIVATE Epoch::AudioEngine)
    add_test(NAME epoch_audio_mixer_contract COMMAND epoch_audio_mixer_contract)
    add_executable(epoch_audio_device_contract tests/audio.device_consumer.cpp)
    target_link_libraries(epoch_audio_device_contract PRIVATE Epoch::AudioEngine)
    add_test(NAME epoch_audio_device_contract COMMAND epoch_audio_device_contract)
    add_executable(epoch_audio_playback_runtime_contract tests/audio.playback_runtime_consumer.cpp)
    target_link_libraries(epoch_audio_playback_runtime_contract PRIVATE Epoch::AudioEngine)
    add_test(NAME epoch_audio_playback_runtime_contract COMMAND epoch_audio_playback_runtime_contract)
endif()
