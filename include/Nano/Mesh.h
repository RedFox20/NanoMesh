#pragma once
#include <rpp/vec.h>
#include <rpp/collections.h>
#include <memory>

/**
 * Enables FBX mesh loader for platforms that support it
 */
#if !NANOMESH_NO_FBX && !defined(ENABLE_FBX_MESH_LOADER)
#  define ENABLE_FBX_MESH_LOADER (_WIN32)
#endif

#ifndef NANOMESH_API
#  if _MSC_VER
#    define NANOMESH_API __declspec(dllexport)
#  else // clang/gcc
#    define NANOMESH_API __attribute__((visibility("default")))
#  endif
#endif

#if _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4251) // Nano::Material::Name needs to have dll-interface
#endif

namespace Nano
{
    using std::string;
    using std::vector;
    using std::shared_ptr;
    
    using rpp::strview;
    using rpp::Vector2;
    using rpp::Vector3;
    using rpp::Color3;
    using rpp::BoundingBox;
    using rpp::IdVector3;
    using rpp::Ray;
    //////////////////////////////////////////////////////////////////////


    struct NANOMESH_API VertexDescr
    {
        int v = -1; // vertex position index (vertexId)
        int t = -1; // vertex texture index (can be -1, aka no UV info)
        int n = -1; // vertex normal index (can be -1, aka no normal info)
        int c = -1; // vertex color index (can be -1, aka no color info)

        bool operator==(const VertexDescr& vd) const;
        bool operator!=(const VertexDescr& vd) const;
    };


    struct NANOMESH_API Triangle
    {
        VertexDescr a, b, c;

        VertexDescr&       operator[](int index)       { return (&a)[index]; }
        const VertexDescr& operator[](int index) const { return (&a)[index]; }
        const VertexDescr* begin() const { return &a; }
        const VertexDescr* end()   const { return &c + 1; }
        VertexDescr* begin() { return &a; }
        VertexDescr* end()   { return &c + 1; }

        bool ContainsVertexId(int vertexId) const;

        bool operator==(const Triangle& t) const;
        bool operator!=(const Triangle& t) const;
    };

    NANOMESH_API string to_string(const Triangle& triangle);


    struct NANOMESH_API Material
    {
        string Name; // name of the material instance
        string MaterialFile; // eg 'default.mtl'

        string DiffusePath;
        string AlphaPath;
        string SpecularPath;
        string NormalPath;
        string EmissivePath;

        Color3 AmbientColor  = Color3::White();
        Color3 DiffuseColor  = Color3::White();
        Color3 SpecularColor = Color3::White();
        Color3 EmissiveColor = Color3::Black();

        float Specular = 1.0f;
        float Alpha    = 1.0f;
    };


    enum MapMode
    {
        // this meshgroup element is not mapped
        MapNone,

        // extra data is mapped per vertex, this means:
        // colors are mapped for every vertex
        // normals are mapped for every vertex
        // coords are  mapped for every vertex, so UV shells must be contiguous
        MapPerVertex, 

        // extra data is mapped per each face vertex; data can still be shared, but this allows
        // discontiguous submesh data, which is very common
        // colors are mapped for every face vertex, this is quite rare
        // normals are mapped for every face vertex, this is common if you have submeshes with split smoothing groups
        // coords mapped this way can have discontiguous UV shells, which is VERY common
        MapPerFaceVertex,

        // extra data is mapped per face, this is very rare
        // colors are mapped for every face
        // normals are mapped for every face
        // coords are NEVER mapped this way
        MapPerFace,

        // extra data is mapped inconsistently and not suitable for direct Array-of-Structures mapping to graphics hardware
        // shared element mapping is a good way to save on file size, but makes mesh modification difficult
        // MeshGroup::OptimizedFlatten() should be called to enable mesh editing
        MapSharedElements,
    };


    struct MeshGroup;

    struct NANOMESH_API PickedTriangle
    {
        // @warning These pointers will invalidate if you modify the mesh!!
        const MeshGroup* group = nullptr;
        const Triangle*  face  = nullptr;
        float distance = 0.0f;
        bool good() const { return group && face && distance != 0.0f; }
        explicit operator bool() const { return good(); }

        // center of the triangle
        Vector3 center() const;

        // accesses group to retrieve the Vector3 position associated with VertexDescr
        Vector3 vertex(const VertexDescr& vd) const;

        // triangle id in this group
        int id() const;
    };


    NANOMESH_API string to_string(const PickedTriangle& triangle);


