set(BASIC_DEMO_PROJECT_LOG_PREFIX "[basic_demo]")
set(BASIC_DEMO_BOARD_MANAGER_PATCH_SCRIPT "${CMAKE_SOURCE_DIR}/tools/bmgr_patch.py")

if(NOT EXISTS "${BASIC_DEMO_BOARD_MANAGER_PATCH_SCRIPT}")
    message(FATAL_ERROR "${BASIC_DEMO_PROJECT_LOG_PREFIX} Board manager patch script not found: ${BASIC_DEMO_BOARD_MANAGER_PATCH_SCRIPT}")
endif()

idf_build_get_property(python PYTHON)

add_custom_target(basic_demo_patch_managed_components ALL
    COMMAND ${python} "${BASIC_DEMO_BOARD_MANAGER_PATCH_SCRIPT}" --project-dir "${CMAKE_SOURCE_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "${BASIC_DEMO_PROJECT_LOG_PREFIX} Patching managed component sources"
    VERBATIM
)
add_dependencies(app basic_demo_patch_managed_components)
