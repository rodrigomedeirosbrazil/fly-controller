#ifndef CONFIG_CONTROLLER_H
#define CONFIG_CONTROLLER_H

/**
 * Controller Type Configuration
 *
 * This file defines the controller types and helper macros for conditional compilation.
 *
 * Usage in platformio.ini:
 *   build_flags = -D CONTROLLER_TYPE=HOBBYWING
 *   build_flags = -D CONTROLLER_TYPE=TMOTOR
 *   build_flags = -D CONTROLLER_TYPE=XAG
 */

// Controller type enumeration
// These values are used as string literals in the build flags
#define CONTROLLER_TYPE_XAG       1
#define CONTROLLER_TYPE_HOBBYWING 2
#define CONTROLLER_TYPE_TMOTOR    3

// Default to HOBBYWING if not specified
#ifndef CONTROLLER_TYPE
#define CONTROLLER_TYPE CONTROLLER_TYPE_HOBBYWING
#endif

// Helper macros for controller type checking (for use in #if directives)
// These work directly in preprocessor directives
#if CONTROLLER_TYPE == CONTROLLER_TYPE_XAG
    #define IS_XAG 1
    #define IS_HOBBYWING 0
    #define IS_TMOTOR 0
    #define USES_CAN_BUS 0
#elif CONTROLLER_TYPE == CONTROLLER_TYPE_HOBBYWING
    #define IS_XAG 0
    #define IS_HOBBYWING 1
    #define IS_TMOTOR 0
    #define USES_CAN_BUS 1
#elif CONTROLLER_TYPE == CONTROLLER_TYPE_TMOTOR
    #define IS_XAG 0
    #define IS_HOBBYWING 0
    #define IS_TMOTOR 1
    #define USES_CAN_BUS 1
#else
    #error "Invalid CONTROLLER_TYPE"
#endif

// Compile-time validation
#if CONTROLLER_TYPE != CONTROLLER_TYPE_XAG && \
    CONTROLLER_TYPE != CONTROLLER_TYPE_HOBBYWING && \
    CONTROLLER_TYPE != CONTROLLER_TYPE_TMOTOR
#error "Invalid CONTROLLER_TYPE. Must be CONTROLLER_TYPE_XAG, CONTROLLER_TYPE_HOBBYWING, or CONTROLLER_TYPE_TMOTOR"
#endif

#endif // CONFIG_CONTROLLER_H

