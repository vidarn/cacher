#include "common.h"
#include "params.h"
#include "Cacher.h"
#include "cached_frame.h"
#include "Dynamic.h"

#define Cacher_CLASS_ID Class_ID(0x854be7e6, 0x6a6b9f59)

#define PBLOCK_REF  0

DLGPROCFUNC    dlg_func = NULL;
LOADFUNC      load_func = NULL;
FREECACHEFUNC free_func = NULL;

class Cacher : public SimpleObject2
{
public:
    //Constructor/Destructor
    Cacher();
    virtual ~Cacher();

    // Parameter block handled by parent

    // From BaseObject
    virtual CreateMouseCallBack* GetCreateMouseCallBack();
    virtual const MCHAR* GetObjectName(){return GetString(IDS_OBJECT_NAME);}

    // From Object
    virtual BOOL HasUVW();
    virtual void SetGenUVW(BOOL sw);
    virtual int IntersectRay(TimeValue t, Ray& ray, float& at, Point3& norm);

    // From Animatable
    virtual void BeginEditParams( IObjParam  *ip, ULONG flags,Animatable *prev);
    virtual void EndEditParams( IObjParam *ip, ULONG flags,Animatable *next);

    // From SimpleObject
    virtual void BuildMesh(TimeValue t);
    virtual void InvalidateUI();

    //From Animatable
    virtual Class_ID ClassID() {return Cacher_CLASS_ID;}
    virtual SClass_ID SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    virtual void GetClassName(TSTR& s) {s = GetString(IDS_CLASS_NAME);}

    virtual RefTargetHandle Clone( RemapDir& remap );

    virtual int NumParamBlocks() { return 1; }                  // return number of ParamBlocks in this instance
    virtual IParamBlock2* GetParamBlock(int /*i*/) { return pblock2; } // return i'th ParamBlock
    virtual IParamBlock2* GetParamBlockByID(BlockID id) { return (pblock2->ID() == id) ? pblock2 : NULL; } // return id'd ParamBlock

    void DeleteThis() { delete this; }

    // members
    CachedData m_cached_data;
};

class CacherDlgProc : public ParamMap2UserDlgProc {
    public:
        Cacher *m_cacher;
        INT_PTR DlgProc(TimeValue t, IParamMap2 *  map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
        {
            INT_PTR ret = FALSE;
            dlg_func = (DLGPROCFUNC)FUNC(DlgFunc,dlg_func);
            if(dlg_func != NULL){
                DebugPrint(L"dlg_func: %d\n", (int)dlg_func);
                //TODO(Vidar) Why does it crash here? Is it after reloading the dll? No! Perhaps it's invalid when we start?
                ret = dlg_func(t,map,hWnd,msg,wParam,lParam,&(m_cacher->m_cached_data));
            } else {
                DWORD lastError = GetLastError();
                DebugPrint(L"Error loading function \"DlgFunc\"\nError code: %d\n",lastError);
            }
            if(msg == WM_COMMAND){
                int control = LOWORD(wParam);
                int command = HIWORD(wParam);
                if(control == IDC_UNLOAD && command == 0){
                    FREEFUNCS;
                    dlg_func = NULL;
                    load_func = NULL;
                }
            }
            return ret;
        }
        void    DeleteThis() {} //NOTE(Vidar) user_dlg_proc is static
};

static CacherDlgProc user_dlg_proc;


class CacherClassDesc : public ClassDesc2 
{
public:
    virtual int IsPublic()                          { return TRUE; }
    virtual void* Create(BOOL /*loading = FALSE*/)      { return new Cacher(); }
    virtual const TCHAR *   ClassName()             { return GetString(IDS_CLASS_NAME); }
    virtual SClass_ID SuperClassID()                { return GEOMOBJECT_CLASS_ID; }
    virtual Class_ID ClassID()                      { return Cacher_CLASS_ID; }
    virtual const TCHAR* Category()                 { return GetString(IDS_CATEGORY); }

