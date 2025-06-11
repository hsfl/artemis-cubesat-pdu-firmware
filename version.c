/*******************************************************************************
  Version Information Implementation File

  Company:
    Artemis Cubesat PDU

  File Name:
    version.c

  Summary:
    This file implements the version information functions.

  Description:
    This file contains the implementation of functions that provide version
    information for the PDU firmware.
*******************************************************************************/

#include "version.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Variables
// *****************************************************************************
// *****************************************************************************

/* Global version information structure */
static const VERSION_INFO versionInfo = {
    .major = VERSION_MAJOR,
    .minor = VERSION_MINOR,
    .patch = VERSION_PATCH,
    .build = VERSION_BUILD
};

/* Global version string */
static const char versionString[] = VERSION_STRING;

// *****************************************************************************
// *****************************************************************************
// Section: Function Implementations
// *****************************************************************************
// *****************************************************************************

const VERSION_INFO* VERSION_GetInfo(void)
{
    return &versionInfo;
}

const char* VERSION_GetString(void)
{
    return versionString;
} 