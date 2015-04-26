// Dynamic.cpp : Defines the exported functions for the DLL application.
//

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#include <istdplug.h>
#include <iparamb2.h>
#include <iparamm2.h>
#include <triobj.h>
#include <process.h>
#include <WindowsMessageFilter.h>
#include "params.h"
#include "cached_frame.h"
#include "resource.h"
#include "Dynamic.h"

#include "lib/lz4.h"

static const wchar_t *filename_template = L"%s_%03d.mesh";
static const int file_format_version = 0;

static const int NUM_BUCKETS = 3;

long long milliseconds_now() {
    static LARGE_INTEGER s_frequency;
    static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
    if (s_use_qpc) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (1000LL * now.QuadPart) / s_frequency.QuadPart;
    } else {
        return GetTickCount();
    }
}

bool SaveFunc(INode *node, MSTR filename, int start, int end)
{
    if(start > end || node == NULL){
        //TODO(Vidar) report error back to MAXSCRIPT
        return false;
    }
    for(int frame=start;frame<=end;frame++){
        TimeValue time = frame*GetTicksPerFrame();
        Object *obj = node->EvalWorldState(time).obj;
        if (obj->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0)))
        {
            bool deleteIt = false;
            TriObject *tri = (TriObject *) obj->ConvertToType(time, Class_ID(TRIOBJ_CLASS_ID, 0));
            if(tri){
                wchar_t *in_filename = filename.ToBSTR();
                size_t buffer_size = (wcslen(in_filename) + wcslen(filename_template)+10);
                wchar_t *filename_buffer = (wchar_t*)malloc(buffer_size*sizeof(wchar_t));
                swprintf(filename_buffer,buffer_size,filename_template,in_filename,frame);


                Mesh& mesh = tri->GetMesh();
                int num_verts  = mesh.getNumVerts();
                int num_faces  = mesh.getNumFaces();
                int num_tverts = 0;
                int source_size = num_verts*3*sizeof(float) + num_faces*5*sizeof(DWORD);
                /*if(mesh.mapSupport(1)){
                    num_tverts = mesh.getNumVerts();
                    source_size += num_tverts*3*sizeof(float) + num_faces*3*sizeof(DWORD);
                }*/
                char *source = (char *)malloc(source_size);
                float *vert_source = (float *)source;
                //TODO(Vidar) convert this to memcpy
                for(int i=0;i<num_verts;i++){
                    Point3 p = mesh.verts[i];
                    vert_source[i*3+0] = p.x;
                    vert_source[i*3+1] = p.y;
                    vert_source[i*3+2] = p.z;
                }
                DWORD *face_source = (DWORD*)(vert_source + num_verts*3);
                for(int i=0;i<num_faces;i++){
                    Face f = mesh.faces[i];
                    face_source[i*5 + 0] = f.v[0];
                    face_source[i*5 + 1] = f.v[1];
                    face_source[i*5 + 2] = f.v[2];
                    face_source[i*5 + 3] = f.smGroup;
                    face_source[i*5 + 4] = f.flags;
                }
                if(num_tverts > 0){
                    float *tvert_source = (float*)(face_source + num_faces*5);
                    for(int i=0;i<num_tverts;i++){
                        Point3 p = mesh.tVerts[i];
                        tvert_source[i*3 + 0] = p.x;
                        tvert_source[i*3 + 1] = p.y;
                        tvert_source[i*3 + 2] = p.z;
                    }
                    DWORD *tvface_source = (DWORD*)(tvert_source + num_tverts*3);
                    for(int i=0;i<num_faces;i++){
                        TVFace f = mesh.tvFace[i];
                        tvface_source[i*3 + 0] = f.t[0];
                        tvface_source[i*3 + 1] = f.t[1];
                        tvface_source[i*3 + 2] = f.t[2];
                    }
                }
                char *compressed[NUM_BUCKETS];
                int compressed_size[NUM_BUCKETS];
                int bucket_size[NUM_BUCKETS];
                int compressed_size_total = 0;
                char *current_source = (char*)source;
                for(int i=0;i<NUM_BUCKETS;i++){
                    bucket_size[i] = source_size/NUM_BUCKETS;
                    if(i== NUM_BUCKETS-1){
                        bucket_size[i] += source_size%NUM_BUCKETS;
                    }
                    compressed[i] = (char *)malloc(LZ4_compressBound(bucket_size[i]));
                    compressed_size[i] = LZ4_compress(current_source,compressed[i],bucket_size[i]);
                    compressed_size_total += compressed_size[i];
                    current_source += bucket_size[i];
                }
                //char *compressed = (char *)malloc(LZ4_compressBound(source_size));
                //int compressed_size = LZ4_compress((char*)source,compressed,source_size);
                free(source);

                DWORD file_size = compressed_size_total + sizeof(int)*4 + 2*NUM_BUCKETS*sizeof(int);

                HANDLE h_file = CreateFile(filename_buffer ,GENERIC_WRITE|GENERIC_READ, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if(h_file != INVALID_HANDLE_VALUE){

                    HANDLE h_file_mapping = CreateFileMapping(h_file, NULL, PAGE_READWRITE, 0, file_size, NULL);

                    void *h_view = MapViewOfFile(h_file_mapping, FILE_MAP_WRITE, 0, 0, 0);
                    int *file_int = (int*)h_view;
                    *(file_int++) = file_format_version;
                    *(file_int++) = num_verts;
                    *(file_int++) = num_faces;
                    *(file_int++) = num_tverts;
                    for(int i=0;i<NUM_BUCKETS;i++){
                        *(file_int++) = compressed_size[i];
                        *(file_int++) = bucket_size[i];
                    }
                    char *file_char = (char *)file_int;
                    for(int i=0;i<NUM_BUCKETS;i++){
                        memcpy(file_char, compressed[i], compressed_size[i]);
                        file_char +=  compressed_size[i];
                        free(compressed[i]);
                    }

                    DWORD error = GetLastError();

                    UnmapViewOfFile(h_view);
                    CloseHandle(h_file_mapping);
                    CloseHandle(h_file);
                }


                free(filename_buffer);
                SysFreeString(in_filename);

                if(obj != tri){
                    tri->DeleteMe();
                }
            }
        }
    }
    return true;
}

