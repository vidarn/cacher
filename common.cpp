#include "common.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#ifdef MAX_REL_2015
const wchar_t* PLUGINS_FOLDER = L"C:\\Program Files\\Autodesk\\3ds Max 2015\\plugins";
const wchar_t* DYNAMIC_DLL_PATH = L"C:\\Program Files\\Autodesk\\3ds Max 2015\\plugins\\cacher_dynamic.dll";
#endif

#ifdef MAX_REL_2014
const wchar_t* PLUGINS_FOLDER = L"C:\\Program Files\\Autodesk\\3ds Max 2014\\plugins";
const wchar_t* DYNAMIC_DLL_PATH = L"C:\\Program Files\\Autodesk\\3ds Max 2014\\plugins\\cacher_dynamic.dll";
#endif

void * loadDynamicFunc(const char *func_name, void *func)
{
    if(func == NULL){
        DWORD len = GetDllDirectory(0,NULL);
        WCHAR *oldDir = new WCHAR[len];
        GetDllDirectory(len,oldDir);
        SetDllDirectory(PLUGINS_FOLDER);

        HINSTANCE oldHandle;
        oldHandle = GetModuleHandle(DYNAMIC_DLL_PATH);
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
    }
    return func;
}

void freeDynamic()
{
    DWORD len = GetDllDirectory(0,NULL);
    WCHAR *oldDir = new WCHAR[len];
    GetDllDirectory(len,oldDir);
    SetDllDirectory(PLUGINS_FOLDER);

    HINSTANCE oldHandle;
    oldHandle = GetModuleHandle(DYNAMIC_DLL_PATH);
    if(oldHandle){
        FreeLibrary(oldHandle);
    }
    SetDllDirectory(oldDir);
    delete[] oldDir;
}

