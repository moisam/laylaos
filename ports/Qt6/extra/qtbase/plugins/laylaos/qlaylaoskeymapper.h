// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QLAYLAOSKEYMAPPER_H
#define QLAYLAOSKEYMAPPER_H

#include <qnamespace.h>

QT_BEGIN_NAMESPACE

class QLaylaOSKeyMapper
{
public:
    QLaylaOSKeyMapper();

    static int translateKeyCode(char key, bool numlockActive);
};

QT_END_NAMESPACE

#endif
