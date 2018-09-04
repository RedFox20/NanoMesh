#pragma once
/**
 * Provides a C compatible mesh interface.
 */
#include "Mesh.h"

#ifndef NANOMESH_CAPI
#define NANOMESH_CAPI extern "C" NANOMESH_API
#endif

////////////////////////////////////////////////////////////////////////////////////

struct NANOMESH_API NanoMaterial
{
    // all publicly visible in C#
    rpp::strview Name; // name of the material instance
    rpp::strview MaterialFile; // 'default.mtl'
    rpp::strview DiffusePath;
    rpp::strview AlphaPath;
    rpp::strview SpecularPath;
    rpp::strview NormalPath;
    rpp::strview EmissivePath;
    rpp::Color3 AmbientColor  = rpp::Color3::White();
    rpp::Color3 DiffuseColor  = rpp::Color3::White();
    rpp::Color3 SpecularColor = rpp::Color3::White();
    rpp::Color3 EmissiveColor = rpp::Color3::Black();
    float Specular = 1.0f;
    float Alpha    = 1.0f;

    explicit NanoMaterial(const Nano::Material* mat)
    {
        if (!mat) return;
        const Nano::Material& m = *mat;
        Name          = m.Name;
        MaterialFile  = m.MaterialFile;
        DiffusePath   = m.DiffusePath;
        AlphaPath     = m.AlphaPath;
        SpecularPath  = m.SpecularPath;
        NormalPath    = m.NormalPath;
        EmissivePath  = m.EmissivePath;
        AmbientColor  = m.AmbientColor;
        DiffuseColor  = m.DiffuseColor;
        SpecularColor = m.SpecularColor;
        EmissiveColor = m.EmissiveColor;
        Specular      = m.Specular;
        Alpha         = m.Alpha;
    }
};

template<class T> struct NanoArrayView
{
    T*  Data = nullptr;
    int Size = 0;
    NanoArrayView() = default;
    NanoArrayView(std::vector<T>& v) : Data(v.data()), Size((int)v.size()) {}
    explicit operator bool() const { return Size > 0; }
    FINLINE T& operator[](int index) { return Data[index]; }
};

enum NanoMeshCoordSys
{
    CoordSysOpenGL, // by default we load mesh files in OpenGL coordinate system
    CoordSysUnity,  // but you can convert to Unity coordsys using NanoMeshGroup::ConvertCoords()
};


// @see Nano::Options
struct NANOMESH_API NanoOptions
{
    // If true, then all named meshgroups will be ignored
    // and all verts/faces will be put into the first object group instead
    // @note This will break multi-material support, so only use this if
    //       you have 1 or 0 materials.
    int ForceSingleGroup  = false;

    // If true, empty groups will not be discarded and will
    // be treated as metadata instead.
    // Check MeshGroup::Offset for position meta
    int CreateEmptyGroups = false;

    // Log mesh group stats during loading
    int LogMeshGroupInfo  = false;

    // split non-contiguous UV shell vertices.
    // this MAY increase vertex count, but if UV's are contiguous then vertexcount+order will remain the same
    // needed in game engines which use AOS Vertex { vec3 pos; vec3 norm; vec2 uv; };
    int SplitUVSeams      = false;

    // flatten Normals and UV's to match vertex count
    int PerVertexFlatten  = false;
};


NANOMESH_API void PrintOptions(const NanoOptions& o);


/**
 * Triangulated mesh
 */
struct NANOMESH_API NanoMeshGroup
{
    // publicly visible in C#
    int GroupId = -1;
    rpp::strview Name;
    NanoMaterial Mat;
    NanoArrayView<rpp::Vector3> Vertices;
    NanoArrayView<rpp::Vector3> Normals;
    NanoArrayView<rpp::Vector2> Coords;
    NanoArrayView<int>     Indices;

    rpp::Vector3 Offset   = rpp::Vector3::Zero();
    rpp::Vector3 Rotation = rpp::Vector3::Zero(); // Euler XYZ DEGREES
    rpp::Vector3 Scale    = rpp::Vector3::One();

    NanoMeshCoordSys CoordSys = CoordSysUnity;

    // not mapped to C#
    Nano::Mesh& Owner;
    Nano::MeshGroup& Data;
    Nano::vector<int> IndexData;

    explicit NanoMeshGroup(Nano::Mesh& mesh, Nano::MeshGroup& group);
    explicit NanoMeshGroup(Nano::Mesh& mesh, int groupId);

    void InitVerts();
    void ConvertCoords(NanoMeshCoordSys coordSys);
};

struct NANOMESH_API NanoMesh
{
    // publicly visible in C#
    rpp::strview Name  = "";
    int NumGroups = 0;
    int NumVerts  = 0;
    int NumTris   = 0;

    // not mapped to C#
    Nano::Mesh Data;
    std::vector<std::unique_ptr<NanoMeshGroup>> Groups;

    NanoMesh();
    explicit NanoMesh(rpp::strview path, const NanoOptions& opt);
    NanoMeshGroup* GetGroup(int groupId);
    NanoMeshGroup* AddGroup(std::string groupname);
};

////////////////////////////////////////////////////////////////////////////////////

NANOMESH_CAPI const char*    NanoGetLastError(void);
NANOMESH_CAPI NanoMesh*      NanoMeshOpen(const char* filename, NanoOptions options);
NANOMESH_CAPI void           NanoMeshClose(NanoMesh* mesh);
NANOMESH_CAPI NanoMeshGroup* NanoMeshGetGroup(NanoMesh* mesh, int groupId);

NANOMESH_CAPI NanoMesh*      NanoMeshCreateEmpty(const char* meshname);
NANOMESH_CAPI bool           NanoMeshSave(NanoMesh* mesh, const char* filename);
NANOMESH_CAPI NanoMeshGroup* NanoMeshNewGroup(NanoMesh* mesh, const char* groupname);

NANOMESH_CAPI void NanoMeshGroupSetMaterial(
                NanoMeshGroup* group,
                const char* name,
                const char* materialFile,
                const char* diffusePath,
                const char* alphaPath,
                const char* specularPath,
                const char* normalPath,
                const char* emissivePath,
                rpp::Color3 ambientColor,
                rpp::Color3 diffuseColor,
                rpp::Color3 specularColor,
                rpp::Color3 emissiveColor,
                float specular,
                float alpha);

////////////////////////////////////////////////////////////////////////////////////
