typedef void (*LOADFUNC)(Mesh*, TimeValue, Interval*, IParamBlock2 *pblock);
typedef bool (*SAVEFUNC)(INode*, MSTR, int, int);

bool SaveFunc(INode *node, MSTR filename, int start, int end);
void LoadFunc(Mesh *mesh, TimeValue t, Interval *ivalid, IParamBlock2 *pblock);

