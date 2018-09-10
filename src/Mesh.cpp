#include <Nano/Mesh.h>
#include <rpp/file_io.h>
#include <rpp/sprint.h>
#include <rpp/timer.h>
#include "InternalConfig.h"

namespace Nano
{
    using std::swap;
    using std::move;
    using std::make_shared;
    using rpp::string_buffer;
    using rpp::append;
    using namespace rpp::literals;
    ///////////////////////////////////////////////////////////////////////////////////////////////

    bool VertexDescr::operator==(const VertexDescr& vd) const
    {
        return v == vd.v && t == vd.t && n == vd.n && c == vd.c;
    }

    bool VertexDescr::operator!=(const VertexDescr& vd) const
    {
        return v != vd.v || t != vd.t || n != vd.n || c != vd.c;
    }

    bool Triangle::ContainsVertexId(int vertexId) const
    {
        return a.v == vertexId || b.v == vertexId || c.v == vertexId;
    }

    bool Triangle::operator==(const Triangle& t) const
    {
        return a == t.a && b == t.b && c == t.c;
    }

    bool Triangle::operator!=(const Triangle& t) const
    {
        return a != t.a || b != t.b || c != t.c;
    }

    string to_string(const Triangle& triangle)
    {
        string_buffer sb;
        sb.writef("{%d,%d,%d}", triangle.a.v, triangle.b.v, triangle.c.v);
        return sb.str();
    }

    Vector3 PickedTriangle::center() const
    {
        Assert(good(), "Invalid PickedTriangle");
        Vector3 c = group->Vertex(face->a);
        c += group->Vertex(face->b);
        c += group->Vertex(face->c);
        c /= 3;
        return c;
    }

    Vector3 PickedTriangle::vertex(const VertexDescr& vd) const
    {
        Assert(good(), "Invalid PickedTriangle");
        Assert(vd.v != -1 && vd.v < group->NumVerts(), 
               "Invalid VertexDescr: %d / %d", vd.v, group->NumVerts());

        return group->VertexData()[vd.v];
    }

    int PickedTriangle::id() const
    {
        if (!group || !face)
            return -1;
        const Triangle* faces = group->Tris.data();
        size_t count = group->Tris.size();
        for (size_t i = 0; i < count; ++i)
            if (faces + i == face)
                return int(i);
        return -1;
    }

