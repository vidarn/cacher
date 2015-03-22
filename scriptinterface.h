#pragma once
#include "ifnpub.h"
#include "maxscript/util/listener.h"
#include "maxscript/maxscript.h"
#include <maxtypes.h>
#include "common.h"
#include "Dynamic.h"

bool TestScriptInterface(INode *node, MSTR filename, int start, int end){
    bool ret = false;
    SAVEFUNC save_func = (SAVEFUNC)FUNC(SaveFunc);
    if(save_func != NULL){
        ret = save_func(node, filename, start, end);
    } else {
        DWORD lastError = GetLastError();
        DebugPrint(L"Error loading function \"SaveFunc\"\nError code: %d\n",lastError);
    }
    FREEFUNCS;
    return ret;
}

class ScriptInterface : public FPInterfaceDesc
{
public:

    static const Interface_ID id;
    
    enum FunctionIDs
    {
        testscriptinterface,
    };

    ScriptInterface()
        : FPInterfaceDesc(id, _M("CacherCommands"), 0, NULL, FP_CORE, p_end)
    {
        AppendFunction(
            testscriptinterface, 
            _M("CacheMesh"),  
            0,            
            TYPE_BOOL,     
            0,              
            4,              
                _M("node"),     0, TYPE_INODE,
                _M("filename"), 0, TYPE_STRING,
                _M("start"),    0, TYPE_INT,
                _M("end"),      0, TYPE_INT,
            p_end);
	}

    BEGIN_FUNCTION_MAP
        FN_4(testscriptinterface, TYPE_BOOL, TestScriptInterface, TYPE_INODE, TYPE_STRING, TYPE_INT, TYPE_INT)
    END_FUNCTION_MAP
};

const Interface_ID ScriptInterface::id = Interface_ID(0x7232964, 0x3037162a);
static ScriptInterface scriptinterface;
