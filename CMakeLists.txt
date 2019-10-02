cmake_minimum_required(VERSION 3.0)
project(vfx)
add_subdirectory(../allolib al) # !!! locate where allolib is

# list of apps, give `name` if example file is `name.cpp`
set(apps
    ssao
    fxaa # TODO
)

# loop through targets
foreach(app ${apps})
    add_executable(${app} "${app}.cpp")
    set_target_properties(${app} PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY bin
        RUNTIME_OUTPUT_DIRECTORY_DEBUG bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE bin
    )
    target_link_libraries(${app} PRIVATE al)
endforeach()
