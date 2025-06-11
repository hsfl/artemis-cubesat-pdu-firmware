/*******************************************************************************
  Version Information Header File

  Company:
    HSFL - Dennis Sarsozo

  File Name:
    version.h

  Summary:
    This header file provides version information for the PDU firmware.

  Description:
    This header file defines the version information structure and macros for
    tracking firmware versions and build numbers.
*******************************************************************************/

#ifndef VERSION_H
#define VERSION_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdint.h>

// *****************************************************************************
// *****************************************************************************
// Section: Version Information
// *****************************************************************************
// *****************************************************************************

/* Version Information Structure */
typedef struct {
    uint8_t major;      /* Major version number */
    uint8_t minor;      /* Minor version number */
    uint8_t patch;      /* Patch version number */
    uint32_t build;     /* Build number */
} VERSION_INFO;

/* Current Version Information */
#define VERSION_MAJOR       1
#define VERSION_MINOR       0
#define VERSION_PATCH       0
#define VERSION_BUILD       1

/* Version String Macros */
#define VERSION_STRINGIFY(x) #x
#define VERSION_MAKE_STRING(x) VERSION_STRINGIFY(x)

#define VERSION_STRING \
    VERSION_MAKE_STRING(VERSION_MAJOR) "." \
    VERSION_MAKE_STRING(VERSION_MINOR) "." \
    VERSION_MAKE_STRING(VERSION_PATCH) "-" \
    VERSION_MAKE_STRING(VERSION_BUILD)

/* Function to get version information */
const VERSION_INFO* VERSION_GetInfo(void);

/* Function to get version string */
const char* VERSION_GetString(void);

#endif /* VERSION_H */ 