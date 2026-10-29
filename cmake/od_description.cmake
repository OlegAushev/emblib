# emb_od_description(<target>
#                    IMAGE <file.bin>
#                    [STRINGS <file>...]
#                    [EXAMPLE <candump> EXPECTED <file> NODE <id>])
#
# Extracts, after each link of the executable <target>, the description of
# its object dictionary from the section .od_description of its ELF into
# <target>.od.jsonl in the current binary directory, and checks the
# description and the image <file.bin>:
#
# - the description of the link before is removed first, so that an ELF
#   without the section fails the check instead of leaving it: objcopy then
#   says so, but exits with 0 and writes no file. objcopy writes the ELF to
#   /dev/null rather than rewrite it;
# - tools/od_decode.py --check fails the build if the description breaks its
#   format, section 3.9 of docs/canopen-od-algorithm.ru.md, or its header
#   states another fingerprint than the hash of its body;
# - tools/image_strings.py fails the build if <file.bin> holds, as a whole
#   string, a path or a title of the description or a line of a file
#   STRINGS: labels exist at compile time only;
# - with EXAMPLE, tools/od_decode.py --candump decodes the frames of the log
#   of candump <candump> by the description, as a host reads them from the
#   node <id>, and fails the build if they decode otherwise than <file>
#   EXPECTED says: an example of the frames of the PDOs, which a change to
#   the description or to the tool must keep.
#
# The commands follow the POST_BUILD commands that <target> has so far, and
# CMake runs them in that order: call the function after the command that
# makes <file.bin>. A relative <file.bin> is taken from the current binary
# directory, a relative file STRINGS, <candump> or EXPECTED from the current
# source directory. The function looks for Python 3, which a project that
# calls it needs.
#
# The section exists only if the linker script keeps it in the ELF without
# loading it:
#
#   .od_description 0 (INFO) : { KEEP(*(.od_description)) }
#
# POST_BUILD commands run only when <target> links, and a change to a tool,
# to a file STRINGS, to the example or to what it expects does not link it:
# the build would have no work to do and skip the checks. The function adds
# them to the LINK_DEPENDS of <target> for that, which only the Ninja and
# Makefile generators honour.
function(emb_od_description target)
    cmake_parse_arguments(PARSE_ARGV 1 arg
        "" "IMAGE;EXAMPLE;EXPECTED;NODE" "STRINGS")
    set(example_args ${arg_EXAMPLE} ${arg_EXPECTED} ${arg_NODE})
    list(LENGTH example_args example_count)
    if(arg_UNPARSED_ARGUMENTS OR arg_KEYWORDS_MISSING_VALUES OR NOT arg_IMAGE
       OR NOT (example_count EQUAL 0 OR example_count EQUAL 3))
        message(FATAL_ERROR
            "usage: emb_od_description(<target> IMAGE <file.bin> "
            "[STRINGS <file>...] "
            "[EXAMPLE <candump> EXPECTED <file> NODE <id>])")
    endif()
    if(NOT CMAKE_OBJCOPY)
        message(FATAL_ERROR "emb_od_description: CMAKE_OBJCOPY is not set")
    endif()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    cmake_path(GET CMAKE_CURRENT_FUNCTION_LIST_DIR PARENT_PATH emblib)
    set(od_decode ${emblib}/tools/od_decode.py)
    set(image_strings ${emblib}/tools/image_strings.py)

    set(description ${CMAKE_CURRENT_BINARY_DIR}/${target}.od.jsonl)
    cmake_path(ABSOLUTE_PATH arg_IMAGE
        BASE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} NORMALIZE)
    set(strings)
    set(strings_options)
    foreach(file IN LISTS arg_STRINGS)
        cmake_path(ABSOLUTE_PATH file
            BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} NORMALIZE)
        list(APPEND strings ${file})
        list(APPEND strings_options --strings ${file})
    endforeach()

    set(example)
    set(example_command)
    if(arg_EXAMPLE)
        cmake_path(ABSOLUTE_PATH arg_EXAMPLE
            BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} NORMALIZE)
        cmake_path(ABSOLUTE_PATH arg_EXPECTED
            BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} NORMALIZE)
        set(example ${arg_EXAMPLE} ${arg_EXPECTED})
        set(example_command
            COMMAND ${Python3_EXECUTABLE} ${od_decode} --check ${description}
                --candump ${arg_EXAMPLE} --node ${arg_NODE}
                --expect ${arg_EXPECTED})
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f ${description}
        COMMAND ${CMAKE_OBJCOPY}
            --dump-section .od_description=${description}
            $<TARGET_FILE:${target}> /dev/null
        COMMAND ${Python3_EXECUTABLE} ${od_decode} --check ${description}
        COMMAND ${Python3_EXECUTABLE} ${image_strings} ${arg_IMAGE}
            --description ${description} ${strings_options}
        ${example_command}
        BYPRODUCTS ${description}
        VERBATIM)

    set_property(TARGET ${target} APPEND PROPERTY
        LINK_DEPENDS ${od_decode} ${image_strings} ${strings} ${example})
endfunction()
