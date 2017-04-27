/***************************************************************************
 * Copyright (c) 2008-2017 University of Guelph.
 *                         All rights reserved.
 *
 * This file is part of the Pilot software package.  For license
 * information, see the LICENSE file in the top level directory of the
 * Pilot source distribution.
 **************************************************************************/

/*!
********************************************************************************
\file pilot_limits.h
\brief Public header file for compiled-in limits.
\author W. Gardner

Your version of Pilot was compiled with these parameters. They are exposed
here in case you trigger an error for exceeding one.  Changing anything here
without recompiling the library will not be effective.
*******************************************************************************/

#ifndef PILOT_LIMITS_H
#define PILOT_LIMITS_H

/*!
********************************************************************************
\def PI_MAX_NAMELEN
\brief The maximum length for string identifiers.

The length of strings used to identify processes, channels, and bundles.
*******************************************************************************/
#define PI_MAX_NAMELEN 100

/*!
********************************************************************************
\def PI_MAX_FORMATLEN
\brief Maximum number of messages allowed in a single format string.

Most format terms result in one message, so for most purposes this limit applies
to the number of terms in the format string.  The ^ flag and %s datatype each
produce an extra array length message, thus reducing the allowable amount of terms.
*******************************************************************************/
#define PI_MAX_FORMATLEN 50

/*!
********************************************************************************
\def PI_MAX_BUNDLES
\brief Maximum number of bundles that can be created.
*******************************************************************************/
#define PI_MAX_BUNDLES 1024

#endif
