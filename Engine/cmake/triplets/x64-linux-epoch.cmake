set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Raylib and SFML both embed STB. Keep Epoch's dependency graph static except
# for SFML so both backends can coexist without duplicate global STB symbols.
if(PORT STREQUAL "sfml")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
else()
    set(VCPKG_LIBRARY_LINKAGE static)
endif()
