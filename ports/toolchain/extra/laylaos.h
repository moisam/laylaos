/* Useful if you wish to make target-specific GCC changes. */
#undef TARGET_LAYLAOS
#define TARGET_LAYLAOS 1
 
/* Additional predefined macros. */
#undef TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()      \
  do {                                \
    builtin_define ("__laylaos__");      \
    builtin_define ("__unix__");      \
    builtin_assert ("system=laylaos");   \
    builtin_assert ("system=unix");   \
    builtin_assert ("system=posix");   \
  } while(0);


