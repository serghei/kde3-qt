/**
** Qt->glib main event loop integration by Norbert Frese 2005
** code based on qeventloop_x11.cpp 3.3.5
**
*/

/****************************************************************************
** $Id: qt/qeventloop_x11_glib.cpp
**
** Implementation of QEventLoop class
**
** Copyright (C) 2000-2005 Trolltech AS.  All rights reserved.
**
** This file is part of the kernel module of the Qt GUI Toolkit.
**
** This file may be distributed under the terms of the Q Public License
** as defined by Trolltech AS of Norway and appearing in the file
** LICENSE.QPL included in the packaging of this file.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** Licensees holding valid Qt Enterprise Edition or Qt Professional Edition
** licenses for Unix/X11 may use this file in accordance with the Qt Commercial
** License Agreement provided with the Software.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.trolltech.com/pricing.html or email sales@trolltech.com for
**   information about Qt Commercial License Agreements.
** See http://www.trolltech.com/qpl/ for QPL licensing information.
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/


#include "qeventloop_glib_p.h" // includes qplatformdefs.h
#include "qeventloop.h"
#include "qapplication.h"
#include "qbitarray.h"
#include "qcolor_p.h"
#include "qt_x11_p.h"

#if defined(QT_THREAD_SUPPORT)
#  include "qmutex.h"
#endif // QT_THREAD_SUPPORT

#include <errno.h>

#include <glib.h>

// Qt-GSource Structure and Callbacks

typedef struct {
    GSource source;
    QEventLoop * qeventLoop;
} QtGSource;

static gboolean qt_gsource_prepare  ( GSource *source,
	gint *timeout );
static gboolean qt_gsource_check    ( GSource *source );
static gboolean qt_gsource_dispatch ( GSource *source,
    GSourceFunc  callback, gpointer user_data );

static GSourceFuncs qt_gsource_funcs = {
    qt_gsource_prepare,
    qt_gsource_check,
    qt_gsource_dispatch,
    NULL,
    NULL,
    NULL
};

// forward main loop callbacks to QEventLoop methods!

static gboolean qt_gsource_prepare  ( GSource *source,
	gint *timeout ) 
{
    QtGSource * qtGSource;
	qtGSource = (QtGSource*) source;
    return qtGSource->qeventLoop->gsourcePrepare(source, timeout); 
}

static gboolean qt_gsource_check    ( GSource *source ) 
{
    QtGSource * qtGSource = (QtGSource*) source;
    return qtGSource->qeventLoop->gsourceCheck(source); 
}

static gboolean qt_gsource_dispatch ( GSource *source,
    GSourceFunc  callback, gpointer user_data ) 
{
    QtGSource * qtGSource = (QtGSource*) source;
    return qtGSource->qeventLoop->gsourceDispatch(source); 
}


// -------------------------------------------------

// resolve the conflict between X11's FocusIn and QEvent::FocusIn
#undef FocusOut
#undef FocusIn

static const int XKeyPress = KeyPress;
static const int XKeyRelease = KeyRelease;
#undef KeyPress
#undef KeyRelease

// from qapplication.cpp
extern bool qt_is_gui_used;

// from qeventloop_unix.cpp
extern timeval *qt_wait_timer();
extern void cleanupTimers();

// ### this needs to go away at some point...
typedef void (*VFPTR)();
typedef QValueList<VFPTR> QVFuncList;
void qt_install_preselect_handler( VFPTR );
void qt_remove_preselect_handler( VFPTR );
static QVFuncList *qt_preselect_handler = 0;
void qt_install_postselect_handler( VFPTR );
void qt_remove_postselect_handler( VFPTR );
static QVFuncList *qt_postselect_handler = 0;

void qt_install_preselect_handler( VFPTR handler )
{
    if ( !qt_preselect_handler )
	qt_preselect_handler = new QVFuncList;
    qt_preselect_handler->append( handler );
}
void qt_remove_preselect_handler( VFPTR handler )
{
    if ( qt_preselect_handler ) {
	QVFuncList::Iterator it = qt_preselect_handler->find( handler );
	if ( it != qt_preselect_handler->end() )
		qt_preselect_handler->remove( it );
    }
}
void qt_install_postselect_handler( VFPTR handler )
{
    if ( !qt_postselect_handler )
	qt_postselect_handler = new QVFuncList;
    qt_postselect_handler->prepend( handler );
}
void qt_remove_postselect_handler( VFPTR handler )
{
    if ( qt_postselect_handler ) {
	QVFuncList::Iterator it = qt_postselect_handler->find( handler );
	if ( it != qt_postselect_handler->end() )
		qt_postselect_handler->remove( it );
    }
}


void QEventLoop::init()
{
	// initialize ProcessEventFlags (all events & wait for more)
	
	d->pev_flags = AllEvents | WaitForMore;	
	
    // initialize the common parts of the event loop
    pipe( d->thread_pipe );
    fcntl(d->thread_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(d->thread_pipe[1], F_SETFD, FD_CLOEXEC);

    // intitialize the X11 parts of the event loop
    d->xfd = -1;
    if ( qt_is_gui_used )
        d->xfd = XConnectionNumber( QPaintDevice::x11AppDisplay() );

    // new GSource

    QtGSource * qtGSource = (QtGSource*) g_source_new(&qt_gsource_funcs,
                             sizeof(QtGSource));
                             
    g_source_set_can_recurse ((GSource*)qtGSource, TRUE);

    qtGSource->qeventLoop = this;
	
    // init main loop and attach gsource
	
	#ifdef DEBUG_QT_GLIBMAINLOOP
		printf("inside init(1)\n");
	#endif		
	
    g_main_loop_new (NULL, 1);
	
    g_source_attach( (GSource*)qtGSource, NULL );

	d->gSource = (GSource*) qtGSource;
	
	// poll for X11 events
	
    if ( qt_is_gui_used ) {
	    
		
		d->x_gPollFD.fd = d->xfd;
		d->x_gPollFD.events = G_IO_IN | G_IO_HUP;
		g_source_add_poll(d->gSource, &d->x_gPollFD); 	
    }

	// poll thread-pipe
	
	d->threadPipe_gPollFD.fd = d->thread_pipe[0];
	d->threadPipe_gPollFD.events = G_IO_IN | G_IO_HUP;
	
	g_source_add_poll(d->gSource, &d->threadPipe_gPollFD); 
	
	#ifdef DEBUG_QT_GLIBMAINLOOP 
		printf("inside init(2)\n");
	#endif		
	
}

void QEventLoop::cleanup()
{
    // cleanup the common parts of the event loop
    close( d->thread_pipe[0] );
    close( d->thread_pipe[1] );
    cleanupTimers();

    // cleanup the X11 parts of the event loop
    d->xfd = -1;
	
	// todo: destroy gsource
}

bool QEventLoop::processEvents( ProcessEventsFlags flags )
{

	#ifdef DEBUG_QT_GLIBMAINLOOP
		printf("inside processEvents(1) looplevel=%d\n", d->looplevel );
	#endif	
    ProcessEventsFlags save_flags;
    int rval;
    save_flags = d->pev_flags;
	
    d->pev_flags = flags;

    rval = g_main_context_iteration(NULL, flags & WaitForMore ? TRUE : FALSE);	

    d->pev_flags = save_flags;
	
	#ifdef DEBUG_QT_GLIBMAINLOOP
		printf("inside processEvents(2) looplevel=%d rval=%d\n", d->looplevel, rval );
	#endif	
	
    return rval; // were events processed?
}


bool QEventLoop::processX11Events()
{
	ProcessEventsFlags flags = d->pev_flags;
    // process events from the X server
    XEvent event;
    int	   nevents = 0;

#if defined(QT_THREAD_SUPPORT)
    QMutexLocker locker( QApplication::qt_mutex );
#endif

    // handle gui and posted events
    if ( qt_is_gui_used ) {
	QApplication::sendPostedEvents();

	// Two loops so that posted events accumulate
	while ( XPending( QPaintDevice::x11AppDisplay() ) ) {
	    // also flushes output buffer
	    while ( XPending( QPaintDevice::x11AppDisplay() ) ) {
		if ( d->shortcut ) {
		    return FALSE;
		}

		XNextEvent( QPaintDevice::x11AppDisplay(), &event );

		if ( flags & ExcludeUserInput ) {
		    switch ( event.type ) {
		    case ButtonPress:
		    case ButtonRelease:
		    case MotionNotify:
		    case XKeyPress:
		    case XKeyRelease:
		    case EnterNotify:
		    case LeaveNotify:
			continue;

		    case ClientMessage:
			{
			    // from qapplication_x11.cpp
			    extern Atom qt_wm_protocols;
			    extern Atom qt_wm_take_focus;
			    extern Atom qt_qt_scrolldone;

			    // only keep the wm_take_focus and
			    // qt_qt_scrolldone protocols, discard all
			    // other client messages
			    if ( event.xclient.format != 32 )
				continue;

			    if ( event.xclient.message_type == qt_wm_protocols ||
				 (Atom) event.xclient.data.l[0] == qt_wm_take_focus )
				break;
			    if ( event.xclient.message_type == qt_qt_scrolldone )
				break;
			}

		    default: break;
		    }
		}

		nevents++;
		if ( qApp->x11ProcessEvent( &event ) == 1 )
		    return TRUE;
	    }
	}
    }

    if ( d->shortcut ) {
	return FALSE;
    }

    QApplication::sendPostedEvents();

    const uint exclude_all = ExcludeSocketNotifiers | 0x08;
    // 0x08 == ExcludeTimers for X11 only
    if ( nevents > 0 && ( flags & exclude_all ) == exclude_all &&
	 ( flags & WaitForMore ) ) {
	    return TRUE;
    }
    return FALSE;
}


bool QEventLoop::gsourcePrepare(GSource *gs, int * timeout) 
{
    
	#ifdef DEBUG_QT_GLIBMAINLOOP
		printf("inside gsourcePrepare(1)\n");
	#endif	

	ProcessEventsFlags flags = d->pev_flags;
	
#if defined(QT_THREAD_SUPPORT)
    QMutexLocker locker( QApplication::qt_mutex );
#endif

    // don't block if exitLoop() or exit()/quit() has been called.
    bool canWait = d->exitloop || d->quitnow ? FALSE : (flags & WaitForMore);

    // Process timers and socket notifiers - the common UNIX stuff

    // return the maximum time we can wait for an event.
    static timeval zerotm;
    timeval *tm = 0;
    if ( ! ( flags & 0x08 ) ) {			// 0x08 == ExcludeTimers for X11 only
	    tm = qt_wait_timer();			// wait for timer or X event
	    if ( !canWait ) {
	        if ( !tm )
                    tm = &zerotm;
	        tm->tv_sec  = 0;			// no time to wait
	        tm->tv_usec = 0;
	    }
    }

	// include or exclude SocketNotifiers (by setting or cleaning poll events)  
	
    if ( ! ( flags & ExcludeSocketNotifiers ) ) {
        QPtrListIterator<QSockNotGPollFD> it( d->sn_list );
        QSockNotGPollFD *sn;
        while ( (sn=it.current()) ) {
            ++it;		
		    sn->gPollFD.events = sn->events;  // restore poll events
		}
	} else {
        QPtrListIterator<QSockNotGPollFD> it( d->sn_list );
        QSockNotGPollFD *sn;
        while ( (sn=it.current()) ) {
            ++it;		
		    sn->gPollFD.events = 0;  // delete poll events
		}
	}

	#ifdef DEBUG_QT_GLIBMAINLOOP
		printf("inside gsourcePrepare(2) canwait=%d\n", canWait);
	#endif 

    if ( canWait )
	   emit aboutToBlock();

	
    if ( qt_preselect_handler ) {
	   QVFuncList::Iterator it, end = qt_preselect_handler->end();
	   for ( it = qt_preselect_handler->begin(); it != end; ++it )
	       (**it)();
    }

    // unlock the GUI mutex and select.  when we return from this function, there is
    // something for us to do
#if defined(QT_THREAD_SUPPORT)
    locker.mutex()->unlock();
#endif

	#ifdef DEBUG_QT_GLIBMAINLOOP 
		printf("inside gsourcePrepare(2.1) canwait=%d\n", canWait);
	#endif 	

	// do we have to dispatch events?
    if (hasPendingEvents()) { 
		*timeout = 0; // no time to stay in poll
		
		#ifdef DEBUG_QT_GLIBMAINLOOP
			printf("inside gsourcePrepare(3a)\n");
		#endif		
		
		return FALSE;
    }

	// stay in poll until something happens?
	if (!tm) { // fixme
		*timeout = -1; // wait forever
		#ifdef DEBUG_QT_GLIBMAINLOOP
			printf("inside gsourcePrepare(3b) timeout=%d \n", *timeout);
		#endif	
		
		
		return FALSE;
	}
	
	// else timeout >=0 
	*timeout = tm->tv_sec * 1000 + tm->tv_usec/1000;
	
	#ifdef DEBUG_QT_GLIBMAINLOOP
		printf("inside gsourcePrepare(3c) timeout=%d \n", *timeout);
	#endif	
	

    return FALSE;
}


bool QEventLoop::gsourceCheck(GSource *gs) {
	
	#ifdef DEBUG_QT_GLIBMAINLOOP
		printf("inside gsourceCheck(1)\n");
	#endif	
	
	
	// Socketnotifier events?
	
	QPtrList<QSockNotGPollFD> *list = &d->sn_list;
	
	//if ( list ) {
	
		
		QSockNotGPollFD *sn = list->first();
		while ( sn ) {
			if ( sn->gPollFD.revents )
				return TRUE;
			sn = list->next();
		}
	//}	
	
	if (d->x_gPollFD.revents) { 
		#ifdef DEBUG_QT_GLIBMAINLOOP
			printf("inside gsourceCheck(2) xfd!\n");
		#endif		
		
		return TRUE;  // we got events!
	}
    if (d->threadPipe_gPollFD.revents) { 
		#ifdef DEBUG_QT_GLIBMAINLOOP
			printf("inside gsourceCheck(2) threadpipe!!\n");
		#endif		
		
		return TRUE;  // we got events!
	}
    if (hasPendingEvents()) {
		#ifdef DEBUG_QT_GLIBMAINLOOP
			printf("inside gsourceCheck(2) pendingEvents!\n");
		#endif		
		
		return TRUE; // we got more X11 events!
	}
    // check if we have timers to activate?
 
	timeval * tm =qt_wait_timer();
	
    if (tm && (tm->tv_sec == 0 && tm->tv_usec == 0 )) {
		 #ifdef DEBUG_QT_GLIBMAINLOOP
			printf("inside gsourceCheck(2) qtwaittimer!\n");
		 #endif		
		
         return TRUE;
    }
 
    // nothing to dispatch 
	
	#ifdef DEBUG_QT_GLIBMAINLOOP
		printf("inside gsourceCheck(2) nothing to dispatch!\n");
	#endif		
	
    return FALSE;	
}


bool QEventLoop::gsourceDispatch(GSource *gs) {

    // relock the GUI mutex before processing any pending events
#if defined(QT_THREAD_SUPPORT)
    QMutexLocker locker( QApplication::qt_mutex );
#endif	
#if defined(QT_THREAD_SUPPORT)
    locker.mutex()->lock();
#endif

	int nevents=0;

	ProcessEventsFlags flags = d->pev_flags;

	#ifdef DEBUG_QT_GLIBMAINLOOP
		printf("inside gsourceDispatch(1)\n");
	#endif	
	
    // we are awake, broadcast it
    emit awake();
    emit qApp->guiThreadAwake();

    // some other thread woke us up... consume the data on the thread pipe so that
    // select doesn't immediately return next time
	
	if ( d->threadPipe_gPollFD.revents) {
	    char c;
	    ::read( d->thread_pipe[0], &c, 1 );
    }

    if ( qt_postselect_handler ) {
	    QVFuncList::Iterator it, end = qt_postselect_handler->end();
	    for ( it = qt_postselect_handler->begin(); it != end; ++it )
	        (**it)();
    }

    // activate socket notifiers
    if ( ! ( flags & ExcludeSocketNotifiers )) {
		// if select says data is ready on any socket, then set the socket notifier
		// to pending
		// if ( &d->sn_list ) {
			
	
			QPtrList<QSockNotGPollFD> *list = &d->sn_list;
			QSockNotGPollFD *sn = list->first();
			while ( sn ) {
				if ( sn->gPollFD.revents )
					setSocketNotifierPending( sn->obj );
				sn = list->next();
			}
		// }
	
		nevents += activateSocketNotifiers();
    }

    // activate timers
    if ( ! ( flags & 0x08 ) ) {
		// 0x08 == ExcludeTimers for X11 only
		nevents += activateTimers();
    }



    // return true if we handled events, false otherwise
    //return (nevents > 0);
	
	// now process x11 events!

	#ifdef DEBUG_QT_GLIBMAINLOOP
		printf("inside gsourceDispatch(2) hasPendingEvents=%d\n", hasPendingEvents());
	#endif
	
	if (hasPendingEvents()) {

		// color approx. optimization - only on X11
		qt_reset_color_avail();		
		
		processX11Events();
	
	}
	
#if defined(QT_THREAD_SUPPORT)
    locker.mutex()->unlock();
#endif	
	
	return TRUE;
	
}

bool QEventLoop::hasPendingEvents() const
{
    extern uint qGlobalPostedEventsCount(); // from qapplication.cpp
    return ( qGlobalPostedEventsCount() || ( qt_is_gui_used  ? XPending( QPaintDevice::x11AppDisplay() ) : 0));
}

void QEventLoop::appStartingUp()
{
    if ( qt_is_gui_used )
        d->xfd = XConnectionNumber( QPaintDevice::x11AppDisplay() );
}

void QEventLoop::appClosingDown()
{
    d->xfd = -1;
}
