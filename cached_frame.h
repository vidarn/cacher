#pragma once
struct CachedFrame{
    bool valid;
    int num_verts;
    int num_faces;
    Point3* verts;
    Face* faces;
};

struct CachedData{
    int start,end;
    CachedFrame *frames;
};
