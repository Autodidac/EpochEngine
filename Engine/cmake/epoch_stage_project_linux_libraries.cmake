# Copy only the Linux runtime libraries staged by the Engine target.
file(GLOB runtime_libraries LIST_DIRECTORIES FALSE
    "${EPOCH_RUNTIME_SOURCE}/libsfml-graphics.so*"
    "${EPOCH_RUNTIME_SOURCE}/libsfml-window.so*"
    "${EPOCH_RUNTIME_SOURCE}/libsfml-system.so*"
    "${EPOCH_RUNTIME_SOURCE}/libvulkan.so*")
if(runtime_libraries)
    file(MAKE_DIRECTORY "${EPOCH_RUNTIME_DESTINATION}")
    foreach(library IN LISTS runtime_libraries)
        get_filename_component(name "${library}" NAME)
        file(COPY_FILE "${library}" "${EPOCH_RUNTIME_DESTINATION}/${name}" ONLY_IF_DIFFERENT)
    endforeach()
endif()
