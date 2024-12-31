# SDK is just a static lib we link to
add_library(lxsdk STATIC
    "clean.cpp"
    "cwrap.cpp"
    "initialize.cpp"
    "lxu_attrdesc.cpp"
    "lxu_attributes.cpp"
    "lxu_chanmod.cpp"
    "lxu_command.cpp"
    "lxu_deform.cpp"
    "lxu_dialog.cpp"
    "lxu_drawover.cpp"
    "lxu_drawover.cpp"
    "lxu_drop.cpp"
    "lxu_format.cpp"
    "lxu_io.cpp"
    "lxu_log.cpp"
    "lxu_math.cpp"
    "lxu_matrix.cpp"
    "lxu_message.cpp"
    "lxu_meta.cpp"
    "lxu_modifier.cpp"
    "lxu_package.cpp"
    "lxu_parser.cpp"
    "lxu_prefvalue.cpp"
    "lxu_quaternion.cpp"
    "lxu_queries.cpp"
    "lxu_scene.cpp"
    "lxu_schematic.cpp"
    "lxu_select.cpp"
    "lxu_shade.cpp"
    "lxu_simd.cpp"
    "lxu_surface.cpp"
    "lxu_tableau.cpp"
    "lxu_value.cpp"
    "lxu_vector.cpp"
    "lxu_vectorarray.cpp"
    "util.cpp"
)

target_include_directories(lxsdk PUBLIC ${INCLUDE_DIR})

set_target_properties(lxsdk
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

target_compile_features(lxsdk
    PRIVATE
        cxx_std_17
)
