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

static const wchar_t *filename_template = L"%s_%03d.mesh";

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
                FILE *fp = _wfopen(filename_buffer, L"wb");
                free(filename_buffer);
                SysFreeString(in_filename);
                if(fp){
                    Mesh& mesh = tri->GetMesh();
                    int num_verts = mesh.getNumVerts();
                    int num_faces = mesh.getNumFaces();
                    fwrite(&num_verts,sizeof(int),1,fp);
                    fwrite(&num_faces,sizeof(int),1,fp);
                    for(int i=0;i<num_verts;i++){
                        Point3 p = mesh.verts[i];
                        fwrite(&p.x,sizeof(float),1,fp);
                        fwrite(&p.y,sizeof(float),1,fp);
                        fwrite(&p.z,sizeof(float),1,fp);
                    }
                    for(int i=0;i<num_faces;i++){
                        Face f = mesh.faces[i];
                        fwrite(f.v,       sizeof(DWORD),3,fp);
                        fwrite(&f.smGroup,sizeof(DWORD),1,fp);
                        fwrite(&f.flags,  sizeof(DWORD),1,fp);
                    }
                    fclose(fp);
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
        int num_verts;
        int num_faces;
        fread(&num_verts,sizeof(int),1,fp);
        fread(&num_faces,sizeof(int),1,fp);
        mesh->setNumVerts(num_verts);
        mesh->setNumFaces(num_faces);

        for(int i=0;i<num_verts;i++){
            Point3 p;
            fread(&p.x,sizeof(float),1,fp);
            fread(&p.y,sizeof(float),1,fp);
            fread(&p.z,sizeof(float),1,fp);
            mesh->setVert(i,p);
        }
        for(int i=0;i<num_faces;i++){
            Face f;
            fread(f.v,       sizeof(DWORD),3,fp);
            fread(&f.smGroup,sizeof(DWORD),1,fp);
            fread(&f.flags,  sizeof(DWORD),1,fp);
            mesh->faces[i] = f;
        }
        fclose(fp);
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
