#include "common.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

void * loadDynamicFunc(const char *func_name)
{
    DWORD len = GetDllDirectory(0,NULL);
    WCHAR *oldDir = new WCHAR[len];
    GetDllDirectory(len,oldDir);
    SetDllDirectory(L"C:\\Program Files\\Autodesk\\3ds Max 2015\\plugins");

    HINSTANCE oldHandle;
    oldHandle = GetModuleHandle(L"C:\\Program Files\\Autodesk\\3ds Max 2015\\plugins\\cacher_dynamic.dll");
    if(oldHandle){
        FreeLibrary(oldHandle);
    }
    //Note(Vidar) Add a breakpoint here if you want to unload the dlls

    HINSTANCE dynamic_dll = LoadLibrary(L"cacher_dynamic.dll");
    SetDllDirectory(oldDir);
    delete[] oldDir;
    if(dynamic_dll != NULL){
        return GetProcAddress(dynamic_dll, func_name);
    } else {
        DWORD lastError = GetLastError();
    }
    return NULL;
}

void freeDynamic()
{
    DWORD len = GetDllDirectory(0,NULL);
    WCHAR *oldDir = new WCHAR[len];
    GetDllDirectory(len,oldDir);
    SetDllDirectory(L"C:\\Program Files\\Autodesk\\3ds Max 2015\\plugins");

    HINSTANCE oldHandle;
    oldHandle = GetModuleHandle(L"C:\\Program Files\\Autodesk\\3ds Max 2015\\plugins\\cacher_dynamic.dll");
    if(oldHandle){
        FreeLibrary(oldHandle);
    }
    SetDllDirectory(oldDir);
}

