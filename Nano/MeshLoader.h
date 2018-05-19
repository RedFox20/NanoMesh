#pragma once
/**
 * Provides a C compatible mesh interface.
 */
#include "Mesh.h"

#ifndef MESHAPIC
#define MESHAPIC extern "C" MESHAPI
#endif

namespace Nano
{
    using std::unique_ptr;
    ////////////////////////////////////////////////////////////////////////////////////

    struct W3DMaterial
    {
        // all publicly visible in C#
        strview Name; // name of the material instance
        strview MaterialFile; // 'default.mtl'
        strview DiffusePath;
        strview AlphaPath;
        strview SpecularPath;
        strview NormalPath;
        strview EmissivePath;
        Color3 AmbientColor  = Color3::WHITE;
        Color3 DiffuseColor  = Color3::WHITE;
        Color3 SpecularColor = Color3::WHITE;
        Color3 EmissiveColor = Color3::BLACK;
        float Specular = 1.0f;
        float Alpha    = 1.0f;

        explicit W3DMaterial(const shared_ptr<Material>& mat)
        {
            if (!mat) return;
            Material& m = *mat;
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

    template<class T> struct W3DArrayView
    {
        T*  Data = nullptr;
        int Size = 0;
        W3DArrayView(){}
        W3DArrayView(vector<T>& v) : Data(v.data()), Size((int)v.size()) {}
        explicit operator bool() const { return Size > 0; }
        FINLINE T& operator[](int index) { return Data[index]; }
    };

    enum W3DMeshCoordSys
    {
        CoordSysOpenGL, // by default we load mesh files in OpenGL coordinate system
        CoordSysUnity,  // but you can convert to Unity coordsys using W3DMeshGroup::ConvertCoords()

    };

    /**
     * Triangulated mesh
     */
    struct W3DMeshGroup
    {
        // publicly visible in C#
        int     GroupId = -1;
        strview     Name;
        W3DMaterial Mat;
        W3DArrayView<Vector3> Vertices;
        W3DArrayView<Vector3> Normals;
        W3DArrayView<Vector2> Coords;
        W3DArrayView<int>     Indices;

        Vector3 Offset   = Vector3::ZERO;
        Vector3 Rotation = Vector3::ZERO; // Euler XYZ DEGREES
        Vector3 Scale    = Vector3::ONE;

        W3DMeshCoordSys CoordSys = CoordSysUnity;

        // not mapped to C#
        Mesh& Owner;
        MeshGroup& Data;
        vector<int> IndexData;

        explicit W3DMeshGroup(Mesh& mesh, MeshGroup& group);
        explicit W3DMeshGroup(Mesh& mesh, int groupId);

        void InitVerts();
        void ConvertCoords(W3DMeshCoordSys coordSys);
    };

    struct W3DMesh
    {
        // publicly visible in C#
        strview Name  = "";
        int NumGroups = 0;
        int NumFaces  = 0;

        // not mapped to C#
        Mesh Data;
        vector<unique_ptr<W3DMeshGroup>> Groups;

        W3DMesh();
        explicit W3DMesh(strview path);
        W3DMeshGroup* GetGroup(int groupId);
        W3DMeshGroup* AddGroup(string groupname);
    };

    ////////////////////////////////////////////////////////////////////////////////////

    MESHAPIC W3DMesh*     W3DMeshOpen(const char* filename);
    MESHAPIC void         W3DMeshClose(W3DMesh* mesh);
    MESHAPIC W3DMeshGroup* W3DMeshGetGroup(W3DMesh* mesh, int groupId);

    MESHAPIC W3DMesh*      W3DMeshCreateEmpty(const char* meshname);
    MESHAPIC bool          W3DMeshSave(W3DMesh* mesh, const char* filename);
    MESHAPIC W3DMeshGroup* W3DMeshNewGroup(W3DMesh* mesh, const char* groupname);

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
                    float alpha);

    ////////////////////////////////////////////////////////////////////////////////////
}

