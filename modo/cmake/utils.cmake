# Build a plugin and bundle it into a kit
function(add_modo_plugin name)
    add_library(${name} MODULE ${ARGN})

    set_target_properties(${name}
        PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            SUFFIX ".lx"
    )

    target_include_directories(${name} PRIVATE ${INCLUDE_DIR})

    target_compile_features(${name}
        PRIVATE
            cxx_std_17
    )

    target_link_libraries(${name}
        PUBLIC
            lxsdk
    )

    set(KIT_NAME ${name})
    configure_file("${CMAKE_SOURCE_DIR}/cmake/index.cfg.in" "${CMAKE_INSTALL_PREFIX}/${name}/index.cfg")
    unset(KIT_NAME)

    install(TARGETS ${name} COMPONENT Runtime DESTINATION "${CMAKE_INSTALL_PREFIX}/${name}/win64")
endfunction(add_modo_plugin)

