
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
//#include "logo.cpp"

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

struct FileHeader {
    int version;
    int compressed_size_verts;
    int compressed_size_faces;
    int num_verts;
    int num_faces;
};

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

                // Read mesh data
                Mesh& mesh = tri->GetMesh();
                int num_verts  = mesh.getNumVerts();
                int num_faces  = mesh.getNumFaces();
                int num_tverts = 0;
                int source_size_verts = num_verts*sizeof(Point3);
                int source_size_faces = num_faces*sizeof(Face);

                // Compression buffer
                char *compressed[2];
                int   compressed_size[2];
                compressed[0]      = (char *)malloc(LZ4_compressBound(source_size_verts));
                compressed_size[0] = LZ4_compress((char*)(mesh.verts),compressed[0],source_size_verts);
                compressed[1]      = (char *)malloc(LZ4_compressBound(source_size_faces));
                compressed_size[1] = LZ4_compress((char*)(mesh.faces),compressed[1],source_size_faces);

                // File header
                FileHeader file_header;
                file_header.version = file_format_version;
                file_header.num_verts = num_verts;
                file_header.num_faces = num_faces;
                file_header.compressed_size_verts = compressed_size[0];
                file_header.compressed_size_faces = compressed_size[1];

                DWORD file_size = compressed_size[0] + compressed_size[1] + sizeof(FileHeader);

                // Write file
                HANDLE h_file = CreateFile(filename_buffer ,GENERIC_WRITE|GENERIC_READ, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if(h_file != INVALID_HANDLE_VALUE){

                    HANDLE h_file_mapping = CreateFileMapping(h_file, NULL, PAGE_READWRITE, 0, file_size, NULL);
                    void *h_view = MapViewOfFile(h_file_mapping, FILE_MAP_WRITE, 0, 0, 0);
                    char *file_char = (char *)h_view;

                    memcpy(file_char,&file_header,sizeof(FileHeader));
                    file_char += sizeof(FileHeader);
                    for(int i=0;i<2;i++){
                        memcpy(file_char, compressed[i], compressed_size[i]);
                        file_char +=  compressed_size[i];
                        free(compressed[i]);
                    }

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
inline static void print_time(wchar_t *str)
{
#if DYNAMIC
    //DebugPrint(L"%s time: %d\n", str, milliseconds_now() - time);
    time = milliseconds_now();
#endif
}
inline static void init_time()
{
#if DYNAMIC
    //DebugPrint(L"----------\n");
    time = milliseconds_now();
#endif
}

CachedFrame LoadCachedFrameFromFile( IParamBlock2 *pblock, int frame)
{
    TimeValue t = frame*GetTicksPerFrame();
    HANDLE h_file = 0;
    MSTR str = pblock->GetStr(pb_filename,t);
    wchar_t *in_filename = str.ToBSTR();
    CachedFrame cf = {};
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

                const int buffer_size = sizeof(FileHeader);
                int char_buffer[buffer_size];

                DWORD num_bytes_read = 0;
                if(ReadFile(h_file, char_buffer, buffer_size, &num_bytes_read, NULL) == 0){
                    DWORD error = GetLastError();
                    DebugPrint(L"Error: %d\n", error);
                }
                FileHeader* header = (FileHeader*)char_buffer;

                int compressed_size_total = header->compressed_size_verts + header->compressed_size_faces;

                HANDLE h_file_mapping = CreateFileMapping(h_file, NULL, PAGE_READONLY, 0, compressed_size_total, NULL);
                void *h_view = MapViewOfFile(h_file_mapping, FILE_MAP_READ, 0, 0, 0);

                char *source_verts = (char*)h_view + sizeof(FileHeader);
                char *source_faces = source_verts + header->compressed_size_verts;

                cf.valid = true;
                cf.num_verts = header->num_verts;
                cf.num_faces = header->num_faces;
                cf.verts = (char *)malloc(header->compressed_size_verts);
                cf.faces = (char *)malloc(header->compressed_size_faces);
                memcpy(cf.verts, source_verts, header->compressed_size_verts);
                memcpy(cf.faces, source_faces, header->compressed_size_faces);

                UnmapViewOfFile(h_view);
                CloseHandle(h_file_mapping);
                CloseHandle(h_file);
            }
            free(filename_buffer);
        }
        SysFreeString(in_filename);
    }
    return cf;
}

CachedFrame LoadCachedFrameFromMemory(char *data)
{
    CachedFrame cf = {};
    FileHeader *header = (FileHeader*)data;

    char *source_verts = data + sizeof(FileHeader);
    char *source_faces = source_verts + header->compressed_size_verts;

    cf.valid = true;
    cf.num_verts = header->num_verts;
    cf.num_faces = header->num_faces;
    cf.verts = (char *)malloc(header->compressed_size_verts);
    cf.faces = (char *)malloc(header->compressed_size_faces);
    memcpy(cf.verts, source_verts, header->compressed_size_verts);
    memcpy(cf.faces, source_faces, header->compressed_size_faces);

    return cf;
}

