#pragma once
struct CachedFrame{
    bool valid;
    int num_verts;
    int num_faces;
    char* verts; //Compressed
    char* faces; //Compressed
};

struct CachedData{
    int start,end;
    CachedFrame *frames;
};
