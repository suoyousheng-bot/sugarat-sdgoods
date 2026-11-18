function(beat_generate_asset output python asset)
    set(generator "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/beat_${asset}.py")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${python}" "${generator}" "${output}"
        DEPENDS "${generator}"
        VERBATIM)
endfunction()