static long long time;
static void print_time(wchar_t *str)
{
#if DYNAMIC
    DebugPrint(L"%s time: %d\n", str, milliseconds_now() - time);
    time = milliseconds_now();
#endif
}
static void init_time()
{
#if DYNAMIC
    DebugPrint(L"----------\n");
    time = milliseconds_now();
#endif
}



struct ThreadData {
    HANDLE h_file;
    char *data;
    int bucket_size;
    int compressed_size_total;
    int offset_source;
};

static unsigned __stdcall thread_proc(void *data)
{
    ThreadData* td = (ThreadData*)data;

    //TODO(Vidar) only map the portion of the file we're interested in
    //TODO(Vidar) Perhaps we can do the file mapping in the main thread and only map the view here
    HANDLE h_file_mapping = CreateFileMapping(td->h_file, NULL, PAGE_READONLY, 0, td->compressed_size_total, NULL);
    void *h_view = MapViewOfFile(h_file_mapping, FILE_MAP_READ, 0, 0, 0);
    char *source = ((char*)h_view) + td->offset_source;
    char *output = td->data;
    LZ4_decompress_fast(source, output, td->bucket_size);

    UnmapViewOfFile(h_view);
    CloseHandle(h_file_mapping);

    _endthreadex( 0 );
    return 0;
}

INT_PTR DlgFunc(TimeValue t, IParamMap2 *  map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, CachedData* cached_data)
{
    INT_PTR ret = FALSE;
    if(msg == WM_COMMAND){
        int control = LOWORD(wParam);
        int command = HIWORD(wParam);
        switch(control){
            case IDC_CACHERAM:
                if(command == BN_BUTTONUP){
                    HWND btn_hwnd = GetDlgItem(hWnd,IDC_CACHERAM);
                    ICustButton *button = GetICustButton(btn_hwnd);
                    FreeCache(cached_data);
                    if(button->IsChecked()){
                        IParamBlock2 *pblock = map->GetParamBlock();
                        Cache(pblock, cached_data);
                    }
                    ReleaseICustButton(button);
                }
                ret = TRUE;
                break;
            case IDC_FILENAME:
                if(command == 0){
                    //NOTE(Vidar) Update status text and free cache
                    IParamBlock2 *pblock = map->GetParamBlock();
                    MSTR str = pblock->GetStr(pb_filename,t);
                    pblock->SetValue(pb_status,t,str);
                    pblock->SetValue(pb_ram,t,FALSE);
                    FreeCache(cached_data);
                }
                ret = TRUE;
                break;
        }
    }
    return ret;
}

