/*
 *	PROGRAM:	JRD access method
 *	MODULE:		thd.h
 *	DESCRIPTION:	Thread support definitions
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2002.10.28 Sean Leyne - Completed removal of obsolete "DGUX" port
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 */
/*
$Id: thd.h,v 1.21 2004-05-06 18:07:02 brodsom Exp $
*/

#ifndef JRD_THD_H
#define JRD_THD_H

#ifdef SOLARIS
#ifdef SOLARIS_MT
#include <thread.h>
#define THD_MUTEX_STRUCT	mutex_t
#define THD_COND_STRUCT		cond_t
#define THD_RWLOCK_STRUCT	rwlock_t
#endif
#endif
 

#ifdef HAVE_POSIX_THREADS
#ifdef SUPERSERVER
#define POSIX_THREADS
#endif
#ifdef SUPERCLIENT
#if defined(LINUX) || defined(FREEBSD)
/* The following ifdef was added to build thread safe gds shared
   library on linux platform. It seems the gdslib works now (20020220)
   with thread enabled applications. Anyway, more tests should be 
   done as I don't have deep knowledge of the interbase/firebird 
   engine and this change may imply side effect I haven't known 
   about yet. Tomas Nejedlik (tomas@nejedlik.cz) */
#define POSIX_THREADS
#endif
#endif
#endif

#ifdef VMS
//
//#define POSIX_THREADS
//
#include "../jrd/isc.h"
#define THD_MUTEX_STRUCT	MTX_T
#endif

#ifdef WIN_NT
#define THD_MUTEX_STRUCT	struct IB_RTL_CRITICAL_SECTION

/* The following code fragment has been extracted from <winnt.h>.
   Microsoft being what it is, one is forced to include just about the
   entire world even if you want only one little declaration.  Since
   including the whole world tends to break things (Microsoft also
   doesn't believe in benign naming conversions) we must copy code. */

typedef struct IB_RTL_CRITICAL_SECTION
{
	void*	DebugInfo;
	ULONG	LockCount;
	ULONG	RecursionCount;
	void*	OwningThread;
	void*	LockSemaphore;
	ULONG	Reserved;
};
#endif

#ifdef MULTI_THREAD
#define ANY_THREADING
#endif
#ifdef V4_THREADING
#define ANY_THREADING
#endif

#ifdef POSIX_THREADS
#include <pthread.h>
#define THD_MUTEX_STRUCT	pthread_mutex_t
#define THD_COND_STRUCT		pthread_cond_t

#ifndef PTHREAD_PROCESS_SHARED
#define PTHREAD_PROCESS_SHARED
#endif

#ifndef PTHREAD_CREATE_DETACHED
#define PTHREAD_CREATE_DETACHED
#endif

#ifdef HAVE_PTHREAD_KEYCREATE
#define pthread_key_create	pthread_keycreate
#endif
#endif

#ifndef THD_MUTEX_STRUCT
#define THD_MUTEX_STRUCT	SCHAR
#endif
#ifndef THD_COND_STRUCT
#define THD_COND_STRUCT		SCHAR
#endif

#ifdef ANY_THREADING
#define GET_THREAD_DATA		((THDD) THD_get_specific())
#else
#define GET_THREAD_DATA		gdbb
#endif

#ifdef MULTI_THREAD
#ifdef SUPERSERVER
#define THREAD_ENTER		SCH_enter()
#define THREAD_EXIT		SCH_exit()
#define THREAD_VALIDATE		SCH_validate()
#define SWEEP_THREAD
//#define GARBAGE_THREAD
#else
#define THREAD_ENTER		gds__thread_enter()
#define THREAD_EXIT		gds__thread_exit()
#define THREAD_VALIDATE
#define AST_THREAD
#endif
#define THREAD_SLEEP(msecs)	THD_sleep (msecs)
#define THREAD_YIELD		THD_yield()
#endif

#ifndef THREAD_ENTER
#define THREAD_ENTER
#define THREAD_EXIT
#define THREAD_VALIDATE
#define THREAD_SLEEP(msecs)	THD_sleep (msecs)
#define THREAD_YIELD
#endif

/* Thread priorities (may be ignored) */

const int THREAD_high			= 1;
const int THREAD_medium_high	= 2;
const int THREAD_medium			= 3;
const int THREAD_medium_low		= 4;
const int THREAD_low			= 5;
const int THREAD_critical		= 6;

/* Thread option flags */

const int THREAD_ast		= 1;	/* Thread can/should run at ast level */
const int THREAD_blast		= 2;	/* Blow away thread during exit handler */
const int THREAD_wait		= 4;	/* Somebody will wait for thread exit */

/* Thread quanta */

const int QUANTUM			= 100;	/* Default quantum */
const int SWEEP_QUANTUM		= 10;	/* Make sweeps less disruptive */


/* Thread specific data */

typedef struct thdd {
	void *thdd_prior_context;
	ULONG thdd_type;		/* what kind of structrue this is */
} *THDD;

/* Thread structure types */

const USHORT THDD_TYPE_TGBL	= 1;		/* used by backup/restore */
const USHORT THDD_TYPE_TSQL	= 2;		/* used by DSQL */
const USHORT THDD_TYPE_TDBB	= 3;		/* used in engine */
const USHORT THDD_TYPE_TRDB	= 4;		/* used in remote interface */
const USHORT THDD_TYPE_TDBA	= 5;		/* used in DBA utility */
const USHORT THDD_TYPE_TIDB	= 6;		/* used by interprocess server */
const USHORT THDD_TYPE_TALICE	= 7;		/* used by gfix */
const USHORT THDD_TYPE_TSEC	= 8;		/* used by gsec */

