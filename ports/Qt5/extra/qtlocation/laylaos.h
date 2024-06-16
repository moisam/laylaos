/*
Copyright Jessica Hamilton 2014
Copyright Rene Rivera 2014-2015
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef BOOST_PREDEF_OS_LAYLAOS_H
#define BOOST_PREDEF_OS_LAYLAOS_H

#include <boost/predef/version_number.h>
#include <boost/predef/make.h>

/*`
[heading `BOOST_OS_LAYLAOS`]

[LaylaOS] operating system.

[table
    [[__predef_symbol__] [__predef_version__]]

    [[`__HAIKU__`] [__predef_detection__]]
    ]
 */

#define BOOST_OS_LAYLAOS BOOST_VERSION_NUMBER_NOT_AVAILABLE

#if !defined(BOOST_PREDEF_DETAIL_OS_DETECTED) && ( \
    defined(__laylaos__) \
    )
#   undef BOOST_OS_LAYLAOS
#   define BOOST_OS_LAYLAOS BOOST_VERSION_NUMBER_AVAILABLE
#endif

#if BOOST_OS_LAYLAOS
#   define BOOST_OS_LAYLAOS_AVAILABLE
#   include <boost/predef/detail/os_detected.h>
#endif

#define BOOST_OS_LAYLAOS_NAME "LaylaOS"

#endif

#include <boost/predef/detail/test.h>
BOOST_PREDEF_DECLARE_TEST(BOOST_OS_LAYLAOS,BOOST_OS_LAYLAOS_NAME)
