function(basic_demo_collect_skill_components out_var)
    idf_build_get_property(build_components BUILD_COMPONENTS)

    get_filename_component(project_root_dir "${CMAKE_SOURCE_DIR}" REALPATH)
    get_filename_component(shared_components_root "${CMAKE_SOURCE_DIR}/../../components" REALPATH)
    get_filename_component(managed_components_root "${CMAKE_SOURCE_DIR}/managed_components" REALPATH)

    set(component_args)
    foreach(component_name IN LISTS build_components)
        idf_component_get_property(component_dir "${component_name}" COMPONENT_DIR)
        if(NOT component_dir)
            continue()
        endif()

        get_filename_component(component_dir "${component_dir}" REALPATH)

        if(component_dir MATCHES "^${managed_components_root}(/|$)")
            continue()
        endif()

        if(component_dir MATCHES "^${project_root_dir}(/|$)" OR
           component_dir MATCHES "^${shared_components_root}(/|$)")
            list(APPEND component_args --component "${component_name}=${component_dir}")
        endif()
    endforeach()

    set(${out_var} "${component_args}" PARENT_SCOPE)
endfunction()

function(basic_demo_collect_skill_files out_var)
    idf_build_get_property(build_components BUILD_COMPONENTS)

    get_filename_component(project_root_dir "${CMAKE_SOURCE_DIR}" REALPATH)
    get_filename_component(shared_components_root "${CMAKE_SOURCE_DIR}/../../components" REALPATH)
    get_filename_component(managed_components_root "${CMAKE_SOURCE_DIR}/managed_components" REALPATH)

    set(skill_files)
    foreach(component_name IN LISTS build_components)
        idf_component_get_property(component_dir "${component_name}" COMPONENT_DIR)
        if(NOT component_dir)
            continue()
        endif()

        get_filename_component(component_dir "${component_dir}" REALPATH)

        if(component_dir MATCHES "^${managed_components_root}(/|$)")
            continue()
        endif()

        if(NOT (component_dir MATCHES "^${project_root_dir}(/|$)" OR
                component_dir MATCHES "^${shared_components_root}(/|$)"))
            continue()
        endif()

        if(NOT EXISTS "${component_dir}/skills")
            continue()
        endif()

        file(GLOB component_skill_files CONFIGURE_DEPENDS LIST_DIRECTORIES false "${component_dir}/skills/*")
        list(APPEND skill_files ${component_skill_files})
    endforeach()

    list(REMOVE_DUPLICATES skill_files)
    set(${out_var} "${skill_files}" PARENT_SCOPE)
endfunction()


function(basic_demo_enable_component_skills_sync)
    set(options)
    set(one_value_args TARGET DEMO_SKILLS_DIR MANIFEST_PATH)
    cmake_parse_arguments(arg "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT arg_TARGET)
        message(FATAL_ERROR "basic_demo_enable_component_skills_sync requires TARGET")
    endif()
    if(NOT arg_DEMO_SKILLS_DIR)
        message(FATAL_ERROR "basic_demo_enable_component_skills_sync requires DEMO_SKILLS_DIR")
    endif()
    if(NOT arg_MANIFEST_PATH)
        message(FATAL_ERROR "basic_demo_enable_component_skills_sync requires MANIFEST_PATH")
    endif()

    idf_build_get_property(python PYTHON)
    basic_demo_collect_skill_components(skill_component_args)
    basic_demo_collect_skill_files(skill_dependency_files)
    set(skill_sync_extra_args)
    set(skill_sync_stamp "${CMAKE_BINARY_DIR}/component_skills_sync.stamp")

    if(CONFIG_BASIC_DEMO_MEMORY_MODE_LIGHTWEIGHT)
        list(APPEND skill_sync_extra_args --exclude-skill-id memory_ops)
    endif()

    add_custom_command(
        OUTPUT "${skill_sync_stamp}"
        COMMAND ${python}
                "${CMAKE_SOURCE_DIR}/tools/sync_component_skills.py"
                --demo-skills-dir "${arg_DEMO_SKILLS_DIR}"
                --manifest-path "${arg_MANIFEST_PATH}"
                ${skill_sync_extra_args}
                ${skill_component_args}
        COMMAND ${CMAKE_COMMAND} -E touch "${skill_sync_stamp}"
        DEPENDS
                "${CMAKE_SOURCE_DIR}/tools/sync_component_skills.py"
                ${skill_dependency_files}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Sync component skills into fatfs_image/skills"
        VERBATIM
    )

    add_custom_target(basic_demo_sync_skills ALL DEPENDS "${skill_sync_stamp}")

    get_property(skill_generator_targets GLOBAL PROPERTY BASIC_DEMO_SKILL_GENERATOR_TARGETS)
    if(skill_generator_targets)
        add_dependencies(basic_demo_sync_skills ${skill_generator_targets})
    endif()

    add_dependencies(${arg_TARGET} basic_demo_sync_skills)
endfunction()
