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
        Mesh mesh { "head_male.obj", Options::LogGroups };
        AssertThat(mesh.NumGroups(), 1);
        (void)mesh.SaveAs("head_male.saved.obj", Options::LogGroups);
    }

    TestCase(reload_saved_obj)
    {
    }

    TestCase(load_save_fbx)
    {
        if (!Mesh::IsFBXSupported())
            return;
        Mesh mesh { "head_male.fbx", Options::LogGroups };
        AssertThat(mesh.NumGroups(), 1);
        (void)mesh.SaveAs("head_male.saved.fbx", Options::LogGroups);
    }

    TestCase(force_single_group)
    {
        Mesh mesh { "head_male.obj", Options::LogGroups|Options::SingleGroup };
        AssertThat(mesh.NumGroups(), 1);
    }

    static bool AreEqual(const rpp::Vector3& a, const rpp::Vector3& b) {
        return a.almostEqual(b);
    }
    static bool AreEqual(const rpp::Vector2& a, const rpp::Vector2& b) {
        return a.almostEqual(b);
    }
    static bool AreEqual(const Nano::Triangle& a, const Nano::Triangle& b) {
        return a == b;
    }

    template<class T>
    static bool CompareArrays(const std::vector<T>& a, 
                              const std::vector<T>& b,
                              const char* what)
    {
        if (!AssertThat(a.size(), b.size())) {
            LogWarning("%s array size did not match: %zu != %zu", 
                        what, a.size(), b.size());
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            if (!Assert(AreEqual(a[i], b[i]))) {
                LogWarning("%s array elements a[%d] != b[%d]  (%s != %s)",
                    what, i, i, to_string(a[i]), to_string(b[i])
                );
                return false;
            }
        }
        return true;
    }

    static bool AreMeshesEqual(const Mesh& a, const Mesh& b)
    {
        if (!AssertThat(a.NumGroups(), b.NumGroups()))
            return false;
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
        const Options options = Options::SingleGroup
                              | Options::LogGroups
                              | Options::SplitSeams;
        Mesh mesh{ "head_male.obj", options };

        (void)mesh.SaveAs("head_male.consistency.obj", options);
        const Mesh mesh1{ "head_male.consistency.obj", options };

        if (AreMeshesEqual(mesh, mesh1))
            LogInfo("Saved mesh is consistent.");
        else
            LogWarning("Saved mesh is not consistent with original mesh!");
    }

    TestCase(validate_obj_fbx_consistency)
    {
        const Options options = Options::SingleGroup
                              | Options::LogGroups
                              | Options::SplitSeams;
        Mesh mesh1 { "head_male.obj", options };
        Mesh mesh2 { "head_male.fbx", options };

        if (AreMeshesEqual(mesh1, mesh2))
            LogInfo("OBJ is consistent with FBX.");
        else
            LogWarning("OBJ is NOT consistent with FBX!");

    }
};
