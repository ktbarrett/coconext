include_guard(GLOBAL)

option(
    COCONEXT_DEVELOPER_MODE
    "Enable strict warnings, debug information, and coverage for coconext targets"
    OFF
)

function(coconext_enable_developer_mode target)
    if(NOT COCONEXT_DEVELOPER_MODE)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /Od /W4 /WX /Zi)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(
            ${target}
            PRIVATE --coverage -g -Og -Wall -Wextra -Wpedantic -Werror
        )
        target_link_options(${target} PRIVATE --coverage)
    else()
        message(WARNING
            "COCONEXT_DEVELOPER_MODE has no flags for ${CMAKE_CXX_COMPILER_ID}"
        )
    endif()
endfunction()
