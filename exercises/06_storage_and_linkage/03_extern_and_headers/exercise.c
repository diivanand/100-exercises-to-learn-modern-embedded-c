// =============================================================================
//  06.03 -- Declarations, definitions and extern
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR ("redefinition of
//  'wake_count'"). The error is the exercise.
//
//  Two words C beginners use interchangeably, and C programmers cannot
//  afford to (Effective C ch. 2; ch. 10 "Structuring a Simple Program"):
//
//    DECLARATION   introduces a name and its type. `extern uint32_t x;`
//                  Costs nothing, may appear in every file that needs x.
//    DEFINITION    also reserves the storage. `uint32_t x = 0;`
//                  Must appear EXACTLY ONCE in the whole program.
//
//  The discipline that follows is the entire .h/.c convention:
//
//      the header DECLARES:      extern uint32_t wake_count;
//      exactly one .c DEFINES:   uint32_t wake_count = 0;
//
//  The starter has two modules that both wanted the counter, so both wrote
//  the defining line. In this single-file simulation the compiler catches
//  the redefinition immediately. Historically, across two real files, it got
//  murkier: `uint32_t x;` (no initialiser) is a TENTATIVE definition, and
//  toolchains long merged duplicates of those silently ("common symbols") --
//  two modules each believing they owned x, sharing one. GCC 10 and Clang 11
//  finally made -fno-common the default, so today duplicates are a link
//  error. Write `extern` when you mean "it lives elsewhere" and an
//  initialiser when you mean "it lives here", and neither era can hurt you.
//
//  WHY C NEEDS THE CONVENTION AT ALL. The linker matches symbols by NAME
//  ONLY -- no types survive to check. If power.h says
//
//      extern char *device_name;      // a POINTER lives at that address
//
//  while power.c defines
//
//      char device_name[16] = "L476"; // an ARRAY of chars lives there
//
//  every file including the header compiles happily and then reads the
//  first four BYTES OF THE STRING as if they were a pointer:
//
//      memory at device_name:  'L' '4' '7' '6' ...
//      device_name (as ptr):   0x3637344C -- the string bytes, little-endian,
//                              mistaken for an address and dereferenced
//
//  Nothing diagnoses it; it links. (In one translation unit the compiler
//  would refuse -- which is one more reason the declaration in the header
//  must be INCLUDED BY the defining .c, never re-typed from memory. Then
//  compiler sees both and checks.) CERT DCL40-C.
//
//  TASK
//    One definition, in power.c where the counter is maintained; an extern
//    declaration in the header where every module can see it. Do not change
//    the tests.
//
//  RUN IT
//    ./mec test 06_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

// ---------------------------------------------------------------------------
// power.h -- declares the counter so any file may reference it
// ---------------------------------------------------------------------------
// TODO: this is where the extern DECLARATION belongs.
void power_record_wake(void);

// ---------------------------------------------------------------------------
// logger.h
// ---------------------------------------------------------------------------
uint32_t logger_wakes_logged(void);

// ---------------------------------------------------------------------------
// power.c
// ---------------------------------------------------------------------------
uint32_t wake_count = 0; // power.c believes it owns the counter

void power_record_wake(void) {
  ++wake_count;
}

// ---------------------------------------------------------------------------
// logger.c
// ---------------------------------------------------------------------------
uint32_t wake_count = 0; // TODO: ...and so does logger.c. Redefinition.

uint32_t logger_wakes_logged(void) {
  return wake_count;
}

TEST("both modules see one counter") {
  power_record_wake();
  power_record_wake();
  power_record_wake();
  CHECK_EQ(logger_wakes_logged(), 3u);
}

TEST("the counter keeps counting across tests") {
  power_record_wake();
  CHECK_EQ(logger_wakes_logged(), 4u); // static duration: 3 from the test above
}
