#include <Nano/Mesh.h>
#include <rpp/file_io.h>
#include <rpp/sprint.h>
#include <rpp/timer.h>
#include <unordered_set>
#include <cstdlib>
#include "InternalConfig.h"

namespace Nano
{
    using std::make_shared;
    using rpp::file;
    using rpp::string_buffer;
    using rpp::buffer_line_parser;
    using namespace rpp::literals;
    ///////////////////////////////////////////////////////////////////////////////////////////////

    static bool SaveMaterials(const Mesh& mesh, strview materialSavePath, strview fileName, Options opt)
    {
        if (mesh.Groups.empty() || !mesh.HasAnyMaterials())
            return false;

        vector<Material*> written;
        shared_ptr<Material> defaultMat;
        auto getDefaultMat = [&]
        {
            if (defaultMat)
                return defaultMat;
            defaultMat = mesh.FindMaterial("default"_sv);
            if (!defaultMat) {
                defaultMat = make_shared<Material>();
                defaultMat->Name = "default"s;
            }
            return defaultMat;
        };

        if (file f = file{ materialSavePath, rpp::CREATENEW })
        {
            string_buffer sb;
            auto writeColor = [&](strview id, Color3 color) { sb.writeln(id, color.r, color.g, color.b); };
            auto writeStr   = [&](strview id, strview str)  { if (str) sb.writeln(id, str); };
            auto writeFloat = [&](strview id, float value)  { if (value != 1.0f) sb.writeln(id, value); };

            sb.writeln("#", fileName, "MTL library");
            for (const MeshGroup& group : mesh.Groups)
            {
                Material& mat = *(group.Mat ? group.Mat : getDefaultMat()).get();
                if (rpp::contains(written, &mat))
                    continue; // skip
                written.push_back(&mat);

                sb.writeln("newmtl", mat.Name);
                writeColor("Ka", mat.AmbientColor);
                writeColor("Kd", mat.DiffuseColor);
                writeColor("Ks", mat.SpecularColor);
                if (mat.EmissiveColor.notZero())
                    writeColor("Ke", mat.EmissiveColor);

                writeFloat("Ns", rpp::clamp(mat.Specular*1000.0f, 0.0f, 1000.0f)); // Ns is [0, 1000]
                writeFloat("d", mat.Alpha);

                writeStr("map_Kd",   mat.DiffusePath);
                writeStr("map_d",    mat.AlphaPath);
                writeStr("map_Ks",   mat.SpecularPath);
                writeStr("map_bump", mat.NormalPath);
                writeStr("map_Ke",   mat.EmissivePath);

                sb.writeln("illum 2"); // default smooth shaded rendering
            }

            if (f.write(sb) == sb.size())
                return true;

            NanoErr(opt, "File write failed: %s", materialSavePath)
        }
        else
        {
            NanoErr(opt, "Failed to create file: %s", materialSavePath);
        }
        return false;
    }

