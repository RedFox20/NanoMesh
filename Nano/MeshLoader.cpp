#include "MeshLoader.h"
#include <rpp/file_io.h>

namespace Nano
{
    using std::make_unique;
    ////////////////////////////////////////////////////////////////////////////////////

    W3DMeshGroup::W3DMeshGroup(Mesh& mesh, MeshGroup& group)
        : GroupId(group.GroupId), Mat{ group.Mat }, Owner{ mesh }, Data{ group }
    {
        Name = Data.Name;
    }

    W3DMeshGroup::W3DMeshGroup(Mesh& mesh, int groupId)
        : GroupId(groupId), Mat{ mesh[groupId].Mat }, Owner{ mesh }, Data{ mesh[groupId] }
    {
        Name = Data.Name;
        InitVerts();
    }

    void W3DMeshGroup::InitVerts()
    {
        if (Data.IsEmpty())
            return;

        // split seam vertices to enable non-contiguous UV shells
        Data.OptimizedFlatten();
        Data.CreateIndexArray(IndexData);
        Vertices = Data.Verts;
        Normals  = Data.Normals;
        Coords   = Data.Coords;
        Indices  = IndexData;

        Offset   = Data.Offset;
        Rotation = Data.Rotation;
        Scale    = Data.Scale;

        if (Vertices.Size == 0 || Indices.Size == 0) {
            fprintf(stderr, "WARNING: No mesh data for group %d\n", GroupId);
            return;
        }
    }

    void W3DMeshGroup::ConvertCoords(W3DMeshCoordSys coordSys)
    {
        if (CoordSys != coordSys)
        {
            // @todo Implement this
            CoordSys = coordSys;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////

    W3DMesh::W3DMesh()
    {
    }

    W3DMesh::W3DMesh(strview path) : Data{ path }
    {
        Groups.resize(Data.NumGroups());
        Name      = Data.Name;
        NumGroups = Data.NumGroups();
        NumFaces  = Data.NumFaces;

        //string copy = path_combine(folder_path(path), file_name(path) + "_validate.obj");
        //Data.SaveAsOBJ(copy);
    }

    W3DMeshGroup* W3DMesh::GetGroup(int groupId)
    {
        if (!Data.IsValidGroup(groupId))
            return nullptr;

        if (auto* groupMesh = Groups[groupId].get())
            return groupMesh;

        Groups[groupId] = make_unique<W3DMeshGroup>(Data, groupId);
        return Groups[groupId].get();
    }

    W3DMeshGroup* W3DMesh::AddGroup(string groupname)
    {
        MeshGroup& group = Data.CreateGroup(groupname);
        Groups.emplace_back(make_unique<W3DMeshGroup>(Data, group));
        return Groups.back().get();
    }

    ////////////////////////////////////////////////////////////////////////////////////

    MESHAPIC W3DMesh* W3DMeshOpen(const char* filename)
    {
        auto sdm = new W3DMesh{ filename };
        if (!sdm->Data)
        {
            W3DMeshClose(sdm);
            return nullptr;
        }
        return sdm;
    }

    MESHAPIC void W3DMeshClose(W3DMesh* mesh)
    {
        delete mesh;
    }

    MESHAPIC W3DMeshGroup* W3DMeshGetGroup(W3DMesh* mesh, int groupId)
    {
        return mesh ? mesh->GetGroup(groupId) : nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////////

    MESHAPIC W3DMesh* W3DMeshCreateEmpty(const char* meshname)
    {
        W3DMesh* mesh   = new W3DMesh{};
        mesh->Data.Name = meshname;
        mesh->Name      = mesh->Data.Name;
        return mesh;
    }

    MESHAPIC bool W3DMeshSave(W3DMesh* mesh, const char* filename)
    {
        return mesh->Data.SaveAs(filename);
    }

    MESHAPIC W3DMeshGroup* W3DMeshNewGroup(W3DMesh* mesh, const char* groupname)
    {
        return mesh->AddGroup(groupname);
    }

    MESHAPIC void W3DMeshGroupSetMaterial(
                    W3DMeshGroup* group, 
                    const char* name,
                    const char* materialFile,
                    const char* diffusePath,
                    const char* alphaPath,
                    const char* specularPath,
                    const char* normalPath,
                    const char* emissivePath,
                    Color3 ambientColor, 
                    Color3 diffuseColor, 
                    Color3 specularColor, 
                    Color3 emissiveColor, 
                    float specular, 
                    float alpha)
    {
        Material& mat = group->Data.CreateMaterial(name);
        mat.MaterialFile  = materialFile;
        mat.DiffusePath   = diffusePath;
        mat.AlphaPath     = alphaPath;
        mat.SpecularPath  = specularPath;
        mat.NormalPath    = normalPath;
        mat.EmissivePath  = emissivePath;
        mat.AmbientColor  = ambientColor;
        mat.DiffuseColor  = diffuseColor;
        mat.SpecularColor = specularColor;
        mat.EmissiveColor = emissiveColor;
        mat.Specular      = specular;
        mat.Alpha         = alpha;
    }

    ////////////////////////////////////////////////////////////////////////////////////
}
