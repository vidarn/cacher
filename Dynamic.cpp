// Dynamic.cpp : Defines the exported functions for the DLL application.
//

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#include <istdplug.h>
#include <iparamb2.h>
#include <iparamm2.h>
#include <triobj.h>
#include "params.h"

#include "lib/lz4.h"

static const wchar_t *filename_template = L"%s_%03d.mesh";
static const int file_format_version = 0;

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
                char *compressed = (char *)malloc(LZ4_compressBound(source_size));
                int compressed_size = LZ4_compress((char*)source,compressed,source_size);
                free(source);

                DWORD file_size = compressed_size + sizeof(int)*6;

                HANDLE h_file = CreateFile(filename_buffer ,GENERIC_WRITE|GENERIC_READ, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if(h_file != INVALID_HANDLE_VALUE){

                    HANDLE h_file_mapping = CreateFileMapping(h_file, NULL, PAGE_READWRITE, 0, file_size, NULL);

                    void *h_view = MapViewOfFile(h_file_mapping, FILE_MAP_WRITE, 0, 0, 0);
                    int *file_int = (int*)h_view;
                    file_int[0] = file_format_version;
                    file_int[1] = num_verts;
                    file_int[2] = num_faces;
                    file_int[3] = num_tverts;
                    file_int[4] = compressed_size;
                    file_int[5] = source_size;
                    char *file_char = (char *)(file_int + 6);
                    memcpy(file_char, compressed, compressed_size);

                    DWORD error = GetLastError();

                    UnmapViewOfFile(h_view);
                    CloseHandle(h_file_mapping);
                    CloseHandle(h_file);
                }

                free(compressed);

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
    DebugPrint(L"%s time: %d\n", str, milliseconds_now() - time);
    time = milliseconds_now();
}
static void init_time()
{
    DebugPrint(L"----------\n");
    time = milliseconds_now();
}

void LoadFunc(Mesh *mesh, TimeValue t, Interval *ivalid, IParamBlock2 *pblock)
{

    init_time();

    void *h_view = 0;
    int frame = t/GetTicksPerFrame();

    int version = 0;
    int num_verts = 0;
    int num_faces = 0;
    int num_tverts = 0;
    int compressed_size = 0;
    int source_size = 0;
    HANDLE h_file_mapping = 0;
    HANDLE h_file = 0;

    print_time(L"Init");
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
            print_time(L"Create filename");

            h_file = CreateFile(filename_buffer ,GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            print_time(L"Open file handle");
            if(h_file != INVALID_HANDLE_VALUE){

                //TODO(Vidar) Read basic info

                int buffer_size = sizeof(int)*6;
                char buffer[sizeof(int)*6];

                ReadFile(h_file, buffer, buffer_size, NULL, NULL);
                int* int_buffer = (int*)buffer;

                version         = int_buffer[0];
                num_verts       = int_buffer[1];
                num_faces       = int_buffer[2];
                num_tverts      = int_buffer[3];
                compressed_size = int_buffer[4];
                source_size     = int_buffer[5];

                print_time(L"Read basic info");

                h_file_mapping = CreateFileMapping(h_file, NULL, PAGE_READONLY, 0, compressed_size, NULL);

                h_view = MapViewOfFile(h_file_mapping, FILE_MAP_READ, 0, 0, 0);

                char *file_char = ((char*)h_view) + 6*sizeof(int);

                print_time(L"Create file mapping");

            }
            free(filename_buffer);
        }
        SysFreeString(in_filename);
    } else {
    }
    print_time(L"Load file");
    if(h_view){
        mesh->setNumVerts( num_verts);
        mesh->setNumFaces( num_faces);
        if(num_tverts > 0){
            mesh->setMapSupport(1);
            mesh->setNumTVerts(num_tverts);
        }

        char *data = (char *)malloc(source_size);
        char *source = ((char*)h_view) + 6*sizeof(int);
        print_time(L"Set verts");
        LZ4_decompress_fast(source, data, source_size);
        print_time(L"Decompress");

        UnmapViewOfFile(h_view);
        CloseHandle(h_file_mapping);
        CloseHandle(h_file);
        print_time(L"Close file");


        //TODO(Vidar) Much prettier to increase pointer as we go

        float* vert_data = (float *)data;
        for(int i=0;i<num_verts;i++){
            Point3 p;
            p.x = vert_data[i*3 + 0];
            p.y = vert_data[i*3 + 1];
            p.z = vert_data[i*3 + 2];
            mesh->setVert(i,p); //TODO(Vidar) Surely there's a faster way of doing this?
        }
        DWORD *face_data = (DWORD*)(vert_data + num_verts*3);
        for(int i=0;i<num_faces;i++){
            Face f;
            f.v[0]    = face_data[i*5 + 0];
            f.v[1]    = face_data[i*5 + 1];
            f.v[2]    = face_data[i*5 + 2];
            f.smGroup = face_data[i*5 + 3];
            f.flags   = face_data[i*5 + 4];
            mesh->faces[i] = f;
        }
        if(num_tverts > 0){
            float *tvert_data = (float*)(face_data + num_faces*5);
            for(int i=0;i<num_tverts;i++){
                Point3 p;
                p.x = tvert_data[i*3 + 0];
                p.y = tvert_data[i*3 + 1];
                p.z = tvert_data[i*3 + 2];
                mesh->tVerts[i] = p;
            }
            DWORD *tvface_data = (DWORD*)(tvert_data + num_tverts*3);
            for(int i=0;i<num_faces;i++){
                TVFace f;
                f.t[0] = tvface_data[i*3 + 0];
                f.t[1] = tvface_data[i*3 + 1];
                f.t[2] = tvface_data[i*3 + 2];
                mesh->tvFace[i] = f;
            }
        }
        free(data);

        print_time(L"Create mesh");
    } else {
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
    //TODO(Vidar) I'm not sure this is correct. Maybe it should be t + GetTicksPerFrame()...
	//ivalid->Set(t,t+1);
	ivalid->Set(t,t+GetTicksPerFrame()-1);
}