    // Common 3D mesh vertex for games, as generic as it can get
    struct BasicVertex
    {
        Vector3 pos;
        Vector2 uv;
        Vector3 norm;
    };


    enum FaceWindOrder
    {
        FaceWindClockWise,
        FaceWindCounterClockWise,
    };

    
    struct WeightId
    {
        int ID; // vertex id, -1 means invalid Vertex ID, [0] based indices
        float Weight;
        WeightId(int id, float weight) : ID(id), Weight(weight) {}

        bool operator==(const WeightId& v) const { return ID == v.ID; }
        bool operator!=(const WeightId& v) const { return ID != v.ID; }
    };


    struct NANOMESH_API MeshGroup
    {
        int GroupId = -1;
        string Name; // name of the suboject
        shared_ptr<Material> Mat;

        Vector3 Offset   = Vector3::Zero();
        Vector3 Rotation = Vector3::Zero(); // XYZ Euler DEGREES
        Vector3 Scale    = Vector3::One();

        // we treat mesh data as 'layers', so everything except Verts is optional
        vector<Vector3> Verts;
        vector<Vector2> Coords;
        vector<Vector3> Normals;
        vector<Color3>  Colors;

        vector<Triangle> Tris; // face descriptors (tris and/or quads)

        MapMode CoordsMapping  = MapNone;
        MapMode NormalsMapping = MapNone;
        MapMode ColorMapping   = MapNone;

        FaceWindOrder Winding = FaceWindClockWise;

        MeshGroup(int groupId, string name)
            : GroupId(groupId), Name(std::move(name)) {}

        bool IsEmpty()   const { return Tris.empty(); }
        int NumTris()    const { return (int)Tris.size(); }
        int NumVerts()   const { return (int)Verts.size(); }
        int NumCoords()  const { return (int)Coords.size(); }
        int NumNormals() const { return (int)Normals.size(); }
        int NumColors()  const { return (int)Colors.size(); }
        Vector3* VertexData() { return Verts.data();   }
        Vector2* CoordData()  { return Coords.data();  }
        Vector3* NormalData() { return Normals.data(); }
        Color3*  ColorData()  { return Colors.data();  }
        const Vector3* VertexData() const { return Verts.data();   }
        const Vector2* CoordData()  const { return Coords.data();  }
        const Vector3* NormalData() const { return Normals.data(); }
        const Color3*  ColorData()  const { return Colors.data();  }

        const Vector3& Vertex(int vertexId)          const { return Verts.data()[vertexId]; }
        const Vector3& Vertex(const VertexDescr& vd) const { return Verts.data()[vd.v]; }

        const Triangle* begin() const { return &Tris.front(); }
        const Triangle* end()   const { return &Tris.back() + 1; }
        Triangle* begin() { return &Tris.front(); }
        Triangle* end()   { return &Tris.back() + 1; }

        // creates and assigns a new material to this mesh group
        Material& CreateMaterial(string name);

        // will flip the face winding from CW to CCW or from CCW to CW
        void InvertFaceWindingOrder();

        bool IsFlattened() const noexcept
        {
            return CoordsMapping  == MapPerFaceVertex
                && NormalsMapping == MapPerFaceVertex
                && ColorMapping   == MapPerFaceVertex;
        }

        void UpdateNormal(const VertexDescr& vd0,
                          const VertexDescr& vd1,
                          const VertexDescr& vd2,
                          const bool checkDuplicateVerts = false) noexcept;

        // Recalculates all normals by find shared and non-shared vertices on the same pos
        // Currently does not respect smoothing groups
        // @param checkDuplicateVerts Will perform an O(n^2) search for duplicate vertices to
        //                            correctly calculate normals for mesh surfaces with unwelded verts
        void RecalculateNormals(const bool checkDuplicateVerts = false) noexcept;

        // Retrieves surface normals from selection
        // @note The mesh must have PerVertex normals!
        Vector3 GetNormalForSelection(const vector<WeightId>& selection) const noexcept;

        // normal = -normal;
        void InvertNormals() noexcept;

        void SetVertexColor(int vertexId, const Color3& vertexColor) noexcept;

        // Flattens all mesh data, so MapMode is MapPerFaceVertex
        // This will make the mesh data compatible with any 3D graphics engine out there
        // However, mesh data will be thus stored less efficiently (no vertex data sharing)
        // 
        // Verts, Coords, Normals and Colors will all be stored in a linear sequence
        // with equal length, so creating a corresponding vertex/index array is trivial
        //
        void FlattenFaceData() noexcept;

