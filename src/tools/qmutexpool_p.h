/****************************************************************************
**
** ...
**
** Copyright (C) 2005-2008 Trolltech ASA.  All rights reserved.
**
** This file is part of the tools module of the Qt GUI Toolkit.
**
** This file may be used under the terms of the GNU General
** Public License versions 2.0 or 3.0 as published by the Free
** Software Foundation and appearing in the files LICENSE.GPL2
** and LICENSE.GPL3 included in the packaging of this file.
** Alternatively you may (at your option) use any later version
** of the GNU General Public License if such license has been
** publicly approved by Trolltech ASA (or its successors, if any)
** and the KDE Free Qt Foundation.
**
** Please review the following information to ensure GNU General
** Public Licensing requirements will be met:
** http://trolltech.com/products/qt/licenses/licensing/opensource/.
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://trolltech.com/products/qt/licenses/licensing/licensingoverview
** or contact the sales department at sales@trolltech.com.
**
** This file may be used under the terms of the Q Public License as
** defined by Trolltech ASA and appearing in the file LICENSE.QPL
** included in the packaging of this file.  Licensees holding valid Qt
** Commercial licenses may use this file in accordance with the Qt
** Commercial License Agreement provided with the Software.
**
** This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
** INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE. Trolltech reserves all rights not granted
** herein.
**
**********************************************************************/

#ifndef QMUTEXPOOL_P_H
#define QMUTEXPOOL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of QSettings. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//
//

#ifdef QT_THREAD_SUPPORT

#ifndef QT_H
#include "qmutex.h"
#include "qmemarray.h"
#endif // QT_H

class Q_EXPORT QMutexPool
{
public:
    QMutexPool( bool recursive = FALSE, int size = 17 );
    ~QMutexPool();

    QMutex *get( void *address );

private:
    QMutex mutex;
    QMutex **mutexes;
    int count;
    bool recurs;
};

extern Q_EXPORT QMutexPool *qt_global_mutexpool;

#endif // QT_THREAD_SUPPORT

#endif // QMUTEXPOOL_P_H
