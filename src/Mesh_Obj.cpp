#include <Nano/Mesh.h>
#include <rpp/file_io.h>
#include <rpp/sprint.h>
#include <rpp/timer.h>
#include <unordered_set>
#include <cstdlib>
#include "InternalConfig.h"

namespace Nano
{
    using namespace rpp::literals;
    ///////////////////////////////////////////////////////////////////////////////////////////////

    static bool SaveMaterials(const Mesh& mesh, rpp::strview materialSavePath, rpp::strview fileName, Options opt)
    {
        if (mesh.Groups.empty() || !mesh.HasAnyMaterials())
            return false;

        std::vector<Material*> written;
        std::shared_ptr<Material> defaultMat;
        auto getDefaultMat = [&]
        {
            if (defaultMat)
                return defaultMat;
            defaultMat = mesh.FindMaterial("default"_sv);
            if (!defaultMat) {
                defaultMat = std::make_shared<Material>();
                defaultMat->Name = "default"s;
            }
            return defaultMat;
        };

        rpp::file f { materialSavePath, rpp::file::CREATENEW };
        if (!f) {
            NanoErr(opt, "Failed to create file: %s", materialSavePath);
        }
        rpp::string_buffer sb;
        auto writeColor = [&](rpp::strview id, rpp::Color3 color) { sb.writeln(id, color.r, color.g, color.b); };
        auto writeStr   = [&](rpp::strview id, rpp::strview str)  { if (str) sb.writeln(id, str); };
        auto writeFloat = [&](rpp::strview id, float value)       { if (value != 1.0f) sb.writeln(id, value); };

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

        if (f.write(sb) != sb.size()) {
            NanoErr(opt, "File write failed: %s", materialSavePath)
        }
        return true;
    }

