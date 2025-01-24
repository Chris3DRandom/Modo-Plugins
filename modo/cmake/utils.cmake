# Build a plugin and bundle it into a kit
function(add_modo_plugin name)
    set(SOURCES)
    set(CONFIGS)
    foreach(input ${ARGN})
        string(FIND "${input}" ".cfg" pos)
        if(${pos} GREATER -1)
            list(APPEND CONFIGS ${input})
        else()
            list(APPEND SOURCES ${input})
        endif()
    endforeach()

    add_library(${name} MODULE ${SOURCES})

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

    # Make this a custom build target so it isn't only fired during configure
    set(KIT_NAME ${name})
    configure_file("${CMAKE_SOURCE_DIR}/cmake/templates/index.cfg.in" "${CMAKE_INSTALL_PREFIX}/${name}/index.cfg")
    unset(KIT_NAME)
    foreach(cfg ${CONFIGS})
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${cfg}" "${CMAKE_INSTALL_PREFIX}/${name}/${cfg}" COPYONLY)
    endforeach()

    install(TARGETS ${name} COMPONENT Runtime DESTINATION "${CMAKE_INSTALL_PREFIX}/${name}/win64")
endfunction(add_modo_plugin)

# Kick out a config with tool attrs for testing.
# Attrs are whatever follows formal parameters
function(add_tool_config TOOL KIT)
    set(TOOL_NAME ${TOOL})

    set(ATTR_LIST "\n")
    foreach(attr ${ARGN})
        string(APPEND ATTR_LIST "<list type=\"Control\" val=\"cmd tool.attr ${TOOL} ${attr} ?\"/>\n")
    endforeach()
    configure_file("${CMAKE_SOURCE_DIR}/cmake/templates/tool.cfg.in" "${CMAKE_INSTALL_PREFIX}/${KIT}/toolprops.cfg")
    
    unset(ATTR_LIST)
    unset(TOOL_NAME)
endfunction(add_tool_config)

function(print_note)
    set(OUTPUT_STRING "NOTE:\n")
    foreach(input ${ARGN})
        string(APPEND OUTPUT_STRING "\t${input}\n")
    endforeach()
    string(APPEND OUTPUT_STRING "END NOTE\n")
    message(NOTICE "${OUTPUT_STRING}")
endfunction(print_note)