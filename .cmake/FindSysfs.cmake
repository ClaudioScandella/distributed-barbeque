#
# Locate Sysfs include paths and libraries
# libsysfs is part of sysfsutils: http://linux-diag.sourceforge.net

# This module defines
# Sysfs_INCLUDE_DIR, where to find libsysfs.h, etc.
# Sysfs_LIBRARIES, the libraries to link against to use libsysfs.
# Sysfs_FOUND, If false, don't try to use libsysfs.

find_path(Sysfs_INCLUDE_DIR sysfs/libsysfs.h)
find_library(Sysfs_LIBRARIES sysfs)

set(Sysfs_FOUND 0)
if (Sysfs_INCLUDE_DIR)
  if (Sysfs_LIBRARIES)
    set(Sysfs_FOUND 1)
    message(STATUS "Found Sysfs: ${Sysfs_LIBRARIES}")
  endif (Sysfs_LIBRARIES)
endif (Sysfs_INCLUDE_DIR)

mark_as_advanced(
  Sysfs_INCLUDE_DIR
  Sysfs_LIBRARIES
)
