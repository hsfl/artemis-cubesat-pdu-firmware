/**
 * @file version.h
 * @brief Version information for Artemis PDU firmware
 */

#ifndef VERSION_H
#define VERSION_H

// Version information - match with revision board, then use patch for any software update
#define VERSION_MAJOR       2
#define VERSION_MINOR       2
#define VERSION_PATCH       1

// Build information
#define BUILD_DATE         __DATE__
#define BUILD_TIME         __TIME__

// Version string macro
#define VERSION_STRING     "v" STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)
#define STRINGIFY(x)       #x

// Full version info string
#define VERSION_INFO       VERSION_STRING " (" BUILD_DATE " " BUILD_TIME ")"

#endif /* VERSION_H */ 