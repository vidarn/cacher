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

bool SaveFunc(INode *node, MSTR filename, int start, int end)
{
    DebugPrint(L"SaveFunc begin!\n");
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
                FILE *fp = _wfopen(filename_buffer, L"wb");
                free(filename_buffer);
                SysFreeString(in_filename);
                if(fp){
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
                        /*fwrite(f.v,       sizeof(DWORD),3,fp);
                        fwrite(&f.smGroup,sizeof(DWORD),1,fp);
                        fwrite(&f.flags,  sizeof(DWORD),1,fp);*/
                    }
                    if(num_tverts > 0){
                        float *tvert_source = (float*)(face_source + num_faces*5);
                        for(int i=0;i<num_tverts;i++){
                            Point3 p = mesh.tVerts[i];
                            tvert_source[i*3 + 0] = p.x;
                            tvert_source[i*3 + 1] = p.y;
                            tvert_source[i*3 + 2] = p.z;
                            /*fwrite(&p.x,sizeof(float),1,fp);
                            fwrite(&p.y,sizeof(float),1,fp);
                            fwrite(&p.z,sizeof(float),1,fp);*/
                        }
                        DWORD *tvface_source = (DWORD*)(tvert_source + num_tverts*3);
                        for(int i=0;i<num_faces;i++){
                            TVFace f = mesh.tvFace[i];
                            tvface_source[i*3 + 0] = f.t[0];
                            tvface_source[i*3 + 1] = f.t[1];
                            tvface_source[i*3 + 2] = f.t[2];
                            //fwrite(f.t,sizeof(DWORD),3,fp);
                        }
                    }
                    char *compressed = (char *)malloc(LZ4_compressBound(source_size));
                    int compressed_size = LZ4_compress((char*)source,compressed,source_size);
                    free(source);

                    fwrite(&file_format_version,sizeof(int),1,fp);
                    fwrite(&num_verts, sizeof(int),1,fp);
                    fwrite(&num_faces, sizeof(int),1,fp);
                    fwrite(&num_tverts,sizeof(int),1,fp);
                    fwrite(&compressed_size,sizeof(int),1,fp);
                    fwrite(&source_size,sizeof(int),1,fp);
                    fwrite(compressed,compressed_size,1,fp);
                    fclose(fp);

                    free(compressed);
                }
                if(obj != tri){
                    tri->DeleteMe();
                }
            }
        }
    }
    return true;
}

void LoadFunc(Mesh *mesh, TimeValue t, Interval *ivalid, IParamBlock2 *pblock)
{

    FILE *fp = 0;
    int frame = t/GetTicksPerFrame();

    MSTR str = pblock->GetStr(pb_filename,t);
    wchar_t *in_filename = str.ToBSTR();
    if(in_filename){
        size_t len = wcslen(in_filename);
        if(len > 10){
            in_filename[len-9] = 0;

            MSTR status = MSTR::FromBSTR(in_filename);
            if(pblock->SetValue(pb_status,t,status)){
                DebugPrint(L"String set!\n");
            } else {
                DebugPrint(L"String not set!\n");
            }

            size_t buffer_size = (len + wcslen(filename_template)+10);
            wchar_t *filename_buffer = (wchar_t*)malloc(buffer_size*sizeof(wchar_t));
            swprintf(filename_buffer,buffer_size,filename_template,in_filename,frame);


            fp = _wfopen(filename_buffer, L"rb");
            free(filename_buffer);
        }
        SysFreeString(in_filename);
    } else {
        //DebugPrint(L"Error reading filename\n");
    }
    if(fp){
        int version;
        int num_verts;
        int num_faces;
        int num_tverts;
        fread(&version,   sizeof(int),1,fp);
        fread(&num_verts, sizeof(int),1,fp);
        fread(&num_faces, sizeof(int),1,fp);
        fread(&num_tverts,sizeof(int),1,fp);
        mesh->setNumVerts( num_verts);
        mesh->setNumFaces( num_faces);
        if(num_tverts > 0){
            mesh->setMapSupport(1);
            mesh->setNumTVerts(num_tverts);
        }
        DebugPrint(L"Version: %d\nNum TVerts: %d\n", version, num_tverts);

        int compressed_size, source_size;
        fread(&compressed_size,sizeof(int),1,fp);
        fread(&source_size,sizeof(int),1,fp);
        char *compressed_data = (char *)malloc(compressed_size);
        fread(compressed_data,compressed_size,1,fp); //Buffer overrun here, whyyyy?
        char *data = (char *)malloc(source_size);
        LZ4_decompress_fast(compressed_data, data, source_size);
        free(compressed_data);

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
            /*fread(f.v,       sizeof(DWORD),3,fp);
            fread(&f.smGroup,sizeof(DWORD),1,fp);
            fread(&f.flags,  sizeof(DWORD),1,fp);*/
            //TODO(Vidar) Write to face directly
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
                /*fread(&p.x,sizeof(float),1,fp);
                fread(&p.y,sizeof(float),1,fp);
                fread(&p.z,sizeof(float),1,fp);*/
                p.x = tvert_data[i*3 + 0];
                p.y = tvert_data[i*3 + 1];
                p.z = tvert_data[i*3 + 2];
                mesh->tVerts[i] = p;
            }
            DWORD *tvface_data = (DWORD*)(tvert_data + num_tverts*3);
            for(int i=0;i<num_faces;i++){
                TVFace f;
                //fread(f.t,sizeof(DWORD),3,fp);
                f.t[0] = tvface_data[i*3 + 0];
                f.t[1] = tvface_data[i*3 + 1];
                f.t[2] = tvface_data[i*3 + 2];
                mesh->tvFace[i] = f;
            }
        }
        fclose(fp);
        free(data);
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