        // Adds additional meshgroups from another Mesh
        // Optionally appends an extra offset to position vertices
        void AddMeshData(const MeshGroup& group, Vector3 offset = Vector3::Zero()) noexcept;

        // Gets a basic vertex mesh representation which can be used safely in most games,
        // because the vertices are safely flattened with optimal vertex sharing
        // @note If you called FlattenMeshData() before this, then optimal vertex sharing is not possible
        void CreateGameVertexData(vector<BasicVertex>& vertices, vector<int>& indices) const noexcept;

        // splits vertices that share an UV seam - this is required for non-contiguos UV support
        void SplitSeamVertices() noexcept;

        // converts coords, normals, colors to MapPerVertex
        void PerVertexFlatten() noexcept;

        // Optimally flattens this mesh by using:
        // SplitSeamVertices() && PerVertexFlatten()
        void OptimizedFlatten() noexcept;

        void CreateIndexArray(vector<int> &indices) const noexcept;
        void CreateIndexArray(vector<unsigned int> &indices) const noexcept;

        // Pick the closest face that intersects with the ray
        PickedTriangle PickTriangle(const Ray& ray) const noexcept;

        BoundingBox CalculateBBox() const noexcept {
            return BoundingBox::create(Verts);
        }
        BoundingBox CalculateBBox(const vector<IdVector3>& deltas) const noexcept {
            return BoundingBox::create(Verts, deltas);
        }

        // prints group info to stdout
        void Print() const;
    };


    //////////////////////////////////////////////////////////////////////

    /**
     * Convenient mesh load options
     * Usage:
     *     Nano::Mesh mesh { "mesh.obj", Options::NoThrow | Options::LogGroups };
     */
    struct NANOMESH_API Options
    {
        /**
         * LOAD:
         * If true, then all named meshgroups will be ignored
         * and all verts/faces will be put into the first object group instead
         * @note This will break multi-material support, so only use this if
         *       you have 1 or 0 materials.
         */
        bool ForceSingleGroup = false;

        /**
         * LOAD:
         * If true, empty groups will not be discarded and will
         * be treated as metadata instead.
         * Check MeshGroup::Offset for position meta
         */
        bool CreateEmptyGroups = false;

        /**
         * LOAD+SAVE:
         * if TRUE  Mesh Load/Save throws an exception during failure
         * if FALSE Mesh Load/Save logs error and returns false
         */
        bool NoExceptions = false;

        /**
         * LOAD+SAVE:
         * Log mesh group stats during load/save
         */
        bool LogMeshGroupInfo = false;

        /**
         * LOAD:
         * Split non-contiguous UV shell vertices.
         * This MAY increase vertex count, but if UV's are contiguous 
         * then vertexcount+order will remain the same.
         * needed in game engines which use Array-Of-Structs:
         * `struct Vertex { vec3 pos; vec3 norm; vec2 uv; };`
         */
        bool SplitUVSeams = false;

        /**
         * LOAD:
         * Flatten Normals and UV's to match vertex count
         */
        int PerVertexFlatten  = false;

        static const Options SingleGroup; // ForceSingleGroup
        static const Options EmptyGroups; // CreateEmptyGroups
        static const Options NoThrow;     // NoExceptions
        static const Options LogGroups;   // LogMeshGroupInfo
        static const Options SplitSeams;  // SplitUVSeams
        static const Options Flatten;     // PerVertexFlatten

        Options operator|(const Options& o) const;
    };

    NANOMESH_API std::string to_string(const Options& o);

    /**
     * Load/Save errors
     */
    struct NANOMESH_API MeshIOError : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    //////////////////////////////////////////////////////////////////////


    /**
     * Mesh coordinate system is the OPENGL coordinate system
     * +X is Right on the screen, +Y is Up, +Z is INTO the screen
     * 
     * @warning All imported meshes are TRIANGULATED.
     *          Game engines work with triangles only.
     *          
     * @warning Mesh data is separated by groups, which can lead
     *          to bigger vertex count.
     *          Game engines can't handle vertex sharing between groups.
     *          You can load mesh with `Nano::Options::SingleGroup` to work around this.
     * 
     * @warning Only one material is allowed per group. 
     *          Game mesh VBO's can only be drawn with a single shader.
     */
    class NANOMESH_API Mesh
    {
    public:
        // These are intentionally public to allow custom mesh manipulation
        string Name;
        vector<MeshGroup> Groups;
        
        // Default empty mesh
        Mesh();
        
