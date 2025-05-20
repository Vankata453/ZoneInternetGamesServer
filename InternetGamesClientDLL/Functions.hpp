#pragma once

/* Function pointers */
#ifdef XP_GAMES
extern void* inet_addr;
extern void* htons;
#else
extern void* GetAddrInfoW;
#endif

/* Acquiring function pointers */
bool GetFunctions();
