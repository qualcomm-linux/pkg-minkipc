find_path(QCOMTEE_INCLUDE_DIR
        NAMES qcomtee_object_types.h
        HINTS ${QCOMTEE_DIR_HINT}
        PATH_SUFFIXES include
)

find_library(QCOMTEE_LIBRARY
        NAMES qcomtee
        HINTS ${QCOMTEE_DIR_HINT}
        PATH_SUFFIXES lib
)

if(QCOMTEE_INCLUDE_DIR AND QCOMTEE_LIBRARY)
        set(QCOMTEE_FOUND TRUE)
        set(QCOMTEE_LIBRARIES ${QCOMTEE_LIBRARY})
        set(QCOMTEE_INCLUDE_DIRS ${QCOMTEE_INCLUDE_DIR})
else()
        set(QCOMTEE_FOUND FALSE)
endif()

mark_as_advanced(QCOMTEE_INCLUDE_DIR QCOMTEE_LIBRARY)
