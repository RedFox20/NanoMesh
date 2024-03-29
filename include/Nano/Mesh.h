#pragma once
#include <rpp/vec.h>
#include <rpp/collections.h>
#include <memory>
#include <stdexcept>

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

        VertexDescr&       operator[](int index) { return (&a)[index]; }
        const VertexDescr& operator[](int index) const { return (&a)[index]; }
        const VertexDescr* begin() const { return &a; }
        const VertexDescr* end()   const { return &c + 1; }
        VertexDescr* begin() { return &a; }
        VertexDescr* end() { return &c + 1; }

        bool ContainsVertexId(int vertexId) const;

        bool operator==(const Triangle& t) const;
        bool operator!=(const Triangle& t) const;
    };

    NANOMESH_API std::string to_string(const Triangle& triangle);


    struct NANOMESH_API Material
    {
        std::string Name; // name of the material instance

        std::string DiffusePath;
        std::string AlphaPath;
        std::string SpecularPath;
        std::string NormalPath;
        std::string EmissivePath;

        rpp::Color3 AmbientColor = rpp::Color3::White();
        rpp::Color3 DiffuseColor = rpp::Color3::White();
        rpp::Color3 SpecularColor = rpp::Color3::White();
        rpp::Color3 EmissiveColor = rpp::Color3::Black();

        float Specular = 1.0f;
        float Alpha = 1.0f;
    };


    enum class MapMode
    {
        // this mesh group element is not mapped
        None,

        // extra data is mapped per vertex, this means:
        // colors are mapped for every vertex
        // normals are mapped for every vertex
        // coords are  mapped for every vertex, so UV shells must be contiguous
        PerVertex,

        // extra data is mapped per each face vertex; data can still be shared, but this allows
        // discontiguous submesh data, which is very common
        // colors are mapped for every face vertex, this is quite rare
        // normals are mapped for every face vertex, this is common if you have submeshes with split smoothing groups
        // coords mapped this way can have discontiguous UV shells, which is VERY common
        PerFaceVertex,

        // extra data is mapped per face, this is very rare
        // colors are mapped for every face
        // normals are mapped for every face
        // coords are NEVER mapped this way
        PerFace,

        // extra data is mapped inconsistently and not suitable for direct Array-of-Structures mapping to graphics hardware
        // shared element mapping is a good way to save on file size, but makes mesh modification difficult
        // MeshGroup::OptimizedFlatten() should be called to enable mesh editing
        SharedElements,
    };


    struct MeshGroup;

    struct NANOMESH_API PickedTriangle
    {
        // @warning These pointers will invalidate if you modify the mesh!!
        const MeshGroup* group = nullptr;
        const Triangle*  face = nullptr;
        float distance = 0.0f;
        bool good() const { return group && face && distance != 0.0f; }
        explicit operator bool() const { return good(); }

        // center of the triangle
        rpp::Vector3 center() const;

        // accesses group to retrieve the Vector3 position associated with VertexDescr
        rpp::Vector3 vertex(const VertexDescr& vd) const;

        // triangle id in this group
        int id() const;
    };


    NANOMESH_API std::string to_string(const PickedTriangle& triangle);


    // Common 3D mesh vertex for games, as generic as it can get
    struct BasicVertex
    {
        rpp::Vector3 pos;
        rpp::Vector2 uv;
        rpp::Vector3 norm;
    };


    enum class FaceWinding
    {
        CW, // ClockWise face winding
        CCW, // Counter-ClockWise face winding, default for OBJ loader
    };

    enum class CoordSys
    {
        GL,    // default for OBJ loader
        Unity, // you can convert to Unity coordsys using MeshGroup::ConvertCoords()
    };


    struct WeightId
    {
        int ID; // vertex id, -1 means invalid Vertex ID, [0] based indices
        float Weight;
        WeightId(int id, float weight) : ID(id), Weight(weight) {}

        bool operator==(const WeightId& v) const { return ID == v.ID; }
        bool operator!=(const WeightId& v) const { return ID != v.ID; }
    };

    struct NANOMESH_API BonePose
    {
        rpp::Vector3 Translation;
        rpp::Vector3 Rotation; // @note XYZ Rotation in DEGREES
        rpp::Vector3 Scale;
    };

    struct NANOMESH_API MeshBone
    {
        int BoneIndex   = 0; // this index in the Bones array
        int ParentIndex = 0; // parent bone index in the Bones array
        std::string Name;
        BonePose Pose {};
    };

    struct NANOMESH_API SkinnedBone
    {
        int BoneIndex   = 0; // this index in the SkinnedBones array
        int ParentIndex = 0; // parent bone index in the SkinnedBones array
        std::string Name;
        BonePose Pose {};
        rpp::Matrix4 InverseBindPoseTransform; // TODO
    };

    struct NANOMESH_API AnimationKeyFrame
    {
        float Time; // time in seconds for this keyframe
        BonePose Pose; // pose of the bone at time
    };

    struct NANOMESH_API BoneAnimation
    {
        int SkinnedBoneIndex = 0; // index of SkinnedBone
        std::vector<AnimationKeyFrame> Frames; // animation keyframes for this bone
    };

    struct NANOMESH_API AnimationClip
    {
        std::string Name; // unique name identifier of this animation clip
        float Duration = 0; // duration of this animation clip
        std::vector<BoneAnimation> Animations; // list of bone animations
    };

    // Up to 4 bone indices per vertex
    struct NANOMESH_API BlendIndices
    {
        unsigned char indices[4];
    };

    // Maps up to 4 bone weights per vertex
    struct NANOMESH_API BlendWeights
    {
        rpp::Vector4 weights;
    };

    struct NANOMESH_API MeshGroup
    {
        int GroupId = -1;
        std::string Name; // name of the suboject
        std::shared_ptr<Material> Mat;

        rpp::Vector3 Offset   = rpp::Vector3::Zero();
        rpp::Vector3 Rotation = rpp::Vector3::Zero(); // XYZ Euler DEGREES
        rpp::Vector3 Scale    = rpp::Vector3::One();

        // we treat mesh data as 'layers', so everything except Verts is optional
        std::vector<rpp::Vector3> Verts;
        std::vector<rpp::Vector2> Coords;
        std::vector<rpp::Vector3> Normals;
        std::vector<rpp::Color3>  Colors;
        std::vector<rpp::Vector4> Weights;
        std::vector<Nano::BlendIndices> BlendIndices;
        std::vector<Nano::BlendWeights> BlendWeights;

        std::vector<Triangle> Tris; // face descriptors (tris and/or quads)

        MapMode CoordsMapping  = MapMode::None;
        MapMode NormalsMapping = MapMode::None;
        MapMode ColorMapping   = MapMode::None;
        MapMode BlendMapping   = MapMode::None; // Only Per-Vertex supported

        FaceWinding   Winding  = FaceWinding::CW;
        CoordSys      System   = CoordSys::GL;

        MeshGroup(int groupId, std::string name)
            : GroupId(groupId), Name(std::move(name)) {}

        bool IsEmpty()   const { return Tris.empty(); }
        int NumTris()    const { return (int)Tris.size(); }
        int NumVerts()   const { return (int)Verts.size(); }
        int NumCoords()  const { return (int)Coords.size(); }
        int NumNormals() const { return (int)Normals.size(); }
        int NumColors()  const { return (int)Colors.size(); }
        int NumBlendIndices() const { return (int)BlendIndices.size(); }
        int NumBlendWeights() const { return (int)BlendWeights.size(); }
        rpp::Vector3* VertexData() { return Verts.data();   }
        rpp::Vector2* CoordData()  { return Coords.data();  }
        rpp::Vector3* NormalData() { return Normals.data(); }
        rpp::Color3*  ColorData()  { return Colors.data();  }
        Nano::BlendIndices*  BlendIndexData()  { return BlendIndices.data(); }
        Nano::BlendWeights*  BlendWeightData() { return BlendWeights.data(); }
        const rpp::Vector3* VertexData() const { return Verts.data(); }
        const rpp::Vector2* CoordData()  const { return Coords.data(); }
        const rpp::Vector3* NormalData() const { return Normals.data(); }
        const rpp::Color3*  ColorData()  const { return Colors.data(); }
        const Nano::BlendIndices*  BlendIndexData()  const { return BlendIndices.data(); }
        const Nano::BlendWeights*  BlendWeightData() const { return BlendWeights.data(); }

        const rpp::Vector3& Vertex(int vertexId)          const { return Verts.data()[vertexId]; }
        const rpp::Vector3& Vertex(const VertexDescr& vd) const { return Verts.data()[vd.v]; }

        const Triangle* begin() const { return &Tris.front(); }
        const Triangle* end()   const { return &Tris.back() + 1; }
        Triangle* begin() { return &Tris.front(); }
        Triangle* end() { return &Tris.back() + 1; }

        // this clears all layers and resets mapping modes
        void Clear();

        // creates and assigns a new material to this mesh group
        Material& CreateMaterial(std::string name);

        // will set the face winding to CW or CCW
        void SetFaceWinding(FaceWinding winding) noexcept;

        void SetCoordSys(CoordSys targetSystem) noexcept;

        bool IsFlattened() const noexcept
        {
            return CoordsMapping == MapMode::PerFaceVertex
                && NormalsMapping == MapMode::PerFaceVertex
                && ColorMapping == MapMode::PerFaceVertex;
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
        rpp::Vector3 GetNormalForSelection(const std::vector<WeightId>& selection) const noexcept;

        // normal = -normal;
        void InvertNormals() noexcept;

        void SetVertexColor(int vertexId, const rpp::Color3& vertexColor) noexcept;

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
        void AddMeshData(const MeshGroup& group, rpp::Vector3 offset =rpp:: Vector3::Zero()) noexcept;

        // Gets a basic vertex mesh representation which can be used safely in most games,
        // because the vertices are safely flattened with optimal vertex sharing
        // @note If you called FlattenMeshData() before this, then optimal vertex sharing is not possible
        void CreateGameVertexData(std::vector<BasicVertex>& vertices, std::vector<int>& indices) const noexcept;

        // splits vertices that share an UV seam - this is required for non-contiguos UV support
        void SplitSeamVertices() noexcept;

        // converts coords, normals, colors to MapPerVertex
        void PerVertexFlatten() noexcept;

        // Optimally flattens this mesh by using:
        // SplitSeamVertices() && PerVertexFlatten()
        void OptimizedFlatten() noexcept;

        void CreateIndexArray(std::vector<int>& indices) const noexcept;
        void CreateIndexArray(std::vector<short>& indices) const noexcept;
        void CreateIndexArray(std::vector<unsigned int>& indices) const noexcept;
        void CreateIndexArray(std::vector<unsigned short>& indices) const noexcept;

        void CreateIndexArray(std::vector<int>& indices, FaceWinding winding) const noexcept;
        void CreateIndexArray(std::vector<short>& indices, FaceWinding winding) const noexcept;

        // Pick the closest face that intersects with the ray
        PickedTriangle PickTriangle(const rpp::Ray& ray) const noexcept;

        rpp::BoundingBox CalculateBBox() const noexcept {
            return rpp::BoundingBox::create(Verts);
        }
        rpp::BoundingBox CalculateBBox(const std::vector<rpp::IdVector3>& deltas) const noexcept {
            return rpp::BoundingBox::create(Verts, deltas);
        }

        // prints group info to stdout
        void Print() const;

        // prints vertex info to stdout
        void PrintVerts(const char* what = nullptr) const;
    };


    //////////////////////////////////////////////////////////////////////

    /**
     * Convenient mesh load options
     * Usage:
     *     Nano::Mesh mesh { "mesh.obj", Options::NoThrow | Options::LogGroups };
     */
    enum class Options : int
    {
        None = 0,

        /**
         * LOAD:
         * If true, then all named meshgroups will be ignored
         * and all verts/faces will be put into the first object group instead
         * @note This will break multi-material support, so only use this if
         *       you have 1 or 0 materials.
         */
        SingleGroup = (1 << 1),

        /**
         * LOAD:
         * If true, empty groups will not be discarded and will
         * be treated as metadata instead.
         * Check MeshGroup::Offset for position meta
         */
        EmptyGroups = (1 << 2),

        /**
         * LOAD+SAVE:
         * if TRUE  Mesh Load/Save throws an exception during failure
         * if FALSE Mesh Load/Save logs error and returns false
         */
        NoThrow = (1 << 3),

        /**
         * LOAD+SAVE:
         * Log mesh group stats during load/save
         */
        Log = (1 << 4),

        /**
         * LOAD:
         * Split non-contiguous UV shell vertices.
         * This MAY increase vertex count, but if UV's are contiguous
         * then vertexcount+order will remain the same.
         * needed in game engines which use Array-Of-Structs:
         * `struct Vertex { vec3 pos; vec3 norm; vec2 uv; };`
         */
        SplitSeams = (1 << 5),

        /**
         * LOAD:
         * Flatten Normals and UV's to match vertex count
         */
        Flatten = (1 << 6),

        /**
         * LOAD:
         * Converts faces to ClockWise, from default CounterClockWise
         */
        ClockWise = (1 << 7),

        /**
         * LOAD:
         * This will enable specific settings for Unity compatibility:
         * + Options::SingleGroup
         * + Options::SplitSeams
         * + Options::Flatten
         * + Options::ClockWise
         * + CoordSys::Unity
         */
        Unity = (1 << 8),
    };

    inline Options operator|(Options a, Options b)
    {
        return static_cast<Options>(static_cast<int>(a) | static_cast<int>(b));
    }

    inline void operator|=(Options& a, Options b)
    {
        a = static_cast<Options>(static_cast<int>(a) | static_cast<int>(b));
    }

    inline bool operator&(Options a, Options b)
    {
        return (static_cast<int>(a) & static_cast<int>(b)) != 0;
    }

    NANOMESH_API std::string to_string(Options o);

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
        std::string Name;
        std::vector<MeshGroup> Groups;
        std::vector<MeshBone> Bones; // all bones
        std::vector<SkinnedBone> SkinnedBones; // only animated/skinned bones
        std::vector<AnimationClip> AnimationClips; // all animation clips

        // Default empty mesh
        Mesh();
        
        // Automatically constructs a new mesh, check good() or cast to bool to check if successful
        explicit Mesh(rpp::strview meshPath, Options options = {});

        ~Mesh();

        int TotalTris() const;
        int TotalVerts() const;
        int TotalCoords() const;
        int TotalNormals() const;
        int TotalColors() const;
        int TotalAnimClips() const;


        bool good() const { return !Groups.empty(); }
        explicit operator bool() const { return  good(); }
        bool operator!()         const { return !good(); }


        MeshGroup* FindGroup(rpp::strview name);
        const MeshGroup* FindGroup(rpp::strview name) const;
        MeshGroup& CreateGroup(std::string name);
        MeshGroup& FindOrCreateGroup(rpp::strview name);

        std::shared_ptr<Material> FindMaterial(rpp::strview name) const;
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


        Mesh(Mesh&& o) = default; // Allow MOVE
        Mesh& operator=(Mesh&& o) = default;

        Mesh(const Mesh&) noexcept = delete; // NOCOPY, call Clone() manually plz
        Mesh& operator=(const Mesh&) noexcept = delete;


        void Clear() noexcept;


        // Create a clone of this 3D Mesh on demand. No automatic copy operators allowed.
        // @param cloneMaterials Will also clone the material references
        Mesh Clone(bool cloneMaterials = false) const noexcept;

        /**
         * Attempts to load this mesh.
         * @note This will only throw if Options::NoExceptions is true
         * @return If !NoExceptions, returns TRUE on SUCCESS
         */
        bool Load(rpp::strview meshPath, Options opt = {});
        
    private:
        void ApplyLoadOptions(Options opt);

    public:
        bool SaveAs(rpp::strview meshPath, Options opt = {}) const;

        // Is FBX supported on this platform?
        static bool IsFBXSupported() noexcept;
        bool LoadFBX(rpp::strview meshPath, Options opt = {});
        bool LoadOBJ(rpp::strview meshPath, Options opt = {});
        
        /**
         * A simple custom mesh text format, which is similar to OBJ
         * Look at NanoMesh/bin/.txt for examples
         * @note This is designed for testing purposes
         */
        bool LoadTXT(rpp::strview meshPath, Options opt = {});

        bool SaveAsFBX(rpp::strview meshPath, Options opt = {}) const;
        bool SaveAsOBJ(rpp::strview meshPath, Options opt = {}) const;
        // bool SaveAsTxt(strview meshPath, Options opt = {}) const;

        // Recalculates all normals by find shared and non-shared vertices on the same pos
        // Currently does not respect smoothing groups
        // @param checkDuplicateVerts Will perform an O(n^2) search for duplicate vertices to
        //                            correctly calculate normals for mesh surfaces with unwelded verts
        void RecalculateNormals(bool checkDuplicateVerts = false) noexcept;

        // normal = -normal;
        void InvertNormals() noexcept;

        rpp::BoundingBox CalculateBBox() const noexcept;

        // Adds additional MeshGroups from another Mesh
        // Optionally appends an extra offset to position vertices
        void AddMeshData(const Mesh& mesh, rpp::Vector3 offset = rpp::Vector3::Zero()) noexcept;

        void SplitSeamVertices() noexcept;

        // Flattens all mesh data, so MapMode is MapPerFaceVertex
        // This will make the mesh data compatible with any 3D graphics engine out there
        // However, mesh data will be thus stored less efficiently (no vertex data sharing)
        // 
        // Verts, Coords, Normals and Colors will all be stored in a linear sequence
        // with equal length, so creating a corresponding vertex/index array is trivial
        //
        void FlattenMeshData() noexcept;
        bool IsFlattened() const noexcept;

        // Optimized flatten is:
        // + g.SplitSeamVertices()
        // + g.PerVertexFlatten()
        void OptimizedFlatten() noexcept;

        // Sets the face winding to all groups
        void SetFaceWinding(FaceWinding winding) noexcept;

        // Converts all MeshGroup 3D vector coord system by changing 
        void SetCoordSys(CoordSys targetSystem) noexcept;

        // Merges all imported groups into a single group
        void MergeGroups() noexcept;

        // Pick the closest face that intersects with the ray
        PickedTriangle PickTriangle(const rpp::Ray& ray) const noexcept;

        // Adds a new empty animation clip and returns its index id
        int AddAnimClip(std::string name, float duration);
    };

    //////////////////////////////////////////////////////////////////////
}

#if _MSC_VER
#  pragma warning(pop) // pop dll-interface warning
#endif