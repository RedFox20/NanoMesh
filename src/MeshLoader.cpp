#include <Nano/MeshLoader.h>
#include <rpp/file_io.h>
#include <rpp/debugging.h>

using std::make_unique;
using namespace Nano;
////////////////////////////////////////////////////////////////////////////////////

NanoMeshGroup::NanoMeshGroup(Mesh& mesh, MeshGroup& group)
    : GroupId(group.GroupId), Mat{ group.Mat.get() }, Owner{ mesh }, Data{ group }
{
    Name = Data.Name;
}

NanoMeshGroup::NanoMeshGroup(Mesh& mesh, int groupId)
    : GroupId(groupId), Mat{ mesh[groupId].Mat.get() }, Owner{ mesh }, Data{ mesh[groupId] }
{
    Name = Data.Name;
    InitVerts();
}

void NanoMeshGroup::InitVerts()
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
        LogWarning("WARNING: No mesh data for group %d\n", GroupId);
        return;
    }
}

void NanoMeshGroup::ConvertCoords(NanoMeshCoordSys coordSys)
{
    if (CoordSys != coordSys)
    {
        // @todo Implement this
        CoordSys = coordSys;
    }
}

////////////////////////////////////////////////////////////////////////////////////

NanoMesh::NanoMesh() = default;

NanoMesh::NanoMesh(strview path) : Data{ path }
{
    Groups.resize(Data.NumGroups());
    Name      = Data.Name;
    NumGroups = Data.NumGroups();
    NumTris   = Data.TotalTris();

    //string copy = path_combine(folder_path(path), file_name(path) + "_validate.obj");
    //Data.SaveAsOBJ(copy);
}

NanoMeshGroup* NanoMesh::GetGroup(int groupId)
{
    if (!Data.IsValidGroup(groupId))
        return nullptr;

    if (auto* groupMesh = Groups[groupId].get())
        return groupMesh;

    Groups[groupId] = make_unique<NanoMeshGroup>(Data, groupId);
    return Groups[groupId].get();
}

NanoMeshGroup* NanoMesh::AddGroup(string groupname)
{
    MeshGroup& group = Data.CreateGroup(groupname);
    Groups.emplace_back(make_unique<NanoMeshGroup>(Data, group));
    return Groups.back().get();
}

////////////////////////////////////////////////////////////////////////////////////

NANOMESH_CAPI NanoMesh* NanoMeshOpen(const char* filename)
{
    auto mesh = new NanoMesh{ filename };
    if (!mesh->Data)
    {
        NanoMeshClose(mesh);
        return nullptr;
    }
    return mesh;
}

NANOMESH_CAPI void NanoMeshClose(NanoMesh* mesh)
{
    delete mesh;
}

NANOMESH_CAPI NanoMeshGroup* NanoMeshGetGroup(NanoMesh* mesh, int groupId)
{
    return mesh ? mesh->GetGroup(groupId) : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////

NANOMESH_CAPI NanoMesh* NanoMeshCreateEmpty(const char* meshname)
{
    NanoMesh* mesh  = new NanoMesh{};
    mesh->Data.Name = meshname;
    mesh->Name      = mesh->Data.Name;
    return mesh;
}

NANOMESH_CAPI bool NanoMeshSave(NanoMesh* mesh, const char* filename)
{
    return mesh->Data.SaveAs(filename);
}

NANOMESH_CAPI NanoMeshGroup* NanoMeshNewGroup(NanoMesh* mesh, const char* groupname)
{
    return mesh->AddGroup(groupname);
}

NANOMESH_CAPI void NanoMeshGroupSetMaterial(
                NanoMeshGroup* group, 
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
    group->Mat = NanoMaterial{ &mat };
}

////////////////////////////////////////////////////////////////////////////////////
