#include <Nano/Mesh.h>
#include <rpp/file_io.h>
#include <rpp/sprint.h>
#include <rpp/timer.h>
#include <cstdlib>
#include "InternalConfig.h"

namespace Nano
{
    using namespace rpp::literals;
    ///////////////////////////////////////////////////////////////////////////////////////////////

    static int SkipAndParse(rpp::strview line)
    {
        line.next(' ');
        return line.to_int();
    }

    template<class T> T* ExpandArray(std::vector<T>& elements, int newElements)
    {
        size_t oldSize = elements.size();
        elements.resize(oldSize + newElements);
        return elements.data() + oldSize;
    }

    static void ParseVerts(MeshGroup& g, rpp::strview line, rpp::buffer_line_parser& parser)
    {
        int numVerts = SkipAndParse(line);
        rpp::Vector3* verts = ExpandArray(g.Verts, numVerts);
        for (int i = 0; i < numVerts && parser.read_line(line); ++i) {
            rpp::Vector3& v = verts[i];
            line >> v.x >> v.y >> v.z;
        }
    }

    static void ParseCoords(MeshGroup& g, rpp::strview line, rpp::buffer_line_parser& parser)
    {
        int numCoords = SkipAndParse(line);
        rpp::Vector2* coords = ExpandArray(g.Coords, numCoords);
        for (int i = 0; i < numCoords && parser.read_line(line); ++i) {
            rpp::Vector2& c = coords[i];
            line >> c.x >> c.y;
        }
    }

    static void ParseNormals(MeshGroup& g, rpp::strview line, rpp::buffer_line_parser& parser)
    {
        int numNormals = SkipAndParse(line);
        rpp::Vector3* normals = ExpandArray(g.Normals, numNormals);
        for (int i = 0; i < numNormals && parser.read_line(line); ++i) {
            rpp::Vector3& n = normals[i];
            line >> n.x >> n.y >> n.z;
        }
    }

    static void ParsePolys(MeshGroup& g, rpp::strview line, rpp::buffer_line_parser& parser)
    {
        int numPolys = SkipAndParse(line);
        g.Tris.reserve(g.Tris.size() + (numPolys * 2)); // assume we are getting 4-point polys

        auto parseDescr = [&](VertexDescr& vd, rpp::strview vertdescr) {
            if (rpp::strview v = vertdescr.next('/')) {
                vd.v = v.to_int() - 1;
            }
            if (rpp::strview t = vertdescr.next('/')) {
                vd.t = t.to_int() - 1;
            }
            if (rpp::strview n = vertdescr) {
                vd.n = n.to_int() - 1;
            }
        };

        for (int i = 0; i < numPolys && parser.read_line(line); ++i)
        {
            Triangle* t = &rpp::emplace_back(g.Tris);

            // parse first triangle
            parseDescr(t->a, line.next(' '));
            parseDescr(t->b, line.next(' '));
            parseDescr(t->c, line.next(' '));

            // when encountering quads or large polygons, we need to triangulate the mesh
            // by tracking the first vertex descr and forming a fan; this requires convex polys
            while (rpp::strview vertdescr = line.next(' '))
            {
                // face vertices are in CCW order:
                // 0--3
                // |\ |
                // | \|
                // 1--2
                // v[0], v[2], v[3]
                VertexDescr vd0 = t->a; // by value, because emplace_back may realloc
                VertexDescr vd2 = t->c;
                t = &rpp::emplace_back(g.Tris);
                t->a = vd0;
                t->b = vd2;
                parseDescr(t->c, vertdescr);
            }
        }
    }

    static void BuildGroup(MeshGroup& g)
    {
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
    }

    static void BuildGroups(Mesh& mesh)
    {
        for (MeshGroup& g : mesh.Groups)
        {
            BuildGroup(g);
            // our TXT default face winding is CCW
            g.Winding = FaceWinding::CCW;
        }
    }

    bool Mesh::LoadTXT(rpp::strview meshPath, Options opt)
    {
        Clear();

        auto parser = rpp::buffer_line_parser::from_file(meshPath);
        if (!parser) {
            NanoErr(opt, "Failed to open file: %s", meshPath);
        }

        if (opt & Options::Log) {
            LogInfo("Load %s", file_nameext(meshPath));
        }

        MeshGroup* g = nullptr;
        #define CheckGroup(name) if (!g) { \
            NanoErr(opt, "No previous 'mesh' declaration found before '%s': %s", name, meshPath); \
            return false; \
        }

        rpp::strview line;
        while (parser.read_line(line))
        {
            if (line.starts_with("mesh"_sv)) {
                bool ignoreGroup = (opt & Options::SingleGroup) && g;
                if (!ignoreGroup) {
                    line.next(' ');
                    g = &FindOrCreateGroup(line);
                }
            }
            else if (line.starts_with("verts"_sv)) {
                CheckGroup("verts");
                ParseVerts(*g, line, parser);
            }
            else if (line.starts_with("coords"_sv)) {
                CheckGroup("coords");
                ParseCoords(*g, line, parser);
            }
            else if (line.starts_with("normals"_sv)) {
                CheckGroup("normals");
                ParseNormals(*g, line, parser);
            }
            else if (line.starts_with("polys"_sv)) {
                CheckGroup("polys");
                ParsePolys(*g, line, parser);
            }
        }
        
        BuildGroups(*this);
        ApplyLoadOptions(opt);
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
}
