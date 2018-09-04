#include <rpp/debugging.h>
#include <rpp/tests.h>
#include <Nano/MeshLoader.h>

TestImpl(test_mesh_loader_api)
{
    TestInit(test_mesh_loader_api)
    {
    }

    TestCase(basic_load_save)
    {
        Nano::Mesh mesh { "head_male.obj", 
            Nano::Options::LogGroups | Nano::Options::SingleGroup  };
        AssertThat(mesh.NumGroups(), 1);


        NanoOptions opt;
        opt.ForceSingleGroup = true;
        opt.SplitUVSeams     = true;
        opt.PerVertexFlatten = true;
        opt.LogMeshGroupInfo = true;
        PrintOptions(opt);

        NanoMesh* nanoMesh = NanoMeshOpen("head_male.obj", opt);
        AssertNotEqual(nanoMesh, nullptr);
        if (!nanoMesh) return;
        AssertThat(nanoMesh->NumGroups, 1); // ForceSingleGroup
        AssertThat(nanoMesh->NumTris, mesh.TotalTris());

        NanoMeshGroup* g = NanoMeshGetGroup(nanoMesh, 0);
        AssertNotEqual(g, nullptr);
        if (!g) return;

        LogInfo("NanoMeshGroup 0: %s", g->Name);
        LogInfo("   %d verts   %d normals   %d uvs   %d indices", 
            g->Vertices.Size,
            g->Normals.Size,
            g->Coords.Size,
            g->Indices.Size);

        AssertThat(g->Vertices.Size, nanoMesh->NumVerts);
        AssertThat(g->Normals.Size, g->Vertices.Size); // PerVertexFlatten
        AssertThat(g->Coords.Size,  g->Vertices.Size); // PerVertexFlatten

        AssertThat(NanoMeshSave(nanoMesh, "head_male.saved.obj"), true);

        NanoMeshClose(nanoMesh);
    }
};
