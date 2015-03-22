#include "common.h"
#include "params.h"
#include "Cacher.h"
#include "Dynamic.h"

#define Cacher_CLASS_ID	Class_ID(0x854be7e6, 0x6a6b9f59)

#define PBLOCK_REF	0

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

	virtual int NumParamBlocks() { return 1; }					// return number of ParamBlocks in this instance
	virtual IParamBlock2* GetParamBlock(int /*i*/) { return pblock2; } // return i'th ParamBlock
	virtual IParamBlock2* GetParamBlockByID(BlockID id) { return (pblock2->ID() == id) ? pblock2 : NULL; } // return id'd ParamBlock

	void DeleteThis() { delete this; }
};



class CacherClassDesc : public ClassDesc2 
{
public:
	virtual int IsPublic() 							{ return TRUE; }
	virtual void* Create(BOOL /*loading = FALSE*/) 		{ return new Cacher(); }
	virtual const TCHAR *	ClassName() 			{ return GetString(IDS_CLASS_NAME); }
	virtual SClass_ID SuperClassID() 				{ return GEOMOBJECT_CLASS_ID; }
	virtual Class_ID ClassID() 						{ return Cacher_CLASS_ID; }
	virtual const TCHAR* Category() 				{ return GetString(IDS_CATEGORY); }

	virtual const TCHAR* InternalName() 			{ return _T("Cacher"); }	// returns fixed parsable name (scripter-visible name)
	virtual HINSTANCE HInstance() 					{ return hInstance; }					// returns owning module handle
	

};


ClassDesc2* GetCacherDesc() { 
	static CacherClassDesc CacherDesc;
	return &CacherDesc; 
}





enum { cacher_params };




static ParamBlockDesc2 cacher_param_blk ( cacher_params, _T("params"),  0, GetCacherDesc(), 
	P_AUTO_CONSTRUCT + P_AUTO_UI, PBLOCK_REF, 
	//rollout
	IDD_PANEL, IDS_PARAMS, 0, 0, NULL,
	// params
	pb_filename, 		_T("filename"),		TYPE_FILENAME, 	0, 	IDS_FILENAME, 
		p_ui, 			TYPE_FILEOPENBUTTON,		IDC_FILENAME, 
		p_end,
	pb_status, 		    _T("status"),		TYPE_STRING, 	0, 	IDS_STATUS, 
		p_ui, 			TYPE_EDITBOX,		IDC_STATUS, 
        p_enabled,      TRUE,
		p_end,
	p_end
	);




//--- Cacher -------------------------------------------------------

Cacher::Cacher()
{
	GetCacherDesc()->MakeAutoParamBlocks(this);
}

Cacher::~Cacher()
{
}

void Cacher::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
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
    LOADFUNC load_func = (LOADFUNC)FUNC(LoadFunc);
    if(load_func != NULL){
        load_func(&mesh, t, &ivalid, pblock2);
    } else {
        DWORD lastError = GetLastError();
        DebugPrint(L"Error loading function \"LoadFunc\"\nError code: %d\n",lastError);
    }
    FREEFUNCS;
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