/* General purpose in use object structure */

typedef struct iuo {
	struct iuo *iuo_next;
	void *iuo_object[10];
	USHORT iuo_in_use_count;
} *IUO;

/* Mutex structure */

typedef struct mutx_t {
	THD_MUTEX_STRUCT mutx_mutex;
} MUTX_T, *MUTX;

/* Recursive mutex structure */
typedef struct rec_mutx_t {
	MUTX_T rec_mutx_mtx[1];
	FB_THREAD_ID rec_mutx_id;
	SLONG rec_mutx_count;
} REC_MUTX_T, *REC_MUTX;

/* Combined mutex and condition variable structure */
#ifndef SOLARIS
typedef struct cond_t {
	THD_MUTEX_STRUCT cond_mutex;
	THD_COND_STRUCT cond_cond;
} COND_T, *COND;

#endif
/* Read/write lock structure */

typedef struct wlck_t {
#ifdef THD_RWLOCK_STRUCT
	THD_RWLOCK_STRUCT wlck_wlock;
#else
	COND_T wlck_cond;
	int wlck_count;
	int wlck_waiters;
#endif
} WLCK_T, *WLCK;

const int WLCK_read		= 1;
const int WLCK_write	= 2;

/* Threading allocation size */

#define THREAD_STRUCT_SIZE(type,n)	(n * sizeof (type) + ALIGNMENT)
#define THREAD_STRUCT_ALIGN(blk)	FB_ALIGN((U_IPTR) blk, ALIGNMENT)

#ifdef V4_THREADING
#define V4_INIT				THD_init()
#define V4_GLOBAL_MUTEX_LOCK		THD_mutex_lock_global()
#define V4_GLOBAL_MUTEX_UNLOCK		THD_mutex_unlock_global()
#define V4_MUTEX_INIT(mutx)		THD_mutex_init (mutx)
#define V4_MUTEX_LOCK(mutx)		THD_mutex_lock (mutx)
#define V4_MUTEX_UNLOCK(mutx)		THD_mutex_unlock (mutx)
#define V4_MUTEX_DESTROY(mutx)		THD_mutex_destroy (mutx)
#define V4_RW_LOCK_INIT_N(wlck,n)	THD_wlck_init_n (wlck, n)
#define V4_RW_LOCK_INIT(wlck)		THD_wlck_init (wlck)
#define V4_RW_LOCK_LOCK(wlck,type)	THD_wlck_lock (wlck, type)
#define V4_RW_LOCK_UNLOCK(wlck)		THD_wlck_unlock (wlck)
#define V4_RW_LOCK_DESTROY(wlck)	THD_wlck_destroy (wlck)
#define V4_RW_LOCK_DESTROY_N(wlck,n)	THD_wlck_destroy_n (wlck, n)
// BRS. 03/23/2003
// Those empty defines was substituted with #ifdef V4_THREADING
//#else
//#define V4_INIT
//#define V4_GLOBAL_MUTEX_LOCK
//#define V4_GLOBAL_MUTEX_UNLOCK
//#define V4_MUTEX_INIT(mutx)
//#define V4_MUTEX_LOCK(mutx)
//#define V4_MUTEX_UNLOCK(mutx)
//#define V4_MUTEX_DESTROY(mutx)
//#define V4_RW_LOCK_INIT_N(wlck,n)
//#define V4_RW_LOCK_INIT(wlck)
//#define V4_RW_LOCK_LOCK(wlck,type)
//#define V4_RW_LOCK_UNLOCK(wlck)
//#define V4_RW_LOCK_DESTROY_N(wlck,n)
//#define V4_RW_LOCK_DESTROY(wlck)
#endif

#ifdef ANY_THREADING
#define THD_INIT			THD_init()
#define THD_GLOBAL_MUTEX_LOCK		THD_mutex_lock_global()
#define THD_GLOBAL_MUTEX_UNLOCK		THD_mutex_unlock_global()
#define THD_MUTEX_INIT_N(mutx,n)	THD_mutex_init_n (mutx, n)
#define THD_MUTEX_DESTROY_N(mutx,n)	THD_mutex_destroy_n (mutx, n)
#define THD_MUTEX_INIT(mutx)		THD_mutex_init (mutx)
#define THD_MUTEX_LOCK(mutx)		THD_mutex_lock (mutx)
#define THD_MUTEX_UNLOCK(mutx)		THD_mutex_unlock (mutx)
#define THD_MUTEX_DESTROY(mutx)		THD_mutex_destroy (mutx)
#else
#define THD_INIT
#define THD_GLOBAL_MUTEX_LOCK
#define THD_GLOBAL_MUTEX_UNLOCK
#define THD_MUTEX_INIT_N(mutx,n)
#define THD_MUTEX_DESTROY_N(mutx,n)
#define THD_MUTEX_INIT(mutx)
#define THD_MUTEX_LOCK(mutx)
#define THD_MUTEX_UNLOCK(mutx)
#define THD_MUTEX_DESTROY(mutx)
#endif

#endif /* JRD_THD_H */
