/**
** Qt->glib main event loop integration by Norbert Frese 2005
** code based on qeventloop_p.h 3.3.5
**
*/

/****************************************************************************
** $Id: qt/qeventloop_glib_p.h
**
** Definition of QEventLoop class
**
** Copyright (C) 1992-2005 Trolltech AS.  All rights reserved.
**
** This file is part of the kernel module of the Qt GUI Toolkit.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** Licensees holding valid Qt Enterprise Edition or Qt Professional Edition
** licenses for Qt/Embedded may use this file in accordance with the
** Qt Embedded Commercial License Agreement provided with the Software.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.trolltech.com/pricing.html or email sales@trolltech.com for
**   information about Qt Commercial License Agreements.
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

#ifndef QEVENTLOOP_GLIB_P_H
#define QEVENTLOOP_GLIB_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  This header file may
// change from version to version without notice, or even be
// removed.
//
// We mean it.
//
//

#ifndef QT_H
#include "qplatformdefs.h"
#endif // QT_H

// SCO OpenServer redefines raise -> kill
#if defined(raise)
# undef raise
#endif

#include "qeventloop.h"
#include "qwindowdefs.h"

class QSocketNotifier;

#include "qptrlist.h"

#include <glib.h>

// uncomment this for main loop related debug-output

// #define DEBUG_QT_GLIBMAINLOOP 1

// Wrapper for QSocketNotifier Object and GPollFD

struct QSockNotGPollFD
{
    QSocketNotifier *obj;
    GPollFD gPollFD; 
	gushort events;  // save events
	bool pending;
};

class QEventLoopPrivate
{
public:
    QEventLoopPrivate()
    {
	reset();
    }

    void reset() {
        looplevel = 0;
        quitcode = 0;
        quitnow = FALSE;
        exitloop = FALSE;
        shortcut = FALSE;
    }

    int looplevel;
    int quitcode;
    unsigned int quitnow  : 1;
    unsigned int exitloop : 1;
    unsigned int shortcut : 1;

#if defined(Q_WS_X11)
    int xfd;
	
	GPollFD x_gPollFD; 
	
#endif // Q_WS_X11

    int thread_pipe[2];

	GPollFD threadPipe_gPollFD;
	
    QPtrList<QSockNotGPollFD> sn_list;
    // pending socket notifiers list
    QPtrList<QSockNotGPollFD> sn_pending_list;
	
	// store flags for one iteration
	uint pev_flags;  	
	
	// My GSource
	
	GSource * gSource;

};

#endif // QEVENTLOOP_GLIB_P_H
