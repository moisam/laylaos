#!/bin/sh

HEADER_DIR=${CROSSCOMPILE_SYSROOT_PATH}/usr/include
[ -e ${HEADER_DIR} ] && rm ${HEADER_DIR}
mkdir -p ${HEADER_DIR}/sys
ln -s '.' ${CROSSCOMPILE_SYSROOT_PATH}/usr/local
#ln -s ${HEADER_DIR} ${CROSSCOMPILE_SYSROOT_PATH}/usr/local/include

touch ${HEADER_DIR}/time.h
touch ${HEADER_DIR}/errno.h
#touch ${HEADER_DIR}/sys/mman.h

cat << EOF > ${HEADER_DIR}/unistd.h
#ifndef _UNISTD_H
#define _UNISTD_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
int execv(const char*, char* const[]);
int execve(const char*, char* const[], char* const[]);
int execvp(const char*, char* const[]);
pid_t fork(void);
#ifdef __cplusplus
}
#endif
#endif
EOF

cat << EOF2 > ${HEADER_DIR}/stdio.h
#ifndef _STDIO_H
#define _STDIO_H
#include <stdarg.h>
#include <stddef.h>
#define SEEK_SET 0
typedef struct { int unused; } FILE;
#ifdef __cplusplus
extern "C" {
#endif
extern FILE* stderr;
#define stderr stderr
int fclose(FILE*);
int fflush(FILE*);
FILE* fopen(const char*, const char*);
int fprintf(FILE*, const char*, ...);
size_t fread(void*, size_t, size_t, FILE*);
int fseek(FILE*, long, int);
long ftell(FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);
void setbuf(FILE*, char*);
int vfprintf(FILE*, const char*, va_list);
#ifdef __cplusplus
}
#endif
#endif
EOF2

cat << EOF3 > ${HEADER_DIR}/sys/types.h
#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H
typedef int pid_t;
typedef long int intptr_t;
#endif
EOF3

cat << EOF4 > ${HEADER_DIR}/string.h
#ifndef _STRING_H
#define _STRING_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void* memcpy(void*, const void*, size_t);
void* memset(void*, int, size_t);
char* strcpy(char*, const char*);
size_t strlen(const char*);
#ifdef __cplusplus
}
#endif
#endif
EOF4

cat << EOF5 > ${HEADER_DIR}/stdlib.h
#ifndef _STDLIB_H
#define _STDLIB_H
#ifdef __cplusplus
extern "C" {
#endif
void abort(void);
int atexit(void (*)(void));
int atoi(const char*);
void free(void*);
char* getenv(const char*);
void* malloc(size_t);
#ifdef __cplusplus
}
#endif
#endif
EOF5