    string to_string(const PickedTriangle& triangle)
    {
        string_buffer sb;
        sb.write('{');
        sb.write(triangle.group?triangle.group->GroupId:-1);
        sb.write(',');
        if (triangle.face) sb.write(*triangle.face);
        else               sb.write(-1);
        sb.write('}');
        return sb.str();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    Material& MeshGroup::CreateMaterial(string name)
    {
        Mat = make_shared<Material>();
        Mat->Name = move(name);
        return *Mat;
    }

    void MeshGroup::SetFaceWinding(FaceWinding winding) noexcept
    {
        if (Winding == winding)
            return;
        for (Triangle& tri : Tris) {
            // 0 1 2 --> 0 2 1
            swap(tri.b, tri.c);
        }
        Winding = winding;
    }

    void MeshGroup::SetCoordSys(CoordSys targetSystem) noexcept
    {
        if (System == targetSystem)
            return;

        auto isBilateralMatch = [=](CoordSys a, CoordSys b) {
            return (System == a && targetSystem == b)
                || (System == b && targetSystem == a);
        };

        if (isBilateralMatch(CoordSys::GL, CoordSys::Unity)) {
            for (Vector3& v : Verts)   v.x = -v.x;
            for (Vector3& n : Normals) n.x = -n.x;
        }

        System = targetSystem;
    }

    void MeshGroup::UpdateNormal(const VertexDescr& vd0, 
                                 const VertexDescr& vd1, 
                                 const VertexDescr& vd2, 
                                 const bool checkDuplicateVerts) noexcept
    {
        auto* verts   = Verts.data();
        auto* normals = Normals.data();

        const Vector3& v0 = verts[vd0.v];
        const Vector3& v1 = verts[vd1.v];
        const Vector3& v2 = verts[vd2.v];

        // calculate triangle normal:
        Vector3 ba = v1 - v0;
        Vector3 ca = v2 - v0;
        Vector3 normal = ba.cross(ca);

        if (!checkDuplicateVerts)
        {
            // apply normal directly to indexed normals, no exhaustive matching
            Assert(vd0.n != -1 && vd1.n != -1 && vd2.n != -1, "Invalid vertex normals: %d, %d, %d", vd0.n, vd1.n, vd2.n);
            normals[vd0.n] += normal;
            normals[vd1.n] += normal;
            normals[vd2.n] += normal;
        }
        else
        {
            // add normals to any vertex that shares v0/v1/v2 coordinates
            // an unoptimized Mesh may have multiple vertices occupying the same XYZ position
            for (const Triangle& f : Tris)
            {
                for (const VertexDescr& vd : f)
                {
                    const Vector3& v = verts[vd.v];
                    if (v == v0 || v == v1 || v == v2)
                    {
                        Assert(vd.n != -1, "Invalid vertex normalId -1");
                        normals[vd.n] += normal;
                    }
                }
            }
        }

    }

    void MeshGroup::RecalculateNormals(const bool checkDuplicateVerts) noexcept
    {
        for (Vector3& normal : Normals)
            normal = Vector3::Zero();

        FaceWinding winding = Winding;

        // normals are calculated for each tri:
        for (const Triangle& tri : Tris)
        {
            if (winding == FaceWinding::CCW)
            {
                UpdateNormal(tri.a, tri.b, tri.c, checkDuplicateVerts);
            }
            else
            {
                UpdateNormal(tri.c, tri.b, tri.a, checkDuplicateVerts);
            }
        }
        for (Vector3& normal : Normals)
            normal.normalize();
    }

    Vector3 MeshGroup::GetNormalForSelection(const vector<WeightId>& selection) const noexcept
    {
        Vector3 normal = Vector3::Zero();
        if (selection.empty() || NormalsMapping != MapPerVertex)
            return normal;

        const Vector3* normals = NormalData();
        for (WeightId wid : selection)
        {
            normal += normals[wid.ID];
        }
        normal.normalize();
        return normal;
    }

    void MeshGroup::InvertNormals() noexcept
    {
        for (Vector3& normal : Normals)
            normal = -normal;
    }


    void MeshGroup::FlattenFaceData() noexcept
    {
        // Flatten the mesh, so each Triangle Vertex is unique
        auto* meshVerts   = Verts.data();
        auto* meshCoords  = Coords.data();
        auto* meshNormals = Normals.data();
        auto* meshColors  = Colors.data();
        size_t count = Tris.size() * 3u;
        vector<Vector3> verts; verts.reserve(count);
        vector<Vector2> coords;  if (!Coords.empty())   coords.reserve(count);
        vector<Vector3> normals; if (!Normals.empty()) normals.reserve(count);
        vector<Color3>  colors;  if (!Colors.empty())   colors.reserve(count);

        int vertexId = 0, coordId = 0, normalId = 0, colorId = 0;
        for (Triangle& f : Tris)
        {
            for (VertexDescr& vd : f)
            {
                if (vd.v != -1) {
                    verts.push_back(meshVerts[vd.v]);
                    vd.v = vertexId++; // set new vertex Id's on the fly
                }
                if (vd.t != -1) {
                    coords.push_back(meshCoords[vd.t]);
                    vd.t = coordId++;
                }
                if (vd.n != -1) {
                    normals.push_back(meshNormals[vd.n]);
                    vd.n = normalId++;
                }
                if (vd.c != -1) {
                    colors.push_back(meshColors[vd.c]);
                    vd.c = colorId++;
                }
            }
        }
        Verts   = move(verts);
        Coords  = move(coords);
        Normals = move(normals);
        Colors  = move(colors);
        CoordsMapping  = Coords.empty()  ? MapNone : MapPerFaceVertex;
        NormalsMapping = Normals.empty() ? MapNone : MapPerFaceVertex;
        ColorMapping   = Colors.empty()  ? MapNone : MapPerFaceVertex;
    }

    void MeshGroup::SetVertexColor(int vertexId, const Color3& vertexColor) noexcept
    {
        Assert(vertexId < NumVerts(), "Invalid vertexId %d >= numVerts(%d)", vertexId, NumVerts());

        if (Colors.empty()) {
            Colors.resize(Verts.size());
            ColorMapping = MapPerVertex;
        }
        Colors[vertexId] = vertexColor;
    }

    void MeshGroup::AddMeshData(const MeshGroup& group, Vector3 offset) noexcept
    {
        const int numVertsOld   = (int)Verts.size();
        const int numCoordsOld  = (int)Coords.size();
        const int numNormalsOld = (int)Normals.size();
        const int numTrisOld    = (int)Tris.size();

        append(Verts, group.Verts);
        if (offset != Vector3::Zero())
        {
            for (int i = numVertsOld, count = (int)Verts.size(); i < count; ++i)
                Verts[i] += offset;
        }
        append(Coords, group.Coords);
        append(Normals, group.Normals);

        // Colors are optional, but since it's a flatmap, we need to resize as appropriate
        if (!Colors.empty() || !group.Colors.empty())
        {
            if (group.Colors.empty()) {
                Colors.resize(Verts.size());
            }
            else {
                Colors.resize(size_t(numVertsOld));
                append(Colors, group.Colors);
            }
            ColorMapping = MapPerVertex;
        }

        rpp::append(Tris, group.Tris);
        for (int i = numTrisOld, numTris = (int)Tris.size(); i < numTris; ++i)
        {
            Triangle& face = Tris[i];
            for (VertexDescr& vd : face)
            {
                vd.v += numVertsOld;
                if (vd.t != -1) vd.t += numCoordsOld;
                if (vd.n != -1) vd.n += numNormalsOld;
                if (vd.c != -1) vd.c += numVertsOld;
            }
        }
    }

    void MeshGroup::CreateGameVertexData(vector<BasicVertex>& vertices, vector<int>& indices) const noexcept
    {
        auto* meshVerts   = Verts.data();
        auto* meshCoords  = Coords.data();
        auto* meshNormals = Normals.data();

        vertices.reserve(Tris.size() * 3u);
        indices.reserve(Tris.size() * 3u);

        int vertexId = 0;
        const auto addVertex = [&](const VertexDescr& vd)
        {
            indices.push_back(vertexId++);
            vertices.emplace_back<BasicVertex>({
                vd.v != -1 ? meshVerts[vd.v]   : Vector3::Zero(),
                vd.t != -1 ? meshCoords[vd.t]  : Vector2::Zero(),
                vd.n != -1 ? meshNormals[vd.n] : Vector3::Zero()
            });
        };

        for (const Triangle& face : Tris)
            for (const VertexDescr& vd : face)
                addVertex(vd);
    }

    void MeshGroup::SplitSeamVertices() noexcept
    {
        auto canShareVertex = [](const VertexDescr& a, const VertexDescr& b) {
            return a.t == b.t && a.n == b.n && a.c == b.c;
        };

        std::unordered_multimap<int, VertexDescr> addedVerts; addedVerts.reserve(NumVerts());

        auto getExistingVertex = [&](const VertexDescr& old, VertexDescr& out) -> bool
        {
            for (auto r = addedVerts.equal_range(old.v); r.first != r.second; ++r.first)
            {
                VertexDescr existing = r.first->second;
                if (canShareVertex(old, existing)) {
                    out = existing;
                    return true;
                }
            }
            return false;
        };

        size_t numTris  = Tris.size();
        auto*  oldFaces = Tris.data();
        auto*  oldVerts = Verts.data();
        vector<Triangle> faces; faces.resize(numTris);
        vector<Vector3>  verts; verts.reserve(Verts.size());

        for (size_t faceId = 0; faceId < numTris; ++faceId)
        {
            const Triangle& oldFace = oldFaces[faceId];
            Triangle& newFace = faces[faceId];
            for (int i = 0; i < 3; ++i)
            {
                const VertexDescr& old = oldFace[i];
                VertexDescr&    result = newFace[i];

                if (getExistingVertex(old, result))
                    continue;

                // insert new
                verts.push_back(oldVerts[old.v]);
                result = { (int)verts.size() - 1, old.t, old.n, old.c };
                addedVerts.emplace(old.v, result);
            }
        }
        Verts = move(verts);
        Tris = move(faces);
    }

    void MeshGroup::PerVertexFlatten() noexcept
    {
        auto* oldCoords  = Coords.empty()  ? nullptr : Coords.data();
        auto* oldNormals = Normals.empty() ? nullptr : Normals.data();
        auto* oldColors  = Colors.empty()  ? nullptr : Colors.data();
        if (!oldCoords && !oldNormals && !oldColors)
            return; // nothing to do here
        
        vector<Vector2> coords;   coords.reserve(Verts.size());
        vector<Vector3> normals; normals.reserve(Verts.size());
        vector<Color3>  colors;   colors.reserve(Verts.size());

        vector<bool> added; added.resize(Verts.size());

        for (Triangle& face : Tris)
        {
            for (VertexDescr& vd : face)
            {
                int vertexId = vd.v;
                if (!added[vertexId])
                {
                    added[vertexId] = true;
                    if (oldCoords)   coords.push_back(vd.t != -1 ?  oldCoords[vd.t] : Vector2::Zero());
                    if (oldNormals) normals.push_back(vd.n != -1 ? oldNormals[vd.n] : Vector3::Zero());
                    if (oldColors)   colors.push_back(vd.c != -1 ?  oldColors[vd.c] : Color3::Zero());
                }

                if (oldCoords)  vd.t = vertexId;
                if (oldNormals) vd.n = vertexId;
                if (oldColors)  vd.c = vertexId;
            }
        }

        if (CoordsMapping) {
            CoordsMapping = MapPerVertex;
            Coords = move(coords);
            Assert(Coords.size()  == Verts.size(), "Coords must match vertices");
        }
        if (NormalsMapping) {
            NormalsMapping = MapPerVertex;
            Normals = move(normals);
            Assert(Normals.size() == Verts.size(), "Normals must match vertices");
        }
        if (ColorMapping) {
            ColorMapping = MapPerVertex;
            Colors = move(colors);
            Assert(Colors.size()  == Verts.size(), "Colors must match vertices");
        }
    }

    void MeshGroup::OptimizedFlatten() noexcept
    {
        SplitSeamVertices();
        PerVertexFlatten();
    }

    void MeshGroup::CreateIndexArray(vector<int>& indices) const noexcept
    {
        CreateIndexArray(indices, Winding);
    }

    void MeshGroup::CreateIndexArray(vector<unsigned>& indices) const noexcept
    {
        CreateIndexArray(reinterpret_cast<vector<int>&>(indices), Winding);
    }

    void MeshGroup::CreateIndexArray(vector<int>& indices, FaceWinding winding) const noexcept
    {
        indices.clear();
        indices.reserve(Tris.size() * 3u);
        if (Winding == winding)
        {
            for (const Triangle& face : Tris) {
                indices.push_back(face.a.v);
                indices.push_back(face.b.v);
                indices.push_back(face.c.v);
            }
        }
        else // flip the winding, 0 1 2 --> 0 2 1
        {
            for (const Triangle& face : Tris) {
                indices.push_back(face.a.v);
                indices.push_back(face.c.v);
                indices.push_back(face.b.v);
            }
        }
    }

    PickedTriangle MeshGroup::PickTriangle(const Ray& ray) const noexcept
    {
        const Vector3* verts = Verts.data();
        const Triangle* picked = nullptr;
        float closestDist = 9999999999999.0f;

        for (const Triangle& tri : Tris)
        {
            const Vector3& v0 = verts[tri.a.v];
            const Vector3& v1 = verts[tri.b.v];
            const Vector3& v2 = verts[tri.c.v];
            float dist = ray.intersectTriangle(v0, v1, v2);
            if (dist > 0.0f && dist < closestDist) {
                closestDist = dist;
                picked      = &tri;
            }
        }
        return picked ? PickedTriangle{ this, picked, closestDist } : PickedTriangle{};
    }

    void MeshGroup::Print() const
    {
        string_buffer sb;
        sb.writef("   group  %-28s", Name.c_str());
        if (NumVerts())  sb.writef("  %5d verts", NumVerts());
        if (NumTris())   sb.writef("  %5d tris", NumTris());
        if (NumCoords()) sb.writef("  %5d uvs", NumCoords());
        if (NumNormals())sb.writef("  %5d normals", NumNormals());
        if (NumColors()) sb.writef("  %5d colors", NumColors());
        if (Offset != Vector3::Zero()) {
            char buf[48]; sb.writef("  offset:%s", Offset.toString(buf));
        }
        LogInfo("%.*s", sb.size(), sb.data());
    }

    void MeshGroup::PrintVerts(const char* what) const
    {
        if (!what) what = Name.c_str();
        const int numVerts = NumVerts();
        const Vector3* verts = VertexData();
        string_buffer sb;
        sb.writef("%s vertices[%d]:", what, numVerts);
        for (int i = 0; i < numVerts; ++i) {
            const Vector3& v = verts[i];
            sb.writef("\n  [%d] { %.3f, %.3f, %.3f }", i, v.x, v.y, v.z);
        }
        LogInfo("%.*s", sb.size(), sb.data());
    }


    ///////////////////////////////////////////////////////////////////////////////////////////////

    std::string to_string(Options o)
    {
        rpp::string_buffer sb;
        bool prepend = false;
        auto write_flag = [&](bool flag, rpp::strview what) {
            if (!flag) return;
            if (prepend) sb.write('|');
            else         prepend = true;
            sb.write(what);
        };
        write_flag(o & Options::SingleGroup, "SingleGroup");
        write_flag(o & Options::EmptyGroups, "EmptyGroups");
        write_flag(o & Options::NoThrow,     "NoThrow");
        write_flag(o & Options::Log,         "Log");
        write_flag(o & Options::SplitSeams,  "SplitSeams");
        write_flag(o & Options::Flatten,     "Flatten");
        write_flag(o & Options::ClockWise,   "ClockWise");
        write_flag(o & Options::Unity,       "Unity");
        return sb.str();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////


    Mesh::Mesh() = default;
    Mesh::~Mesh() = default;

    Mesh::Mesh(strview meshPath, Options options)
    {
        Load(meshPath, options);
    }

    int Mesh::TotalTris() const
    {
        return rpp::sum_all(Groups, &MeshGroup::NumTris);
    }
    int Mesh::TotalVerts() const
    {
        return rpp::sum_all(Groups, &MeshGroup::NumVerts);
    }
    int Mesh::TotalCoords() const
    {
        return rpp::sum_all(Groups, &MeshGroup::NumCoords);
    }
    int Mesh::TotalNormals() const
    {
        return rpp::sum_all(Groups, &MeshGroup::NumNormals);
    }
    int Mesh::TotalColors() const
    {
        return rpp::sum_all(Groups, &MeshGroup::NumColors);
    }

    MeshGroup* Mesh::FindGroup(strview name)
    {
        for (auto& group : Groups)
            if (group.Name == name) return &group;
        return nullptr;
    }

    const MeshGroup* Mesh::FindGroup(strview name) const
    {
        for (auto& group : Groups)
            if (group.Name == name) return &group;
        return nullptr;
    }

    MeshGroup& Mesh::CreateGroup(string name)
    {
        return rpp::emplace_back(Groups, (int)Groups.size(), name);
    }

    MeshGroup& Mesh::FindOrCreateGroup(strview name)
    {
        if (MeshGroup* group = FindGroup(name))
            return *group;
        return emplace_back(Groups, (int)Groups.size(), name);
    }

    shared_ptr<Material> Mesh::FindMaterial(strview name) const
    {
        for (const MeshGroup& group : Groups)
            if (name.equalsi(group.Name))
                return group.Mat;
        return {};
    }

    bool Mesh::HasAnyMaterials() const
    {
        for (const MeshGroup& group : Groups) if (group.Mat) return true;
        return false;
    }

    void Mesh::Clear() noexcept
    {
        Name.clear();
        Groups.clear();
    }

    Mesh Mesh::Clone(const bool cloneMaterials) const noexcept
    {
        Mesh obj;
        obj.Name     = Name;
        obj.Groups   = Groups;
        if (cloneMaterials) {
            for (auto& group : obj.Groups)
                if (group.Mat)
                    group.Mat = make_shared<Material>(*group.Mat);
        }
        return obj;
    }

    bool Mesh::Load(strview meshPath, Options opt)
    {
        rpp::ScopedPerfTimer perf{ "Nano::Mesh::Load" };

        if (opt & Options::Unity) {
            opt |= Options::SingleGroup | Options::SplitSeams
                |  Options::Flatten     | Options::ClockWise;
        }

        strview ext = file_ext(meshPath);
        if (ext.equalsi("fbx"_sv)) return LoadFBX(meshPath, opt);
        if (ext.equalsi("obj"_sv)) return LoadOBJ(meshPath, opt);
        if (ext.equalsi("txt"_sv)) return LoadTXT(meshPath, opt);
        NanoErr(opt, "Error: unrecognized mesh format for file '%s'", meshPath);
    }

    void Mesh::ApplyLoadOptions(Options opt)
    {
        if (opt & Options::SplitSeams) {
            SplitSeamVertices();
            for (MeshGroup& g : Groups)
                g.SplitSeamVertices();
        }
        if (opt & Options::Flatten) {
            OptimizedFlatten();
        }

        FaceWinding winding = (opt & Options::ClockWise) ? FaceWinding::CW
                                                         : FaceWinding::CCW;
        SetFaceWinding(winding);

        if (opt & Options::Unity) {
            SetCoordSys(CoordSys::Unity);
        }

        if (opt & Options::Log) {
            for (MeshGroup& g : Groups) {
                g.Print();
            }
            if (!(opt & Options::SingleGroup)) {
                LogInfo("Loaded %-31s  %5d verts  %5d tris",
                    Name, TotalVerts(), TotalTris());
            }
        }
    }

    bool Mesh::SaveAs(strview meshPath, Options opt) const
    {
        rpp::ScopedPerfTimer perf{ "Nano::Mesh::SaveAs" };
        strview ext = file_ext(meshPath);
        if (ext.equalsi("fbx"_sv)) return SaveAsFBX(meshPath, opt);
        if (ext.equalsi("obj"_sv)) return SaveAsOBJ(meshPath, opt);

        NanoErr(opt, "Error: unrecognized mesh format for file '%s'", meshPath);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    void Mesh::RecalculateNormals(const bool checkDuplicateVerts) noexcept
    {
        for (MeshGroup& group : Groups)
            group.RecalculateNormals(checkDuplicateVerts);
    }

    void Mesh::InvertNormals() noexcept
    {
        for (MeshGroup& group : Groups)
            group.InvertNormals();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    void Mesh::AddMeshData(const Mesh& mesh, Vector3 offset) noexcept
    {
        size_t numGroupsOld  = Groups.size();
        rpp::append(Groups, mesh.Groups);

        auto oldGroupHasIdenticalName = [=](strview name) {
            for (size_t i = 0; i < numGroupsOld; ++i)
                if (Groups[i].Name == name) return true;
            return false;
        };

        for (size_t i = numGroupsOld; i < Groups.size(); ++i)
        {
            MeshGroup& group = Groups[i];
            while (oldGroupHasIdenticalName(group.Name))
                group.Name += "_" + std::to_string(numGroupsOld);

            if (offset != Vector3::Zero()) {
                for (Vector3& vertex : group.Verts)
                    vertex += offset;
            }
        }
    }

    BoundingBox Mesh::CalculateBBox() const noexcept
    {
        if (Groups.empty())
            return {};
        BoundingBox bounds = BoundingBox::create(Groups.front().Verts);
        for (size_t i = 1; i < Groups.size(); ++i)
            bounds.join(BoundingBox::create(Groups[i].Verts));
        return bounds;
    }

    void Mesh::SplitSeamVertices() noexcept
    {
        for (MeshGroup& g : Groups)
            g.SplitSeamVertices();
    }

    void Mesh::FlattenMeshData() noexcept
    {
        for (MeshGroup& group : Groups)
            if (!group.IsFlattened()) group.FlattenFaceData();
    }

    bool Mesh::IsFlattened() const noexcept
    {
        for (const MeshGroup& group : Groups)
            if (!group.IsFlattened()) return false;
        return true;
    }

    void Mesh::OptimizedFlatten() noexcept
    {
        for (MeshGroup& group : Groups)
            group.OptimizedFlatten();
    }

    void Mesh::SetFaceWinding(FaceWinding winding) noexcept
    {
        for (MeshGroup& g : Groups) {
            g.SetFaceWinding(winding);
        }
    }

    void Mesh::SetCoordSys(CoordSys targetSystem) noexcept
    {
        for (MeshGroup& g : Groups) {
            g.SetCoordSys(targetSystem);
        }
    }

    void Mesh::MergeGroups() noexcept
    {
        if (Groups.size() <= 1u)
            return;

        auto& merged = Groups.front();
        while (Groups.size() > 1)
        {
            merged.AddMeshData(Groups.back());
            Groups.pop_back();
        }
    }

    PickedTriangle Mesh::PickTriangle(const Ray& ray) const noexcept
    {
        PickedTriangle closest = {};
        for (const MeshGroup& group : Groups)
        {
            if (PickedTriangle result = group.PickTriangle(ray)) {
                if (!closest || result.distance < closest.distance)
                    closest = result;
            }
        }
        return closest;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
}