    static vector<shared_ptr<Material>> LoadMaterials(strview matlibFile)
    {
        vector<shared_ptr<Material>> materials;

        if (auto parser = buffer_line_parser::from_file(matlibFile))
        {
            string matlibFolder = folder_path(matlibFile);
            Material* mat = nullptr;
            strview line;
            while (parser.read_line(line))
            {
                strview id = line.next(' ');
                if (id == "newmtl")
                {
                    materials.push_back(make_shared<Material>());
                    mat = materials.back().get();
                    mat->Name         = line.trim();
                    mat->MaterialFile = matlibFile;
                }
                else if (mat)
                {
                    if      (id == "Ka") mat->AmbientColor  = Color3::parseColor(line);
                    else if (id == "Kd") mat->DiffuseColor  = Color3::parseColor(line);
                    else if (id == "Ks") mat->SpecularColor = Color3::parseColor(line);
                    else if (id == "Ke") mat->EmissiveColor = Color3::parseColor(line);
                    else if (id == "Ns") mat->Specular = line.to_float() / 1000.0f; // Ns is [0, 1000], normalize to [0, 1]
                    else if (id == "d")  mat->Alpha    = line.to_float();
                    else if (id == "Tr") mat->Alpha    = 1.0f - line.to_float();
                    else if (id == "map_Kd")   mat->DiffusePath  = matlibFolder + line.next(' ');
                    else if (id == "map_d")    mat->AlphaPath    = matlibFolder + line.next(' ');
                    else if (id == "map_Ks")   mat->SpecularPath = matlibFolder + line.next(' ');
                    else if (id == "map_bump") mat->NormalPath   = matlibFolder + line.next(' ');
                    else if (id == "map_Ke")   mat->EmissivePath = matlibFolder + line.next(' ');
                }
            }
        }
        return materials;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    static constexpr size_t MaxStackAlloc = 1024 * 1024;

    struct ObjLoader
    {
        Mesh& mesh;
        strview meshPath;
        Options options;
        buffer_line_parser parser;
        size_t numVerts = 0, numCoords = 0, numNormals = 0, numColors = 0, numFaces = 0;
        vector<shared_ptr<Material>> materials;
        MeshGroup* group = nullptr;
        bool triedDefaultMat = false;

        Vector3* vertsData   = nullptr;
        Vector2* coordsData  = nullptr;
        Vector3* normalsData = nullptr;
        Color3*  colorsData  = nullptr;

        void* dataBuffer = nullptr;
        size_t bufferSize = 0;

        explicit ObjLoader(Mesh& mesh, strview meshPath, Options options)
            : mesh{ mesh }, meshPath{ meshPath }, options{ options }, 
              parser{ buffer_line_parser::from_file(meshPath) }
        {
        }

        ~ObjLoader()
        {
            if (bufferSize > MaxStackAlloc) free(dataBuffer);
        }

        bool ProbeStats()
        {
            strview line;
            while (parser.read_line(line))
            {
                char c = line[0];
                if (c == 'v') switch (line[1])
                {
                    case ' ': ++numVerts;   break;
                    case 'n': ++numNormals; break;
                    case 't': ++numCoords;  break;
                    default:;
                }
                else if (c == 'f' && line[1] == ' ')
                {
                    ++numFaces;
                }
            }

            parser.reset();
            if (numVerts == 0) {
                NanoErr(options, "Mesh::LoadOBJ() failed: No vertices in %s\n", meshPath);
                return false;
            }

            // megaBuffer strategy - one big allocation instead of a dozen small ones
            bufferSize = numVerts    * sizeof(Vector3)
                        + numCoords  * sizeof(Vector2)
                        + numNormals * sizeof(Vector3)
                        + numVerts   * sizeof(Color3);
            return true;
        }

        struct pool_helper {
            void* ptr;
            template<class T> T* next(size_t count) {
                T* data = (T*)ptr;
                ptr = data + count;
                return data;
            }
        };

        void InitPointers(void* allocated)
        {
            dataBuffer = allocated;
            pool_helper pool = { allocated };
            vertsData   = pool.next<Vector3>(numVerts);
            coordsData  = pool.next<Vector2>(numCoords);
            normalsData = pool.next<Vector3>(numNormals);
            colorsData  = pool.next<Color3>(numVerts);
        }

        shared_ptr<Material> FindMat(strview matName)
        {
            if (materials.empty() && !triedDefaultMat) {
                triedDefaultMat = true;
                string defaultMat = file_replace_ext(meshPath, "mtl");
                materials = LoadMaterials(defaultMat);
            }
            for (auto& mat : materials)
                if (matName.equalsi(mat->Name))
                    return mat;
            return {};
        }

        MeshGroup* CurrentGroup()
        {
            return group ? group : (group = &mesh.CreateGroup({}));
        }

        void ParseMeshData()
        {
            int vertexId = 0, coordId = 0, normalId = 0, colorId = 0;

            strview line;
            while (parser.read_line(line)) // for each line
            {
                char c = line[0];
                if (c == 'v')
                {
                    c = line[1];
                    if (c == ' ') { // v 1.0 1.0 1.0
                        line.skip(2); // skip 'v '
                        Vector3& v = vertsData[vertexId];
                        line >> v.x >> v.y >> v.z;

                        if (!line.empty())
                        {
                            Vector3 col;
                            line >> col.x >> col.y >> col.z;
                            if (col.sqlength() > 0.001f)
                            {
                                // for OBJ we always use Per-Vertex color mapping...
                                // there is simply no other standardised way to do it
                                if (colorId == 0) {
                                    memset(colorsData, 0, numVerts*sizeof(Color3));
                                    numColors = numVerts;
                                }
                                ++colorId;
                                colorsData[vertexId] = col;
                            }
                        }
                        ++vertexId;
                        continue;
                    }
                    if (c == 'n') { // vn 1.0 1.0 1.0
                        line.skip(3); // skip 'vn '
                        Vector3& n = normalsData[normalId++];
                        line >> n.x >> n.y >> n.z;
                        // Use this if exporting for Direct3D
                        //n.z = -n.z; // invert Z to convert to lhs coordinates
                        continue;
                    }
                    if (c == 't') { // vt 1.0 1.0
                        line.skip(3); // skip 'vt '
                        Vector2& uv = coordsData[coordId++];
                        line >> uv.x >> uv.y;
                        //if (fmt == TXC_Direct3DTexCoords) // Use this if exporting for Direct3D
                        //    c.y = 1.0f - c.y; // invert the V coord to convert to lhs coordinates
                        continue;
                    }
                }
                else if (c == 'f')
                {
                    // f Vertex1/Texture1/Normal1 Vertex2/Texture2/Normal2 Vertex3/Texture3/Normal3
                    auto& faces = CurrentGroup()->Tris;
                    Triangle* f = &rpp::emplace_back(faces);

                    // load the face indices
                    line.skip(2); // skip 'f '
                    line.trim_start(' ');

                    auto parseDescr = [&](VertexDescr& vd, strview vertdescr)
                    {
                        if (strview v = vertdescr.next('/')) {
                            vd.v = v.to_int() - 1;
                            // negative indices, relative to the current maximum vertex position
                            // (-1 references the last vertex defined)
                            if (vd.v < 0) vd.v = vertexId + vd.v + 1; 
                        }
                        if (strview t = vertdescr.next('/')) {
                            vd.t = t.to_int() - 1;
                            if (vd.t < 0) vd.t = coordId + vd.t + 1;
                        }
                        if (strview n = vertdescr) {
                            vd.n = n.to_int() - 1;
                            if (vd.n < 0) vd.n = normalId + vd.n + 1;
                        }
                    };

                    // parse triangle
                    parseDescr(f->a, line.next(' '));
                    parseDescr(f->b, line.next(' '));
                    parseDescr(f->c, line.next(' '));

                    // when encountering quads or large polygons, we need to triangulate the mesh
                    // by tracking the first vertex descr and forming a fan; this requires convex polys
                    while (strview vertdescr = line.next(' '))
                    {
                        // @note According to OBJ spec, face vertices are in CCW order:
                        // 0--3
                        // |\ |
                        // | \|
                        // 1--2

                        // v[0], v[2], v[3]
                        VertexDescr vd0 = f->a; // by value, because emplace_back may realloc
                        VertexDescr vd2 = f->c;
                        f = &rpp::emplace_back(faces);
                        f->a = vd0;
                        f->b = vd2;
                        parseDescr(f->c, vertdescr);
                    }
                }
                //else if (c == 's')
                //{
                //    line.skip(2); // skip "s "
                //    line >> group->SmoothingGroup;
                //}
                else if (c == 'u' && memcmp(line.str, "usemtl", 6) == 0)
                {
                    line.skip(7); // skip "usemtl "
                    strview matName = line.next(' ');
                    CurrentGroup()->Mat = FindMat(matName);
                }
                else if (c == 'm' && memcmp(line.str, "mtllib", 6) == 0)
                {
                    line.skip(7); // skip "mtllib "
                    strview matlib = line.next(' ');
                    string matlibPath = path_combine(folder_path(meshPath), matlib);
                    materials = LoadMaterials(matlibPath);
                }
                else if (c == 'g')
                {
                    line.skip(2); // skip "g "
                    bool ignoreGroup = options.ForceSingleGroup && group;
                    if (!ignoreGroup)
                    {
                        group = &mesh.FindOrCreateGroup(line.next(' '));
                    }
                }
                else if (c == 'o')
                {
                    line.skip(2); // skip "o "
                    mesh.Name = (string)line.next(' ');
                }
            }
        }

        void RemoveEmptyGroups() const
        {
            for (auto it = mesh.Groups.begin(); it != mesh.Groups.end();)
                if (it->IsEmpty()) it = mesh.Groups.erase(it); else ++it;
        }

        // unfortunately, Blender does not export verts in linear order
        // @return New index
        static int GetNewIndex(std::vector<int>& orderedUniqueIds, int oldIndex)
        {
            int* data = orderedUniqueIds.data();
            int size = (int)orderedUniqueIds.size();
            for (int newIndex = 0; newIndex < size; ++newIndex)
                if (data[newIndex] == oldIndex)
                    return newIndex;
            orderedUniqueIds.push_back(oldIndex);
            return size;
        }

        void SlowBlenderHack(MeshGroup& g) const
        {
            const bool vertexColors = numColors > 0;

            std::vector<int> uniqueVerts, uniqueCoords, uniqueNormals;
            uniqueVerts.reserve(g.Tris.size() * 3);
            if (g.Tris.front().a.t != -1) uniqueCoords.reserve(uniqueVerts.size());
            if (g.Tris.front().a.n != -1) uniqueNormals.reserve(uniqueVerts.size());

            for (Triangle& face : g.Tris)
            {
                for (VertexDescr& vd : face)
                {
                    vd.v = GetNewIndex(uniqueVerts, vd.v);
                    if (vd.t != -1) vd.t = GetNewIndex(uniqueCoords, vd.t);
                    if (vd.n != -1) vd.n = GetNewIndex(uniqueNormals, vd.n);
                    if (vertexColors) vd.c = vd.v;
                }
            }

            auto copyElements = [](auto& dst, auto* src, const vector<int>& uniqueIndices) {
                dst.reserve(uniqueIndices.size());
                for (int index : uniqueIndices)
                    dst.push_back(src[index]);
            };
            copyElements(g.Verts, vertsData, uniqueVerts);
            copyElements(g.Coords, coordsData, uniqueCoords);
            copyElements(g.Normals, normalsData, uniqueNormals);
            if (vertexColors) {
                // OBJ colors use per-vertex mapping
                copyElements(g.Colors, colorsData, uniqueVerts);
                g.ColorMapping = MapPerVertex;
            }
        }

        // when OBJ mesh has only 1 group
        void CopyAllMeshDataToGroup(MeshGroup& g) const
        {
            auto copyElements = [](auto& dst, auto* src, int count) {
                if (count > 0) dst.assign(src, src + count);
            };
            copyElements(g.Verts,   vertsData,   numVerts);
            copyElements(g.Coords,  coordsData,  numCoords);
            copyElements(g.Normals, normalsData, numNormals);
            if (numColors > 0) {
                // OBJ colors use per-vertex mapping
                copyElements(g.Colors, colorsData, numColors);
                g.ColorMapping = MapPerVertex;
            }
        }

        void BuildGroup(MeshGroup& g, int numGroups) const
        {
            if (g.Name.empty() && g.Mat) // assign default name
                g.Name = g.Mat->Name;
            if (g.Tris.empty())
                return;

            rpp::Timer t;
            const bool useSlowBlenderHack = false;
            if (useSlowBlenderHack)
            {
                SlowBlenderHack(g);
            }
            else
            {
                // if total number of groups is 1, then we don't need anything
                // complicated, just copy all the data and we're done
                if (numGroups == 1)
                {
                    LogInfo("CopyAllMeshDataToGroup");
                    CopyAllMeshDataToGroup(g);
                }
                else
                // because OBJ stores a global list of vertices, normals, uvs,
                // we need to completely recalculate indices and arrays for each group
                {
                    LogInfo("SlowBlenderHack");
                    SlowBlenderHack(g);
                }
            }

            int numVerts   = g.NumVerts();
            int numNormals = g.NumNormals();
            int numCoords  = g.NumCoords();
            int numTris    = g.NumTris();

            if      (numNormals <= 0)        g.NormalsMapping = MapNone;
            else if (numNormals == numVerts) g.NormalsMapping = MapPerVertex;
            else if (numNormals == numTris)  g.NormalsMapping = MapPerFace;
            else if (numNormals >  numVerts) g.NormalsMapping = MapPerFaceVertex;
            else                             g.NormalsMapping = MapSharedElements;
            if      (numCoords == 0)         g.CoordsMapping  = MapNone;
            else if (numCoords == numVerts)  g.CoordsMapping  = MapPerVertex;
            else if (numCoords >  numVerts)  g.CoordsMapping  = MapPerFaceVertex;
            else Assert(false, "Unfamiliar CoordsMapping mode");

            LogInfo("BuildGroup %s elapsed: %.1fms", g.Name, t.elapsed_ms());
        }

        void BuildMeshGroups() const
        {
            for (MeshGroup& g : mesh.Groups)
            {
                BuildGroup(g, (int)mesh.Groups.size());

                // OBJ default face winding is CCW
                g.Winding = FaceWindCounterClockWise;
            }

            if (!mesh.Groups.empty() && mesh.Groups.front().Name.empty())
                mesh.Groups.front().Name = "default";
        }
    };

    bool Mesh::LoadOBJ(strview meshPath, Options opt)
    {
        Clear();

        ObjLoader loader { *this, meshPath, opt };

        if (!loader.parser) {
            NanoErr(opt, "Failed to open file: %s", meshPath);
            return false;
        }

        if (!loader.ProbeStats()) {
            NanoErr(opt, "Mesh::LoadOBJ() failed! No vertices in: %s", meshPath);
            return false;
        }

        if (opt.LogMeshGroupInfo) {
            LogInfo("Load %-33s  %5zu verts  %5zu polys",
                file_nameext(meshPath), loader.numVerts, loader.numFaces);
        }
        
        // OBJ maps vertex data globally, not per-mesh-group like most game engines expect
        // so this really complicates things when we build the mesh groups...
        // to speed up mesh loading, we use very heavy stack allocation

        // ObjLoader will free these in destructor if malloc was used
        // ReSharper disable once CppNonReclaimedResourceAcquisition
        void* mem = loader.bufferSize <= MaxStackAlloc
                    ? alloca(loader.bufferSize)
                    : malloc(loader.bufferSize);
        loader.InitPointers(mem);
        loader.ParseMeshData();

        if (!opt.CreateEmptyGroups)
            loader.RemoveEmptyGroups();

        loader.BuildMeshGroups();
        ApplyLoadOptions(opt);
        return true;
    }


    ///////////////////////////////////////////////////////////////////////////////////////////////

    static vector<Vector3> FlattenColors(const MeshGroup& group)
    {
        vector<Vector3> colors = { group.Verts.size(), Vector3::Zero() };

        for (const Triangle& face : group)
            for (const VertexDescr& vd : face)
            {
                if (vd.c == -1) continue;
                Vector3& dst = colors[vd.v];
                if (dst == Vector3::Zero() || dst == Vector3::One())
                    dst = group.Colors[vd.c];
            }
        return colors;
    }

    bool Mesh::SaveAsOBJ(strview meshPath, Options opt) const
    {
        if (file f { meshPath, rpp::CREATENEW })
        {
            string_buffer sb;
            // straight to file, #dontcare about perf atm
            if (opt.LogMeshGroupInfo) {
                LogInfo("Save %-33s  %5d verts  %5d tris", 
                    file_nameext(meshPath), TotalVerts(), TotalTris());
            }
            string matlib = rpp::file_replace_ext(meshPath, "mtl");
            strview matlibFile = rpp::file_nameext(matlib);
            if (SaveMaterials(*this, matlib, matlibFile, opt))
                sb.writeln("mtllib", matlibFile);

            if (!Name.empty())
                sb.writeln("o", Name);

            int vertexBase  = 1;
            int coordsBase  = 1;
            int normalsBase = 1;

            for (int group = 0; group < (int)Groups.size(); ++group)
            {
                const MeshGroup& g = Groups[group];
                if (opt.LogMeshGroupInfo)
                    g.Print();

                auto* vertsData = g.Verts.data();
                if (g.Colors.empty())
                {
                    for (const Vector3& v : g.Verts)
                        sb.writef("v %.6f %.6f %.6f\n", v.x, v.y, v.z);
                }
                else // non-standard extension for OBJ vertex colors
                {
                    // @todo Just leave a warning and export incorrect vertex colors?
                    Assert((g.ColorMapping == MapPerVertex || g.ColorMapping == MapPerFaceVertex),
                           "OBJ export only supports per-vertex and per-face-vertex color mapping!");
                    Assert(g.NumColors() >= g.NumVerts(), "Group %s NumColors does not match NumVerts", g.Name);

                    auto& colors = g.ColorMapping == MapPerFaceVertex ? FlattenColors(g) : g.Colors;
                    auto* colorsData = colors.data();

                    const int numVerts = g.NumVerts();
                    for (int i = 0; i < numVerts; ++i)
                    {
                        const Vector3& v = vertsData[i];
                        const Vector3& c = colorsData[i];
                        if (c == Vector3::Zero()) sb.writef("v %.6f %.6f %.6f\n", v.x, v.y, v.z);
                        else sb.writef("v %.6f %.6f %.6f %.6f %.6f %.6f\n", v.x, v.y, v.z, c.x, c.y, c.z);
                    }
                }

                for (const Vector2& v : g.Coords)  sb.writef("vt %.4f %.4f\n", v.x, v.y);
                for (const Vector3& v : g.Normals) sb.writef("vn %.4f %.4f %.4f\n", v.x, v.y, v.z);

                if (!g.Name.empty()) sb.writeln("g", g.Name);
                if (g.Mat)           sb.writeln("usemtl", g.Mat->Name);
                sb.writeln("s", group);
                for (const Triangle& face : g.Tris)
                {
                    sb.write('f');
                    for (const VertexDescr& vd : face)
                    {
                        sb.write(' '); sb.write(vd.v + vertexBase);
                        if (vd.t != -1) { sb.write('/'); sb.write(vd.t + coordsBase); }
                        if (vd.n != -1) { sb.write('/'); sb.write(vd.n + normalsBase); }
                    }
                    sb.writeln();
                }

                vertexBase  += g.NumVerts();
                coordsBase  += g.NumCoords();
                normalsBase += g.NumNormals();
            }

            if (f.write(sb) == sb.size())
                return true;

            NanoErr(opt, "File write failed: %s", meshPath);
        }
        else
        {
            NanoErr(opt, "Failed to create file: %s", meshPath);
        }
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
}
