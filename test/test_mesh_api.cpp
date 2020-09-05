#include <rpp/debugging.h>
#include <rpp/tests.h>
#include <Nano/Mesh.h>
using Nano::Mesh;
using Nano::Options;
using Nano::MeshGroup;

TestImpl(test_mesh_api)
{
    TestInit(test_mesh_api)
    {
    }

    TestCase(load_and_save_obj)
    {
        Mesh mesh { "head_male.obj", Options::Log };
        AssertThat(mesh.NumGroups(), 1);
        (void)mesh.SaveAs("head_male.saved.obj", Options::Log);
    }

    TestCase(reload_saved_obj)
    {
    }

    TestCase(load_save_fbx)
    {
        if (!Mesh::IsFBXSupported())
            return;
        Mesh mesh { "head_male.fbx", Options::Log };
        AssertThat(mesh.NumGroups(), 1);
        (void)mesh.SaveAs("head_male.saved.fbx", Options::Log);
    }

    TestCase(force_single_group)
    {
        Mesh mesh { "head_male.obj", Options::SingleGroup | Options::Log };
        AssertThat(mesh.NumGroups(), 1);
    }

    bool AreEqual(const rpp::Vector3& a, const rpp::Vector3& b) {
        return a.almostEqual(b);
    }
    bool AreEqual(const rpp::Vector2& a, const rpp::Vector2& b) {
        return a.almostEqual(b);
    }
    bool AreEqual(const Nano::Triangle& a, const Nano::Triangle& b) {
        return a == b;
    }

    template<class T>
    bool CompareArrays(const std::vector<T>& a,
                       const std::vector<T>& b, const char* what)
    {
        if (a.size() != b.size())
        {
            AssertEqual(a.size(), b.size());
            LogWarning("%s array size did not match: %zu != %zu", 
                        what, a.size(), b.size());
            return false;
        }

        for (int i = 0; i < static_cast<int>(a.size()); ++i)
        {
            if (AreEqual(a[i], b[i]))
            {
                Assert( AreEqual(a[i], b[i]) );
                LogWarning("%s array elements a[%d] != b[%d]  (%s != %s)",
                    what, i, i, to_string(a[i]), to_string(b[i])
                );
                return false;
            }
        }
        return true;
    }

    bool AreMeshesEqual(const Mesh& a, const Mesh& b)
    {
        if (a.NumGroups() != b.NumGroups())
        {
            AssertEqual(a.NumGroups(), b.NumGroups());
            return false;
        }

        for (int i = 0; i < a.NumGroups(); ++i)
        {
            const MeshGroup& ga = a[i];
            const MeshGroup& gb = b[i];
            if (!CompareArrays(ga.Verts,   gb.Verts,   "Vertex"))   return false;
            if (!CompareArrays(ga.Tris,    gb.Tris,    "Triangle")) return false;
            if (!CompareArrays(ga.Coords,  gb.Coords,  "UV"))       return false;
            if (!CompareArrays(ga.Normals, gb.Normals, "Normals"))  return false;
        }
        return true;
    }

    TestCase(validate_load_save_consistency)
    {
        Options options = Options::SingleGroup
                        | Options::SplitSeams
                        | Options::Log;
        Mesh mesh{ "head_male.obj", options };

        (void)mesh.SaveAs("head_male.consistency.obj", options);
        const Mesh mesh1{ "head_male.consistency.obj", options };

        if (AreMeshesEqual(mesh, mesh1))
            LogInfo("Saved mesh is consistent.");
        else
            LogWarning("Saved mesh is not consistent with original mesh!");
    }

    bool AreVerticesEqual(const Mesh& a, const Mesh& b)
    {
        if (a.NumGroups() != b.NumGroups())
        {
            AssertEqual(a.NumGroups(), b.NumGroups());
            return false;
        }

        for (int i = 0; i < a.NumGroups(); ++i)
        {
            const MeshGroup& ga = a[i];
            const MeshGroup& gb = b[i];
            if (!CompareArrays(ga.Verts, gb.Verts, "Vertex"))
                return false;
        }
        return true;
    }

    TestCase(validate_obj_vertex_order)
    {
        Options options = Options::SingleGroup
                        | Options::SplitSeams
                        | Options::Flatten
                        | Options::Log;
        Mesh a { "box_4x2x1.obj", options };
        Mesh b { "box_4x2x1.txt", options };
        if (AreMeshesEqual(a, b))
            LogInfo("OBJ vertex order is correct.");
        else
            LogWarning("OBJ vertex order is INCORRECT!");
        
        a[0].PrintVerts("Box.OBJ");
        b[0].PrintVerts("Box.TXT");
    }

    TestCase(validate_options_unity)
    {
        Mesh a { "box_4x2x1.obj", Options::Log };
        Mesh b { "box_4x2x1.obj", Options::Unity | Options::Log };
        MeshGroup& ga = a[0];
        MeshGroup& gb = b[0];
        AssertThat(ga.Verts[0].x, -gb.Verts[0].x);  // GL --> Unity coordsys
        AssertThat(ga.Tris[0].b.v, gb.Tris[0].c.v); // CCW --> CW winding  0 1 2 --> 0 2 1
        AssertThat(ga.Tris[0].c.v, gb.Tris[0].b.v); // CCW --> CW winding  0 1 2 --> 0 2 1
        AssertNotEqual(ga.NumVerts(), ga.NumCoords()); // optimized mapping
        AssertThat(gb.NumVerts(), gb.NumCoords()); // per-vertex flattened mapping
    }
};
