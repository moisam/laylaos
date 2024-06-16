
#undef LIB_SPEC
#define LIB_SPEC "-lc"

#undef STARTFILE_SPEC
#define STARTFILE_SPEC "crti.o%s %{!shared: crtbegin.o%s} %{shared: crtbeginS.o%s}"

#undef ENDFILE_SPEC
#define ENDFILE_SPEC "%{!shared: crtend.o%s} %{shared: crtendS.o%s} crtn.o%s"

#undef LINK_SPEC
#define LINK_SPEC  \
    "%{shared:-shared}  \
     %{static:-static}  \
     %{!shared:     \
        %{rdynamic:-export-dynamic}     \
        %{!static: -dynamic-linker /usr/lib/ld.so}  \
      }     \
      %{symbolic:-Bsymbolic}"

// ELF object format
#undef  OBJECT_FORMAT_ELF
#define OBJECT_FORMAT_ELF

