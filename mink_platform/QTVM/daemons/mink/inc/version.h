// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef VERSION_H
#define VERSION_H

/**
    Macro used to set the version.

    - Major = 4 bits (maximum 15). \n
      Major indicates an incompatible API change. \n
    - Minor = 12 bits (maximum 4096). \n
      Minor version is the addition/removal of features in a backward compatible manner \n
    - Patch = 16 bits (maximum 65535). \n
      Patch can indicate backward compatible bug fixes.
*/
#define SET_VERSION(major, minor, patch) \
  (((major & 0xF) << 28) | ((minor & 0xFFF) << 16) | (patch & 0xFFFF))

// Version number
#define QTVM_PLATFORM_VERSION SET_VERSION(1, 0, 0)

// Release Note
/*
 * Version 1.0.0
 * - Initial release
 *   1.0.0 as the initial release, which has a checkpoint that all FRs on Pakala
 *   have been completed and switched to the stable branch sec.mink.1.15.
 *
 * - Implemented basic features
 *   FR100223: Mink SW versioning for QTVM Platform
 *
 * - Fixed known bugs
 *   None
 */

#endif // VERSION_H
