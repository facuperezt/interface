# =============================================================================
# interface :: Compiler Flags
# =============================================================================
# Sets up compiler warnings, sanitizers, and C++23 feature requirements.
# Applied to all targets via a common interface library.
# =============================================================================

add_library(interface_compiler_flags INTERFACE)
add_library(interface::compiler_flags ALIAS interface_compiler_flags)

# ---------------------------------------------------------------------------
# C++23 requirement
# ---------------------------------------------------------------------------
target_compile_features(interface_compiler_flags INTERFACE cxx_std_23)

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------
set(INTERFACE_GCC_CLANG_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
)

set(INTERFACE_GCC_ONLY_WARNINGS
    -Wmisleading-indentation
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wuseless-cast
)

set(INTERFACE_MSVC_WARNINGS
    /W4
    /w14242  # conversion, possible loss of data
    /w14254  # operator conversion, possible loss of data
    /w14263  # member function does not override any base class virtual member function
    /w14265  # class has virtual functions, but destructor is not virtual
    /w14287  # unsigned/negative constant mismatch
    /we4289  # loop control variable used outside the for-loop scope
    /w14296  # expression is always false
    /w14311  # pointer truncation
    /w14545  # expression before comma evaluates to a function
    /w14546  # function call before comma missing argument list
    /w14547  # operator before comma has no effect
    /w14549  # operator before comma has no effect
    /w14555  # expression has no effect
    /w14619  # pragma warning: there is no warning number
    /w14640  # thread un-safe static member initialization
    /w14826  # conversion is sign-extended
    /w14905  # wide string literal cast to LPSTR
    /w14906  # string literal cast to LPWSTR
    /w14928  # illegal copy-initialization
    /permissive-
)

if(MSVC)
    target_compile_options(interface_compiler_flags INTERFACE ${INTERFACE_MSVC_WARNINGS})
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(interface_compiler_flags INTERFACE
        ${INTERFACE_GCC_CLANG_WARNINGS}
        ${INTERFACE_GCC_ONLY_WARNINGS}
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(interface_compiler_flags INTERFACE
        ${INTERFACE_GCC_CLANG_WARNINGS}
    )
endif()

# ---------------------------------------------------------------------------
# Sanitizers (Debug builds only)
# ---------------------------------------------------------------------------
option(INTERFACE_ENABLE_SANITIZERS "Enable ASan + UBSan in Debug builds" OFF)

if(INTERFACE_ENABLE_SANITIZERS)
    if(NOT MSVC)
        target_compile_options(interface_compiler_flags INTERFACE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
            $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        )
        target_link_options(interface_compiler_flags INTERFACE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# LTO (Release builds)
# ---------------------------------------------------------------------------
option(INTERFACE_ENABLE_LTO "Enable Link-Time Optimization for Release" OFF)

if(INTERFACE_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lto_supported OUTPUT lto_error)
    if(lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        message(STATUS "LTO enabled for Release builds")
    else()
        message(WARNING "LTO requested but not supported: ${lto_error}")
    endif()
endif()
