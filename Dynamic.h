typedef bool (*SAVEFUNC)(INode*, MSTR, int, int);
typedef void (*FREECACHEFUNC)(int, int, struct CachedFrame*);
typedef void (*CACHEFUNC)(IParamBlock2*, int*, int*, struct CachedFrame**);
typedef void (*LOADFUNC)(Mesh*, TimeValue, Interval*, struct CachedFrame*, int, int);

bool SaveFunc(INode *node, MSTR filename, int start, int end);
void FreeCache(int start, int end, CachedFrame *cached_frames);
void Cache(IParamBlock2 *pblock, int *start, int *end, struct CachedFrame **cached_frames);
void LoadFunc(Mesh *mesh, TimeValue t, Interval *ivalid, struct CachedFrame* cached_frames, int start, int end);

