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
\file pilot_log_colors.h
\brief Header file for color assignments in MPE log.
\author Sid Bao

MPE logging for display in Jumpshot (-pisvc=j) uses a default color scheme that
can be altered by modifying this file and recompiling.  Valid color names have to
be taken from the X11 color chart.

The categories of logged states (rectangles) and events (bubbles) are as follows
(symbol LOG_CLR_xxx):
- CONFIG: configuration phase
- COMPUTE: computation
- OUTPUT: PI_Write
- INPUT: PI_Read
- COLL_OUT: collective output (PI_Broadcast, etc.)
- COLL_INP: collective input (PI_Reduce, etc.)
- STATE_EVT: event inside a state (e.g., message passing data)
- NONSTATE_EVT: other event not connected with a state (e.g., PI_TrySelect Result)

It is intended that COLL_OUT/INP be darker shades of OUTPUT/INPUT.
*******************************************************************************/

#ifndef PILOT_LOG_COLORS_H
#define PILOT_LOG_COLORS_H

#define LOG_CLR_CONFIG      "bisque"
#define LOG_CLR_COMPUTE     "gray"
#define LOG_CLR_OUTPUT      "green"
#define LOG_CLR_INPUT       "red"
#define LOG_CLR_COLL_OUT    "ForestGreen"
#define LOG_CLR_COLL_INP    "IndianRed"
#define LOG_CLR_STATE_EVT   "yellow"
#define LOG_CLR_NONSTATE_EVT "cyan"

#endif