        // Automatically constructs a new mesh, check good() or cast to bool to check if successful
        explicit Mesh(strview meshPath, Options options = {});

        ~Mesh();

        int TotalTris() const;
        int TotalVerts() const;
        int TotalCoords() const;
        int TotalNormals() const;
        int TotalColors() const;


        bool good() const { return !Groups.empty(); }
        explicit operator bool() const { return  good(); }
        bool operator!()         const { return !good(); }


        MeshGroup* FindGroup(strview name);
        const MeshGroup* FindGroup(strview name) const;
        MeshGroup& CreateGroup(string name);
        MeshGroup& FindOrCreateGroup(strview name);

        shared_ptr<Material> FindMaterial(strview name) const;
        bool HasAnyMaterials() const;

        int NumGroups() const { return (int)Groups.size();  }
        bool IsValidGroup(int groupId) const { return (size_t)groupId < Groups.size(); }


        MeshGroup* begin()               { return Groups.data(); }
        MeshGroup* end()                 { return Groups.data() + Groups.size(); }
        MeshGroup& Default()             { return Groups.front(); }
        MeshGroup& operator[](int index) { return Groups[index]; }

        const MeshGroup* begin()               const { return Groups.data(); }
        const MeshGroup* end()                 const { return Groups.data() + Groups.size(); }
        const MeshGroup& Default()             const { return Groups.front(); }
        const MeshGroup& operator[](int index) const { return Groups[index]; }


        Mesh(Mesh&& o) noexcept = default; // Allow MOVE
        Mesh(const Mesh&) noexcept = delete; // NOCOPY, call Clone() manually plz
        Mesh& operator=(Mesh&& o) = default;
        Mesh& operator=(const Mesh&) noexcept = delete;


        void Clear() noexcept;


        // Create a clone of this 3D Mesh on demand. No automatic copy operators allowed.
        // @param cloneMaterials Will also clone the material references
        Mesh Clone(const bool cloneMaterials = false) const noexcept;

        /**
         * Attempts to load this mesh.
         * @note This will only throw if Options::NoExceptions is true
         * @return If !NoExceptions, returns TRUE on SUCCESS
         */
        bool Load(strview meshPath, Options opt = {});
        
    private:
        void ApplyLoadOptions(const Options& opt);

    public:
        bool SaveAs(strview meshPath, Options opt = {}) const;

        // Is FBX supported on this platform?
        static bool IsFBXSupported() noexcept;
        bool LoadFBX(strview meshPath, Options opt = {});
        bool LoadOBJ(strview meshPath, Options opt = {});
        
        /**
         * A simple custom mesh text format, which is similar to OBJ
         * Look at NanoMesh/bin/.txt for examples
         * @note This is designed for testing purposes
         */
        bool LoadTXT(strview meshPath, Options opt = {});

        bool SaveAsFBX(strview meshPath, Options opt = {}) const;
        bool SaveAsOBJ(strview meshPath, Options opt = {}) const;
        // bool SaveAsTxt(strview meshPath, Options opt = {}) const;

        // Recalculates all normals by find shared and non-shared vertices on the same pos
        // Currently does not respect smoothing groups
        // @param checkDuplicateVerts Will perform an O(n^2) search for duplicate vertices to
        //                            correctly calculate normals for mesh surfaces with unwelded verts
        void RecalculateNormals(const bool checkDuplicateVerts = false) noexcept;

        // normal = -normal;
        void InvertNormals() noexcept;

        BoundingBox CalculateBBox() const noexcept;

        // Adds additional meshgroups from another Mesh
        // Optionally appends an extra offset to position vertices
        void AddMeshData(const Mesh& mesh, Vector3 offset = Vector3::Zero()) noexcept;

        // Flattens all mesh data, so MapMode is MapPerFaceVertex
        // This will make the mesh data compatible with any 3D graphics engine out there
        // However, mesh data will be thus stored less efficiently (no vertex data sharing)
        // 
        // Verts, Coords, Normals and Colors will all be stored in a linear sequence
        // with equal length, so creating a corresponding vertex/index array is trivial
        //
        void FlattenMeshData() noexcept;
        bool IsFlattened() const noexcept;

        // Optimized flatten
        void OptimizedFlatten() noexcept;

        // Merges all imported groups into a single group
        void MergeGroups() noexcept;

        // Pick the closest face that intersects with the ray
        PickedTriangle PickTriangle(const Ray& ray) const noexcept;
    };

    //////////////////////////////////////////////////////////////////////
}

#if _MSC_VER
#  pragma warning(pop) // pop dll-interface warning
#endif