void LoadCachedMesh(CachedFrame cf, Mesh *mesh)
{
    mesh->setNumVerts(cf.num_verts);
    mesh->setNumFaces(cf.num_faces);

    LZ4_decompress_fast(cf.verts, (char*)mesh->verts, cf.num_verts*sizeof(Point3));
    LZ4_decompress_fast(cf.faces, (char*)mesh->faces, cf.num_faces*sizeof(Face));
}

//TODO(Vidar) Handle changes in frame range

void FreeCachedFrame(CachedFrame *cf)
{
    free(cf->verts); cf->verts = NULL;
    free(cf->faces); cf->faces = NULL;
    cf->valid = false;
}

void FreeCache(CachedData* cached_data)
{
    for(int frame = cached_data->start; frame <= cached_data->end; frame++){
        FreeCachedFrame(cached_data->frames + frame);
    }
    free(cached_data->frames); cached_data->frames = NULL;
    cached_data->start = 1;
    cached_data->end   = 0;
}

void Cache(IParamBlock2 *pblock, CachedData *cached_data, HWND hWnd)
{
    Interval interval;
    pblock->GetValue(pb_start, 0, cached_data->start, interval);
    pblock->GetValue(pb_end,   0, cached_data->end,   interval);
    float inv_frame_range = 0;
    if(cached_data->start < cached_data->end){
        cached_data->frames = (CachedFrame *)malloc((cached_data->end+1)*sizeof(CachedFrame));
        inv_frame_range = 100.f/(float)(cached_data->end-cached_data->start);
    }
    for(int frame = 0; frame <= cached_data->end; frame++){
        int progress_bar_amount = (int)((float)frame*inv_frame_range);
        SendMessage(GetDlgItem(hWnd,IDC_PROGRESS),PBM_SETPOS,progress_bar_amount+1,0);
        SendMessage(GetDlgItem(hWnd,IDC_PROGRESS),PBM_SETPOS,progress_bar_amount,0);
        RedrawWindow(hWnd,NULL,NULL,RDW_ERASE);


        CachedFrame cf = {};
        if(frame >= cached_data->start){
            cf = LoadCachedFrameFromFile(pblock,frame);
        }
        cached_data->frames[frame] = cf;
    }
}

void LoadFunc(Mesh *mesh, TimeValue t, IParamBlock2 *pblock, Interval *ivalid, CachedData cached_data, HINSTANCE hInstance)
{
    init_time();
    int frame = t/GetTicksPerFrame();
    bool loaded_frame = false;
    if(frame >= cached_data.start && frame <= cached_data.end){
        CachedFrame cf = cached_data.frames[frame];
        if(cf.valid){
            LoadCachedMesh(cf,mesh);
        }
    }
    if(!loaded_frame){
        int num_verts = 0;
        int num_faces = 0;

        CachedFrame cf = LoadCachedFrameFromFile(pblock,frame);
        if(cf.valid){
            LoadCachedMesh(cf,mesh);
            FreeCachedFrame(&cf);
        } else {
            //TODO(Vidar) Obviously, we should not do this every update. Cache the logo somewhere!
            HRSRC hResInfo = FindResource(hInstance,MAKEINTRESOURCE(IDR_LOGO1),L"LOGO");
            bool loaded_logo = false;
            HGLOBAL logo = LoadResource(hInstance,hResInfo);
            if( logo != NULL){
                void * logo_data = LockResource(logo);
                CachedFrame cf = LoadCachedFrameFromMemory((char*)logo_data);
                LoadCachedMesh(cf,mesh);
                FreeCachedFrame(&cf);
                loaded_logo = true;
            }
            if(!loaded_logo){
                mesh->setNumVerts(0);
                mesh->setNumFaces(0);
                DebugPrint(L"Something went wrong when loading the logo mesh!\n");
            }
        }
    }
    ivalid->Set(t,t+GetTicksPerFrame()-1);
    print_time(L"Update mesh");
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
                    HWND btn_hWnd = GetDlgItem(hWnd,IDC_CACHERAM);
                    ICustButton *button = GetICustButton(btn_hWnd);
                    FreeCache(cached_data);
                    BOOL checked = (BOOL)button->IsChecked();
                    ReleaseICustButton(button);
                    if(checked){
                        IParamBlock2 *pblock = map->GetParamBlock();
                        Cache(pblock, cached_data, hWnd);
                    }
                    SendMessage(GetDlgItem(hWnd,IDC_PROGRESS),PBM_SETPOS,0,0);
                    RedrawWindow(hWnd,NULL,NULL,RDW_ERASE);
                    button = GetICustButton(btn_hWnd);
                    button->SetCheck(checked);
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

