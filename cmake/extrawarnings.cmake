# Enable some useful warnings
ADD_DEFINITIONS(
   -Wall
   -Wextra
   -Wmissing-declarations
   -Wmissing-noreturn
   -Wpointer-arith
   -Wcast-align
   -Wwrite-strings
   -Wformat-nonliteral
   -Wformat-security
   -Wswitch-enum
   -Winit-self
   -Wmissing-include-dirs
   -Wundef
   -Wmissing-format-attribute
   -Wno-reorder
   -Wunused
   -Wuninitialized
   -Woverloaded-virtual
   -Wunused-value
   -pedantic
   -Wnonnull
   -Wsequence-point
   #-Wsystem-headers
   -Wsizeof-pointer-memaccess
   #-Wuseless-cast
   -Wvarargs

   #See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55578
   -Wno-unused-function
   -Wno-attributes
   -Wno-missing-include-dirs
   -Wno-unknown-pragmas
)

#Add more warnings for compilers that support it. I used this command:
#curl https://gcc.gnu.org/onlinedocs/gcc-4.9.2/gcc/Warning-Options.html | \
#grep -E "^[\t ]+<br><dt><code>-W[a-zA-Z=-]*" -o | grep -E "\-W[a-zA-Z=-]*" -o >
#cat /tmp/48 /tmp/49 | sort | uniq -u
# IF (CMAKE_COMPILER_IS_GNUCC)

IF (CMAKE_COMPILER_IS_GNUCC)
   IF (GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9)
      ADD_DEFINITIONS(
         -Wunused-but-set-parameter
         -Wconditionally-supported
         #-Wsuggest-attribute=const
         -Wno-cpp
         -Wdouble-promotion
         -Wdate-time
         -Wdelete-incomplete
         -Wfloat-conversion
      )
   ENDIF()

   if (GCC_VERSION VERSION_GREATER 5.1 OR GCC_VERSION VERSION_EQUAL 5.1)
      ADD_DEFINITIONS(
         -Wsuggest-override
         #-Wsuggest-final-types
         #-Wsuggest-final-methods
         -Wbool-compare
         -Wformat-signedness
         -Wlogical-not-parentheses
         -Wnormalized
         -Wshift-count-negative
         -Wshift-count-overflow
         -Wsized-deallocation
         -Wsizeof-array-argument
      )
   ENDIF()

   IF (GCC_VERSION VERSION_GREATER 6.0 OR GCC_VERSION VERSION_EQUAL 6.0)
      ADD_DEFINITIONS(
         -Wnull-dereference
         -Wshift-negative-value
         -Wshift-overflow
         -Wduplicated-cond
         -Wmisleading-indentation
         -Wnon-virtual-dtor
      )
   ENDIF()
ELSE()
    ADD_DEFINITIONS(-Wno-unknown-pragmas -Wno-unknown-warning-option)
ENDIF()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   ADD_DEFINITIONS(
      -Wno-c++98-compat
      -Wno-c++98-compat-pedantic
      -Wno-unknown-pragmas
      -Wno-documentation-unknown-command
      -Wno-padded
      -Wno-old-style-cast
      -Wno-sign-conversion
      -Wno-exit-time-destructors
      -Wno-global-constructors
      -Wno-shorten-64-to-32
      -Wno-weak-vtables
      -Wno-float-conversion
      -Wno-reserved-id-macro
      #-Weverything
   )
endif()