    virtual const TCHAR* InternalName()             { return _T("Cacher"); }    // returns fixed parsable name (scripter-visible name)
    virtual HINSTANCE HInstance()                   { return hInstance; }                   // returns owning module handle
    

};


ClassDesc2* GetCacherDesc() { 
    static CacherClassDesc CacherDesc;
    return &CacherDesc; 
}

enum { cacher_params };

static ParamBlockDesc2 cacher_param_blk ( cacher_params, _T("params"),  0, GetCacherDesc(), 
    P_AUTO_CONSTRUCT + P_AUTO_UI, PBLOCK_REF, 
    //rollout
    IDD_PANEL, IDS_PARAMS, 0, 0, &user_dlg_proc,
    // params
    pb_filename,        _T("filename"),     TYPE_FILENAME,  0,  IDS_FILENAME, 
        p_ui,           TYPE_FILEOPENBUTTON,        IDC_FILENAME, 
        p_end,
    pb_status,          _T("status"),       TYPE_STRING,    0,  IDS_STATUS, 
        p_ui,           TYPE_EDITBOX,       IDC_STATUS, 
        p_enabled,      TRUE,
        p_end,
    pb_ram,             _T("load_to_ram"),  TYPE_BOOL,      0,  IDS_CACHERAM,
        p_ui,           TYPE_CHECKBUTTON, IDC_CACHERAM,
        p_end,
    pb_start,           _T("start"),        TYPE_INT,       0,  IDS_START,
		p_default, 		0, 
		p_range, 		0,1000, 
		p_ui, 			TYPE_SPINNER,		EDITTYPE_INT,   IDC_STARTEDIT,	IDC_STARTSPIN, 1, 
        p_end,
    pb_end,             _T("end"),          TYPE_INT,       0,  IDS_END,
		p_default, 		100, 
		p_range, 		0,1000, 
		p_ui, 			TYPE_SPINNER,		EDITTYPE_INT,   IDC_ENDEDIT,	IDC_ENDSPIN, 1, 
        p_end,
    p_end
    );




//--- Cacher -------------------------------------------------------

Cacher::Cacher()
{
    m_cached_data.start = 1;
    m_cached_data.end   = 0;
    m_cached_data.frames = NULL;
    GetCacherDesc()->MakeAutoParamBlocks(this);
}

Cacher::~Cacher()
{
    DebugPrint(L"Destructor!\n");
    free_func = (FREECACHEFUNC)FUNC(FreeCache, free_func);
    if(free_func != NULL){
        free_func(&m_cached_data);
    } else {
        DWORD lastError = GetLastError();
        DebugPrint(L"Error loading function \"Freefunc\"\nError code: %d\n",lastError);
    }
    
}

void Cacher::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
    user_dlg_proc.m_cacher = this;

    SimpleObject2::BeginEditParams(ip,flags,prev);
    GetCacherDesc()->BeginEditParams(ip, this, flags, prev);
}

void Cacher::EndEditParams( IObjParam* ip, ULONG flags, Animatable* next )
{
    //TODO: Save plugin parameter values into class variables, if they are not hosted in ParamBlocks. 
    SimpleObject2::EndEditParams(ip,flags,next);
    GetCacherDesc()->EndEditParams(ip, this, flags, next);
}

//From Object
BOOL Cacher::HasUVW() 
{ 
    //TODO: Return whether the object has UVW coordinates or not
    return TRUE; 
}

void Cacher::SetGenUVW(BOOL sw) 
{
    if (sw==HasUVW()) 
        return;
    //TODO: Set the plugin's internal value to sw
}

//Class for interactive creation of the object using the mouse
class CacherCreateCallBack : public CreateMouseCallBack {
    IPoint2 sp0;              //First point in screen coordinates
    Cacher* ob; //Pointer to the object 
    Point3 p0;                //First point in world coordinates
    Point3 p1; 
public: 
    int proc( ViewExp *vpt,int msg, int point, int flags, IPoint2 m, Matrix3& mat);
    void SetObj(Cacher *obj) {ob = obj;}
};

int CacherCreateCallBack::proc(ViewExp *vpt,int msg, int point, int /*flags*/, IPoint2 m, Matrix3& mat )
{
    TimeValue t (0);
    if (msg==MOUSE_POINT) {
        ob->suspendSnap = TRUE;
        sp0 = m;
        p0 = vpt->SnapPoint(m,m,NULL,SNAP_IN_PLANE);
        mat.SetTrans(p0); // sets the pivot location
        return CREATE_STOP;
    } 
    else {
        if (msg == MOUSE_ABORT) return CREATE_ABORT;
    }
    return TRUE;
}

static CacherCreateCallBack CacherCreateCB;

//From BaseObject
CreateMouseCallBack* Cacher::GetCreateMouseCallBack() 
{
    CacherCreateCB.SetObj(this);
    return(&CacherCreateCB);
}


//From SimpleObject
void Cacher::BuildMesh(TimeValue t)
{
    load_func = (LOADFUNC)FUNC(LoadFunc, load_func);
    if(load_func != NULL){
        load_func(&mesh, t, pblock2, &ivalid, m_cached_data, hInstance);
    } else {
        DWORD lastError = GetLastError();
        DebugPrint(L"Error loading function \"LoadFunc\"\nError code: %d\n",lastError);
    }
    mesh.InvalidateGeomCache();
}

void Cacher::InvalidateUI() 
{
    // Hey! Update the param blocks
    pblock2->GetDesc()->InvalidateUI();
}


// From Object
int Cacher::IntersectRay(TimeValue /*t*/, Ray& /*ray*/, float& /*at*/, Point3& /*norm*/)
{
    //TODO: Return TRUE after you implement this method
    return FALSE;
}

// From ReferenceTarget
RefTargetHandle Cacher::Clone(RemapDir& remap) 
{
    Cacher* newob = new Cacher();   
    //TODO: Make a copy all the data and also clone all the references
    newob->ReplaceReference(0,remap.CloneRef(pblock2));
    newob->ivalid.SetEmpty();
    BaseClone(this, newob, remap);
    return(newob);
}
