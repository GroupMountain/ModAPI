// LeviLamina only loads a mod whose DLL uses its unified memory allocation operators, so every mod
// (including this test mod) needs exactly one translation unit like this one. It mirrors
// src/modapi/core/MemoryOperators.cpp.



#define LL_MEMORY_OPERATORS
#include <ll/api/memory/MemoryOperators.h>
