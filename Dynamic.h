typedef bool    (*SAVEFUNC)(INode*, MSTR, int, int);
typedef INT_PTR (*DLGPROCFUNC)(TimeValue, IParamMap2 *, HWND, UINT, WPARAM, LPARAM, struct CachedData*);
typedef void    (*FREECACHEFUNC)(CachedData*);
typedef void    (*CACHEFUNC)(IParamBlock2*, CachedData *);
typedef void    (*LOADFUNC)(Mesh*, TimeValue, IParamBlock2 *, Interval*, struct CachedData);

bool    SaveFunc(INode *node, MSTR filename, int start, int end);
INT_PTR DlgFunc(TimeValue t, IParamMap2 *  map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, struct CachedData* cached_data);
void    FreeCache(CachedData* cached_data);
void    Cache(IParamBlock2 *pblock, CachedData *cached_data);
void    LoadFunc(Mesh *mesh, TimeValue t, IParamBlock2 *pblock, Interval *ivalid, struct CachedData cached_data);