char * LoadFrameData(IParamBlock2 *pblock, int frame, int *num_verts, int *num_faces)
{
    TimeValue t = frame*GetTicksPerFrame();
    int version;
    int num_tverts;
    int compressed_size[NUM_BUCKETS];
    int bucket_size[NUM_BUCKETS];
    int bucket_size_total = 0;
    int compressed_size_total = 0;
    char *data = NULL;
    HANDLE h_file;
    MSTR str = pblock->GetStr(pb_filename,t);
    wchar_t *in_filename = str.ToBSTR();
    if(in_filename){
        size_t len = wcslen(in_filename);
        if(len > 10){
            in_filename[len-9] = 0;

            MSTR status = MSTR::FromBSTR(in_filename);
            pblock->SetValue(pb_status,t,status);

            size_t buffer_size = (len + wcslen(filename_template)+10);
            wchar_t *filename_buffer = (wchar_t*)malloc(buffer_size*sizeof(wchar_t));
            swprintf(filename_buffer,buffer_size,filename_template,in_filename,frame);

            SECURITY_ATTRIBUTES security_attr;
            security_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
            security_attr.lpSecurityDescriptor = NULL;
            security_attr.bInheritHandle = TRUE;

            h_file = CreateFile(filename_buffer ,GENERIC_READ, 0, &security_attr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

            if(h_file != INVALID_HANDLE_VALUE){

                const int buffer_size = sizeof(int)*(4 + 2*NUM_BUCKETS);
                int char_buffer[buffer_size];

                ReadFile(h_file, char_buffer, buffer_size, NULL, NULL);
                int* buffer = (int*)char_buffer;

                version         = *(buffer++);
                *num_verts       = *(buffer++);
                *num_faces       = *(buffer++);
                num_tverts      = *(buffer++);
                for(int i=0;i<NUM_BUCKETS;i++){
                    compressed_size[i] = *(buffer++);
                    bucket_size[i] = *(buffer++);
                    bucket_size_total += bucket_size[i];
                    compressed_size_total += compressed_size[i];
                }

                data = (char *)malloc(bucket_size_total);

                ThreadData thread_data[NUM_BUCKETS];
                HANDLE h_threads[NUM_BUCKETS];
                int offset_source = sizeof(int)*(4 + 2*NUM_BUCKETS);
                int offset_data =0 ;
                for(int i=0;i<NUM_BUCKETS;i++){
                    thread_data[i].h_file = h_file;
                    thread_data[i].data = data + offset_data;;
                    thread_data[i].bucket_size = bucket_size[i];
                    thread_data[i].compressed_size_total = compressed_size_total;
                    thread_data[i].offset_source = offset_source;

                    h_threads[i] = (HANDLE)_beginthreadex(NULL, 0 ,&thread_proc,(void*)(thread_data+i),  0, NULL);
                    offset_source += compressed_size[i];
                    offset_data   += bucket_size[i];
                }

                for(int i=0;i<NUM_BUCKETS;i++){
                    WaitForSingleObject(h_threads[i], INFINITE );
                }


                CloseHandle(h_file);
            }
            free(filename_buffer);
        }
        SysFreeString(in_filename);
    }
    return data;
}

//TODO(Vidar) Handle changes in frame range

void FreeCache(CachedData* cached_data)
{
    for(int frame = cached_data->start; frame <= cached_data->end; frame++){
        CachedFrame *cf = cached_data->frames + frame;
        free(cf->verts); cf->verts = NULL;
        free(cf->faces); cf->faces = NULL;
        cf->valid = false;
    }
    free(cached_data->frames); cached_data->frames = NULL;
    cached_data->start = 1;
    cached_data->end   = 0;
}

void Cache(IParamBlock2 *pblock, CachedData *cached_data)
{
    void *h_view = 0;
    Interval interval;
    pblock->GetValue(pb_start, 0, cached_data->start, interval);
    pblock->GetValue(pb_end,   0, cached_data->end,   interval);
    if(cached_data->start < cached_data->end){
        cached_data->frames = (CachedFrame *)malloc((cached_data->end+1)*sizeof(CachedFrame));
    }
    for(int frame = 0; frame <= cached_data->end; frame++){
        CachedFrame cf = {};
        int num_verts  = 0;
        int num_faces  = 0;

        if(frame >= cached_data->start){
            char* data = LoadFrameData(pblock, frame, &num_verts, &num_faces);
            if(data){

                cf.verts = (Point3*)malloc(sizeof(Point3)*num_verts);
                cf.faces = (Face*  )malloc(sizeof(Face  )*num_faces);
                cf.num_verts = num_verts;
                cf.num_faces = num_faces;

                char *curr_data = data;
                memcpy((void*)cf.verts,(void*)curr_data,num_verts*sizeof(Point3));

                curr_data += num_verts*3*sizeof(float);
                memcpy((void*)cf.faces,(void*)curr_data,num_faces*sizeof(Face));

                free(data);
                
                cf.valid = true;
            }
        }

        cached_data->frames[frame] = cf;
        /* TODO(Vidar) We should handle messages to keep 3ds max from being unresponsive
        MaxSDK::WindowsMessageFilter messageFilter;
        messageFilter.RunNonBlockingMessageLoop();
        if(messageFilter.Aborted()){
            DebugPrint(L"Abort! Abort!\n");
        }*/
    }
}

void LoadFunc(Mesh *mesh, TimeValue t, IParamBlock2 *pblock, Interval *ivalid, CachedData cached_data)
{
    init_time();
    int frame = t/GetTicksPerFrame();
    bool loaded_frame = false;
    if(frame >= cached_data.start && frame <= cached_data.end){
        CachedFrame cf = cached_data.frames[frame];
        if(cf.valid){
            mesh->setNumVerts(cf.num_verts);
            mesh->setNumFaces(cf.num_faces);
            //TODO(Vidar) What if we just change the pointer? Kind of dangerous, but perhaps it could work
            // however, what happens when 3ds max decides to release the memory..?
            // On second thought, I don't think we need to worry about it, it seems fast enough allready
            memcpy((void*)mesh->verts,(void*)cf.verts,cf.num_verts*sizeof(Point3));
            memcpy((void*)mesh->faces,(void*)cf.faces,cf.num_faces*sizeof(Face  ));
            loaded_frame = true;
        }
    }
    if(!loaded_frame){
        int num_verts = 0;
        int num_faces = 0;

        char* data = LoadFrameData(pblock, frame, &num_verts, &num_faces);
        if(data){
            mesh->setNumVerts(num_verts);
            mesh->setNumFaces(num_faces);

            char *curr_data = data;
            memcpy((void*)mesh->verts,(void*)curr_data,num_verts*sizeof(Point3));
            curr_data += num_verts*sizeof(Point3);
            memcpy((void*)mesh->faces,(void*)curr_data,num_faces*sizeof(Face  ));

            free(data);
        } else {
            //TODO(Vidar) Draw a prettier default icon
            float s = 10.0f;
            mesh->setNumVerts(4);
            mesh->setNumFaces(3);
            mesh->setVert(0,s*Point3(0.0,0.0,0.0)); 
            mesh->setVert(1,s*Point3(10.0,0.0,0.0)); 
            mesh->setVert(2,s*Point3(0.0,10.0,0.0)); 
            mesh->setVert(3,s*Point3(0.0,0.0,10.0)); 

            mesh->faces[0].setVerts(0, 1, 2);
            mesh->faces[0].setEdgeVisFlags(1,1,0);
            mesh->faces[0].setSmGroup(2);
            mesh->faces[1].setVerts(3, 1, 0);
            mesh->faces[1].setEdgeVisFlags(1,1,0);
            mesh->faces[1].setSmGroup(2);
            mesh->faces[2].setVerts(0, 2, 3);
            mesh->faces[2].setEdgeVisFlags(1,1,0);
            mesh->faces[2].setSmGroup(4);
        }
    }
    ivalid->Set(t,t+GetTicksPerFrame()-1);
    print_time(L"Update mesh");
}
