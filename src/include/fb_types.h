/*
 *
 *  The contents of this file are subject to the Mozilla Public
 *  License Version 1.1 (the "License"); you may not use this file
 *  except in compliance with the License. You may obtain a copy of
 *  the License at http://www.mozilla.org/MPL/
 *  Alternatively, the contents of this file may be used under the
 *  terms of the GNU General Public License Version 2 or later (the
 *  "GPL"), in which case the provisions of the GPL are applicable
 *  instead of those above. You may obtain a copy of the Licence at
 *  http://www.gnu.org/copyleft/gpl.html
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Relevant for more details.
 *
 *    This file was created by members of the firebird development team.
 *    All individual contributions remain the Copyright (C) of those
 *    individuals.  Contributors to this file are either listed here or
 *    can be obtained from a CVS history command.
 *
 *   All rights reserved.
 *
 *   Contributor(s):
 *       Mike Nordel <tamlin@algonet.se>
 *       Mark O'Donohue <mark.odonohue@ludwig.edu.au>
 *
 *
 *  $Id: fb_types.h,v 1.41 2004-01-29 05:56:33 skidder Exp $
 *
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "OS/2" port
 *
 */


#ifndef INCLUDE_FB_TYPES_H
#define INCLUDE_FB_TYPES_H


/******************************************************************/
/* Define type, export and other stuff based on c/c++ and Windows */
/******************************************************************/
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
	#define  ISC_EXPORT	__stdcall
	#define  ISC_EXPORT_VARARG	__cdecl
#else
	#define  ISC_EXPORT
	#define  ISC_EXPORT_VARARG
#endif

/*******************************************************************/
/* 64 bit Integers                                                 */
/*******************************************************************/

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__)) && !defined(__GNUC__)
typedef __int64				ISC_INT64;
typedef unsigned __int64	ISC_UINT64;
#else
typedef long long int			ISC_INT64;
typedef unsigned long long int	ISC_UINT64;
#endif

// Nickolay: it is easier to assume that integer is at least 32-bit.
// This comes from limitation that we cannot reliably detect datatype size at
// compile time in cases when we do not control compilation (public headers) 
// We are not going to support 16-bit platforms, right?
//
// Temporarly restrict new definition until ULONG clash with Windows
// type is solved. Win64 port is not possible before that point.
// Cannot use SIZEOF_LONG define here because we are in a public header
#if defined(_LP64) || defined(__LP64__) || defined(__arch64__)
	// EKU: Firebird requires (S)LONG to be 32 bit
	typedef int SLONG;
	typedef unsigned int ULONG;
#else
	typedef long SLONG;
	typedef unsigned long ULONG;
#endif

typedef struct {
	SLONG high;
	ULONG low;
} SQUAD;

struct GDS_QUAD_t {
	SLONG gds_quad_high;
	ULONG gds_quad_low;
};

typedef struct GDS_QUAD_t GDS_QUAD;
typedef struct GDS_QUAD_t ISC_QUAD;

#define	isc_quad_high	gds_quad_high
#define	isc_quad_low	gds_quad_low

// Basic data types

// typedef signed char SCHAR;
// TMN: TODO It seems SCHAR is used just about *everywhere* where a plain
// "char" is really intended. This currently forces us to this bad definition.
//
typedef char SCHAR;


typedef unsigned char UCHAR;
typedef short SSHORT;
typedef unsigned short USHORT;


//
// TMN: some misc data types from all over the place
//
struct vary
{
	USHORT vary_length;
	char   vary_string[1];
};
// TMN: Currently we can't do this, since remote uses a different
// definition of VARY than the rest of the code! :-<
//typedef vary* VARY;

struct lstring
{
	ULONG	lstr_length;
	ULONG	lstr_allocated;
	UCHAR*	lstr_address;
};


typedef unsigned char BOOLEAN;
typedef char TEXT;				/* To be expunged over time */
/*typedef unsigned char STEXT;	Signed text - not used
typedef unsigned char UTEXT;	Unsigned text - not used */
typedef unsigned char BYTE;		/* Unsigned byte - common */
/*typedef char SBYTE;			Signed byte - not used */
typedef long ISC_STATUS;
typedef long IPTR;
typedef unsigned long U_IPTR;
typedef void (*FPTR_VOID) ();
typedef void (*FPTR_VOID_PTR) (void*);
typedef int (*FPTR_INT) ();
typedef int (*FPTR_INT_VOID_PTR) (void*);
typedef void (*FPTR_PRINT_CALLBACK) (void*, SSHORT, const char*);
/* Used for isc_version */
typedef void (*FPTR_VERSION_CALLBACK)(void*, const char*);
/* Used for isc_que_events and internal functions */
typedef void (*FPTR_EVENT_CALLBACK)(void*, USHORT, const UCHAR*);

// The type of JRD's ERR_post, DSQL's ERRD_post & post_error,
// REMOTE's move_error & GPRE's post_error.
typedef void (*FPTR_ERROR) (ISC_STATUS, ...);

typedef ULONG RCRD_OFFSET;
typedef USHORT FLD_LENGTH;
/* CVC: internal usage. I suspect the only reason to return int is that
vmslock.cpp:LOCK_convert() calls VMS' sys$enq that may require this signature,
but our code never uses the return value. */
typedef int (*lock_ast_t)(void*);

#define ISC_STATUS_LENGTH	20
typedef ISC_STATUS ISC_STATUS_ARRAY[ISC_STATUS_LENGTH];

/* Number of elements in an array */
#define FB_NELEM(x)	((int)(sizeof(x) / sizeof(x[0])))
#define FB_ALIGN(n, b) ((n + b - 1) & ~(b - 1))

#endif /* INCLUDE_FB_TYPES_H */

