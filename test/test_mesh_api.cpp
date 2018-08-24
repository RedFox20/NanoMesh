#include <rpp/tests.h>
#include <Nano/Mesh.h>

TestImpl(test_mesh_api)
{
    TestInit(test_mesh_api)
    {
    }

    TestCase(load_and_save_obj)
    {
        Nano::Mesh mesh { "head_male.obj", Nano::Options::LogGroups };
        AssertThat(mesh.NumGroups(), 7);
        mesh.SaveAs("head_male.saved.obj", Nano::Options::LogGroups);
    }

    TestCase(reload_saved_obj)
    {
    }

    TestCase(load_save_fbx)
    {
        if (!Nano::Mesh::IsFBXSupported())
            return;
        Nano::Mesh mesh { "head_male.fbx", Nano::Options::LogGroups };
        AssertThat(mesh.NumGroups(), 1);
        mesh.SaveAs("head_male.saved.fbx", Nano::Options::LogGroups);
    }

    TestCase(force_single_group)
    {
        Nano::Mesh mesh { "head_male.obj", Nano::Options::LogGroups|Nano::Options::SingleGroup };
        AssertThat(mesh.NumGroups(), 1);
    }
};
