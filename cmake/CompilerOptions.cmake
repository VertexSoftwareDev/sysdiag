# Shared warning / hardening flags applied to every first-party target.

function(sysdiag_apply_compiler_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /utf-8 /sdl /Zc:__cplusplus
            # Do not report warnings from SDK / third-party headers.
            /external:anglebrackets /external:W0
            # Extra useful warnings that are off by default.
            /w14242 /w14254 /w14263 /w14287 /w14296 /w14311 /w14826 /w14905 /w14906 /w14928)
        if(SYSDIAG_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wconversion -Wsign-conversion -Wshadow
            -Wnon-virtual-dtor -Wcast-align -Woverloaded-virtual
            -Wnull-dereference -Wdouble-promotion -Wformat=2
            -Wimplicit-fallthrough)
        if(SYSDIAG_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(SYSDIAG_ENABLE_SANITIZERS AND NOT MINGW)
            target_compile_options(${target} PUBLIC
                -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
            target_link_options(${target} PUBLIC -fsanitize=address,undefined)
        endif()
    endif()
endfunction()
