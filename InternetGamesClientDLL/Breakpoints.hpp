#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Setting breakpoints on functions */
bool SetBreakpoints();

/* Exception handler */
LONG CALLBACK VectoredExceptionHandler(EXCEPTION_POINTERS* exceptionInfo);
