#pragma once

void * loadDynamicFunc(const char *func_name);

void freeDynamic();

#ifdef DYNAMIC
#define FUNC(name) loadDynamicFunc(#name)
#define FREEFUNCS freeDynamic()
#else
#define FUNC(name) name
#define FREEFUNCS
#endif