    static std::vector<std::shared_ptr<Material>> LoadMaterials(rpp::strview matlibFile)
    {
        std::vector<std::shared_ptr<Material>> materials;

        if (auto parser = rpp::buffer_line_parser::from_file(matlibFile))
        {
            std::string matlibFolder = folder_path(matlibFile);
            Material* mat = nullptr;
            rpp::strview line;
            while (parser.read_line(line))
            {
                rpp::strview id = line.next(' ');
                if (id == "newmtl")
                {
                    materials.push_back(std::make_shared<Material>());
                    mat = materials.back().get();
                    mat->Name = line.trim();
                }
                else if (mat)
                {
                    if      (id == "Ka") mat->AmbientColor  = rpp::Color3::parseColor(line);
                    else if (id == "Kd") mat->DiffuseColor  = rpp::Color3::parseColor(line);
                    else if (id == "Ks") mat->SpecularColor = rpp::Color3::parseColor(line);
                    else if (id == "Ke") mat->EmissiveColor = rpp::Color3::parseColor(line);
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
        rpp::strview meshPath;
        Options options;
        rpp::buffer_line_parser parser;
        size_t numVerts = 0, numCoords = 0, numNormals = 0, numColors = 0, numFaces = 0;
        std::vector<std::shared_ptr<Material>> materials;
        MeshGroup* group = nullptr;
        bool triedDefaultMat = false;

        rpp::Vector3* vertsData   = nullptr;
        rpp::Vector2* coordsData  = nullptr;
        rpp::Vector3* normalsData = nullptr;
        rpp::Color3*  colorsData  = nullptr;

        void* dataBuffer = nullptr;
        size_t bufferSize = 0;

        explicit ObjLoader(Mesh& mesh, rpp::strview meshPath, Options options)
            : mesh{ mesh }, meshPath{ meshPath }, options{ options }, 
              parser{ rpp::buffer_line_parser::from_file(meshPath) }
        {
        }

        ~ObjLoader()
        {
            if (bufferSize > MaxStackAlloc) free(dataBuffer);
        }

        bool ProbeStats()
        {
            rpp::strview line;
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
            }

            // megaBuffer strategy - one big allocation instead of a dozen small ones
            bufferSize = numVerts    * sizeof(rpp::Vector3)
                        + numCoords  * sizeof(rpp::Vector2)
                        + numNormals * sizeof(rpp::Vector3)
                        + numVerts   * sizeof(rpp::Color3);
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
            vertsData   = pool.next<rpp::Vector3>(numVerts);
            coordsData  = pool.next<rpp::Vector2>(numCoords);
            normalsData = pool.next<rpp::Vector3>(numNormals);
            colorsData  = pool.next<rpp::Color3>(numVerts);
        }

        std::shared_ptr<Material> FindMat(rpp::strview matName)
        {
            if (materials.empty() && !triedDefaultMat) {
                triedDefaultMat = true;
                std::string defaultMat = file_replace_ext(meshPath, "mtl");
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

            rpp::strview line;
            while (parser.read_line(line)) // for each line
            {
                char c = line[0];
                if (c == 'v')
                {
                    c = line[1];
                    if (c == ' ') { // v 1.0 1.0 1.0
                        line.skip(2); // skip 'v '
                        rpp::Vector3& v = vertsData[vertexId];
                        line >> v.x >> v.y >> v.z;

                        if (!line.empty())
                        {
                            rpp::Vector3 col;
                            line >> col.x >> col.y >> col.z;
                            if (col.sqlength() > 0.001f)
                            {
                                // for OBJ we always use Per-Vertex color mapping...
                                // there is simply no other standardised way to do it
                                if (colorId == 0) {
                                    memset(colorsData, 0, numVerts*sizeof(rpp::Color3));
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
                        rpp::Vector3& n = normalsData[normalId++];
                        line >> n.x >> n.y >> n.z;
                        // Use this if exporting for Direct3D
                        //n.z = -n.z; // invert Z to convert to lhs coordinates
                        continue;
                    }
                    if (c == 't') { // vt 1.0 1.0
                        line.skip(3); // skip 'vt '
                        rpp::Vector2& uv = coordsData[coordId++];
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

                    auto parseDescr = [&](VertexDescr& vd, rpp::strview vertdescr)
                    {
                        if (rpp::strview v = vertdescr.next('/')) {
                            vd.v = v.to_int() - 1;
                            // negative indices, relative to the current maximum vertex position
                            // (-1 references the last vertex defined)
                            if (vd.v < 0) vd.v = vertexId + vd.v + 1; 
                        }
                        if (rpp::strview t = vertdescr.next('/')) {
                            vd.t = t.to_int() - 1;
                            if (vd.t < 0) vd.t = coordId + vd.t + 1;
                        }
                        if (rpp::strview n = vertdescr) {
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
                    while (rpp::strview vertdescr = line.next(' '))
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
                    rpp::strview matName = line.next(' ');
                    CurrentGroup()->Mat = FindMat(matName);
                }
                else if (c == 'm' && memcmp(line.str, "mtllib", 6) == 0)
                {
                    line.skip(7); // skip "mtllib "
                    rpp::strview matlib = line.next(' ');
                    std::string matlibPath = path_combine(folder_path(meshPath), matlib);
                    materials = LoadMaterials(matlibPath);
                }
                else if (c == 'g')
                {
                    line.skip(2); // skip "g "
                    bool ignoreGroup = (options & Options::SingleGroup) && group;
                    if (!ignoreGroup)
                    {
                        group = &mesh.FindOrCreateGroup(line.next(' '));
                    }
                }
                else if (c == 'o')
                {
                    line.skip(2); // skip "o "
                    mesh.Name = (std::string)line.next(' ');
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

            auto copyElements = [](auto& dst, auto* src, const std::vector<int>& uniqueIndices) {
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
                g.ColorMapping = MapMode::PerVertex;
            }
        }

        // when OBJ mesh has only 1 group
        void CopyAllMeshDataToOneGroup(MeshGroup& g) const
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
                g.ColorMapping = MapMode::PerVertex;
            }
        }

        void BuildGroup(MeshGroup& g, int numGroups) const
        {
            if (g.Name.empty() && g.Mat) // assign default name
                g.Name = g.Mat->Name;
            if (g.Tris.empty())
                return;

            //rpp::Timer t;
            // if total number of groups is 1, then we don't need anything
            // complicated, just copy all the data and we're done
            if (numGroups == 1)
            {
                CopyAllMeshDataToOneGroup(g);
            }
            else
                // because OBJ stores a global list of vertices, normals, uvs,
                // we need to completely recalculate indices and arrays for each group
            {
                SlowBlenderHack(g);
            }

            int numVerts   = g.NumVerts();
            int numNormals = g.NumNormals();
            int numCoords  = g.NumCoords();
            int numTris    = g.NumTris();

            if      (numNormals <= 0)        g.NormalsMapping = MapMode::None;
            else if (numNormals == numVerts) g.NormalsMapping = MapMode::PerVertex;
            else if (numNormals == numTris)  g.NormalsMapping = MapMode::PerFace;
            else if (numNormals >  numVerts) g.NormalsMapping = MapMode::PerFaceVertex;
            else                             g.NormalsMapping = MapMode::SharedElements;
            if      (numCoords == 0)         g.CoordsMapping  = MapMode::None;
            else if (numCoords == numVerts)  g.CoordsMapping  = MapMode::PerVertex;
            else if (numCoords >  numVerts)  g.CoordsMapping  = MapMode::PerFaceVertex;
            else Assert(false, "Unfamiliar CoordsMapping mode");

            //LogInfo("BuildGroup %s elapsed: %.1fms", g.Name, t.elapsed_ms());
        }

        void BuildMeshGroups() const
        {
            for (MeshGroup& g : mesh.Groups)
            {
                BuildGroup(g, (int)mesh.Groups.size());

                // OBJ default face winding is CCW
                g.Winding = FaceWinding::CCW;
            }

            if (!mesh.Groups.empty() && mesh.Groups.front().Name.empty())
                mesh.Groups.front().Name = "default";
        }
    };

    bool Mesh::LoadOBJ(rpp::strview meshPath, Options opt)
    {
        Clear();

        ObjLoader loader { *this, meshPath, opt };

        if (!loader.parser) {
            NanoErr(opt, "Failed to open file: %s", meshPath);
        }

        if (!loader.ProbeStats()) {
            NanoErr(opt, "Mesh::LoadOBJ() failed! No vertices in: %s", meshPath);
        }

        if (opt & Options::Log) {
            LogInfo("Load %-33s  %5zu verts  %5zu polys  %s",
                file_nameext(meshPath), loader.numVerts, loader.numFaces, to_string(opt));
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

        if (!(opt & Options::EmptyGroups))
            loader.RemoveEmptyGroups();

        loader.BuildMeshGroups();
        ApplyLoadOptions(opt);
        return true;
    }


    ///////////////////////////////////////////////////////////////////////////////////////////////

    static std::vector<rpp::Vector3> FlattenColors(const MeshGroup& group)
    {
        std::vector<rpp::Vector3> colors = { group.Verts.size(), rpp::Vector3::Zero() };

        for (const Triangle& face : group)
            for (const VertexDescr& vd : face)
            {
                if (vd.c == -1) continue;
                rpp::Vector3& dst = colors[vd.v];
                if (dst == rpp::Vector3::Zero() || dst == rpp::Vector3::One())
                    dst = group.Colors[vd.c];
            }
        return colors;
    }

    bool Mesh::SaveAsOBJ(rpp::strview meshPath, Options opt) const
    {
        rpp::file f { meshPath, rpp::file::CREATENEW };
        if (!f) {
            NanoErr(opt, "Failed to create file: %s", meshPath);
        }

        rpp::string_buffer sb;
        // straight to file, #dontcare about perf atm
        if (opt & Options::Log) {
            LogInfo("Save %-33s  %5d verts  %5d tris", 
                file_nameext(meshPath), TotalVerts(), TotalTris());
        }
        std::string matlib = rpp::file_replace_ext(meshPath, "mtl");
        rpp::strview matlibFile = rpp::file_nameext(matlib);
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
            if (opt & Options::Log) {
                g.Print();
            }

            auto* vertsData = g.Verts.data();
            if (g.Colors.empty())
            {
                for (const rpp::Vector3& v : g.Verts)
                    sb.writef("v %.6f %.6f %.6f\n", v.x, v.y, v.z);
            }
            else // non-standard extension for OBJ vertex colors
            {
                // @todo Just leave a warning and export incorrect vertex colors?
                Assert((g.ColorMapping == MapMode::PerVertex || g.ColorMapping == MapMode::PerFaceVertex),
                       "OBJ export only supports per-vertex and per-face-vertex color mapping!");
                Assert(g.NumColors() >= g.NumVerts(), "Group %s NumColors does not match NumVerts", g.Name);

                auto& colors = g.ColorMapping == MapMode::PerFaceVertex ? FlattenColors(g) : g.Colors;
                auto* colorsData = colors.data();

                const int numVerts = g.NumVerts();
                for (int i = 0; i < numVerts; ++i)
                {
                    const rpp::Vector3& v = vertsData[i];
                    const rpp::Vector3& c = colorsData[i];
                    if (c == rpp::Vector3::Zero()) sb.writef("v %.6f %.6f %.6f\n", v.x, v.y, v.z);
                    else sb.writef("v %.6f %.6f %.6f %.6f %.6f %.6f\n", v.x, v.y, v.z, c.x, c.y, c.z);
                }
            }

            for (const rpp::Vector2& v : g.Coords)  sb.writef("vt %.4f %.4f\n", v.x, v.y);
            for (const rpp::Vector3& v : g.Normals) sb.writef("vn %.4f %.4f %.4f\n", v.x, v.y, v.z);

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

        if (f.write(sb) != sb.size()) {
            NanoErr(opt, "File write failed: %s", meshPath);
        }
        return true;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
}
