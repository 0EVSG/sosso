# Default compiler flags.
set(BUILD_SANITIZE_FLAGS "")
set(BUILD_WARNING_FLAGS "-Wall -Wextra -Wpedantic")

# Optional: Extensive compiler warnings for QA.
option(BUILD_EXTENSIVE_WARNINGS
  "Add extensive compiler warnings to the build."
  OFF
)

if (BUILD_EXTENSIVE_WARNINGS)
  set(BUILD_WARNING_FLAGS
    "${BUILD_WARNING_FLAGS} -Weverything \
                            -Wno-c++98-compat \
                            -Wno-c++98-compat-pedantic \
                            -Wno-c++98-c++11-compat"
  )
endif (BUILD_EXTENSIVE_WARNINGS)

# Optional: Clang Tidy for QA, hard-coded to clang 16.0 for now.
option(BUILD_CLANG_TIDY
  "Add static checks and lint to build using clang-tidy."
  OFF
)

if (BUILD_CLANG_TIDY)
  find_program(BUILD_CLANG_TIDY_PROGRAM NAMES clang-tidy16 clang-tidy)
  if (BUILD_CLANG_TIDY_PROGRAM)
    set(CMAKE_CXX_CLANG_TIDY ${BUILD_CLANG_TIDY_PROGRAM}
      "-checks=bugprone*,\
               clang-analyzer*,\
               misc*,\
               modernize*,\
               performance*,\
               portability*,\
               readability*,\
               -readability-else-after-return,\
               -readability-implicit-bool-conversion"
    )
  endif (BUILD_CLANG_TIDY_PROGRAM)
endif (BUILD_CLANG_TIDY)

option(BUILD_INCLUDE_WHAT_YOU_USE
  "Add include-what-you-use header include checks to the build."
  OFF
)

if (BUILD_INCLUDE_WHAT_YOU_USE)
  find_program(BUILD_IWYU_PROGRAM NAMES include-what-you-use)
  if (BUILD_IWYU_PROGRAM)
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
      ${BUILD_IWYU_PROGRAM} "-Xiwyu" "--transitive_includes_only"
    )
  endif(BUILD_IWYU_PROGRAM)
endif (BUILD_INCLUDE_WHAT_YOU_USE)

# Combined compiler flags for convenience.
set(BUILD_COMPILER_FLAGS "${BUILD_WARNING_FLAGS} ${BUILD_SANITIZE_FLAGS}")
