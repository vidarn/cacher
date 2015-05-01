#pragma once

void * loadDynamicFunc(const char *func_name, void *func);

void freeDynamic();

#ifdef DYNAMIC

#define FUNC(name,func) loadDynamicFunc(#name,func)
#define FREEFUNCS freeDynamic()

#else

#define FUNC(name,func) name
#define FREEFUNCS

#endif

