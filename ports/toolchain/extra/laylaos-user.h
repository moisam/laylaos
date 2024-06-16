
/*
 * link against C standard library
 * we have no pthreads library, thread support is in LibC
 */
#undef LIB_SPEC
#define LIB_SPEC "%{pthread:} -lc"

#undef STARTFILE_SPEC
//#define STARTFILE_SPEC "crti.o%s %{!shared: crtbegin.o%s} %{shared: crtbeginS.o%s}"
#define STARTFILE_SPEC "%{!shared: %{!pg:crt0.o%s}} crti.o%s %{!shared: crtbegin.o%s} %{shared: crtbeginS.o%s}"

#undef ENDFILE_SPEC
#define ENDFILE_SPEC "%{!shared: crtend.o%s} %{shared: crtendS.o%s} crtn.o%s"
//#define ENDFILE_SPEC "%{!shared: crtend.o%s} crtn.o%s"

/* Dynamic linking */
#define DYNAMIC_LINKER "/usr/lib/ld.so"

#undef LINK_SPEC
#define LINK_SPEC  \
    "%{shared:-shared}  \
     %{static:-static}  \
     %{!shared:     \
        %{rdynamic:-export-dynamic}     \
        %{!static: -dynamic-linker /usr/lib/ld.so}  \
      }     \
      %{symbolic:-Bsymbolic}"

//#undef LINK_EH_SPEC
//#define LINK_EH_SPEC ""

// ELF object format
#undef  OBJECT_FORMAT_ELF
#define OBJECT_FORMAT_ELF

