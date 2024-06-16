#!/bin/sh

HEADER_DIR=${CROSSCOMPILE_SYSROOT_PATH}/usr/include

rm_or_die()
{
    rm $1
    
    [ $? -eq 0 ] || exit_failure "$0: failed to remove $1"
}

rm_or_die ${HEADER_DIR}/time.h
rm_or_die ${HEADER_DIR}/errno.h
rm_or_die ${HEADER_DIR}/unistd.h
rm_or_die ${HEADER_DIR}/stdio.h
rm_or_die ${HEADER_DIR}/stdlib.h
rm_or_die ${HEADER_DIR}/string.h
rm_or_die ${HEADER_DIR}/sys/types.h

