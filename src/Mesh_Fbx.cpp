#include <Nano/Mesh.h>
#include "InternalConfig.h"

#if ENABLE_FBX_MESH_LOADER
#include <memory> // unique_ptr
#include <mutex>
#define FBXSDK_NAMESPACE_USING 0
#include <fbxsdk.h>
#include <rpp/file_io.h>

namespace Nano
{
    namespace fbx = fbxsdk;
    static fbx::FbxManager*    SdkManager;
    static fbx::FbxIOSettings* IOSettings;
    static std::mutex     SdkMutex;

    ///////////////////////////////////////////////////////////////////////////////////////////////


    // scoped pointer for safely managing FBX resources
    template<class T> using scoped_ptr = std::unique_ptr<T, void(*)(T*)>;

    template<class T> struct FbxPtr : scoped_ptr<T>
    {
        FbxPtr(T* obj) : scoped_ptr<T>(obj, [](T* o) { o->Destroy(); }) {}
    };

    // scoped read lock
    template<class T> struct FbxReadLock
    {
        fbx::FbxLayerElementArrayTemplate<T>& arr;
        const T* data;
        int count;
        explicit FbxReadLock(fbx::FbxLayerElementArrayTemplate<T>& arr) : arr{arr}, 
            data{(T*)arr.GetLocked(fbx::FbxLayerElementArray::eReadLock)}, 
            count{arr.GetCount()} {}
        ~FbxReadLock() { arr.ReadUnlock(); }
        FbxReadLock(FbxReadLock&&) = delete;
        FbxReadLock(const FbxReadLock&) = delete;
        FbxReadLock& operator=(FbxReadLock&&) = delete;
        FbxReadLock& operator=(const FbxReadLock&) = delete;
    };

    // scoped write lock with a resize initializer
    template<class T> struct FbxWriteLock
    {
        fbx::FbxLayerElementArrayTemplate<T>& arr;
        T* data;
        explicit FbxWriteLock(int resizeTo, fbx::FbxLayerElementArrayTemplate<T>& arr) : arr{ arr }
        {
            arr.AddMultiple(resizeTo);
            data = (T*)arr.GetLocked(fbx::FbxLayerElementArray::eReadWriteLock);
        }
        ~FbxWriteLock() { arr.ReadWriteUnlock(); }
        FbxWriteLock(FbxWriteLock&&) = delete;
        FbxWriteLock(const FbxWriteLock&) = delete;
        FbxWriteLock& operator=(FbxWriteLock&&) = delete;
        FbxWriteLock& operator=(const FbxWriteLock&) = delete;
    };

    static void InitFbxManager()
    {
        static bool initialized = [] // C++17 thread-safe static init
        {
            SdkManager = fbx::FbxManager::Create();
            IOSettings = fbx::FbxIOSettings::Create(SdkManager, "IOSRoot");
            SdkManager->SetIOSettings(IOSettings);
            return true;
        }();
    }

    static fbx::FbxGeometryElement::EMappingMode toFbxMapping(MapMode mode)
    {
        switch (mode)
        {
            default: Assert(false, "Unsupported mesh reference mode");
            case MapMode::None:          return fbx::FbxGeometryElement::eNone;
            case MapMode::PerVertex:     return fbx::FbxGeometryElement::eByControlPoint;
            case MapMode::PerFaceVertex: return fbx::FbxGeometryElement::eByPolygonVertex;
            case MapMode::PerFace:       return fbx::FbxGeometryElement::eByPolygon;
        }
    }

    static fbx::FbxLayerElement::EReferenceMode toFbxReference(MapMode mode)
    {
        switch (mode)
        {
            default: Assert(false, "Unsupported mesh reference mode");
            case MapMode::None:
            case MapMode::PerVertex:     return fbx::FbxLayerElement::eDirect;
            case MapMode::PerFaceVertex: return fbx::FbxLayerElement::eIndexToDirect;
            case MapMode::PerFace:       return fbx::FbxLayerElement::eIndexToDirect;
        }
    }

    // description string for mapping mode
    static const char* toString(fbx::FbxGeometryElement::EMappingMode mapping) noexcept
    {
        switch (mapping)
        {
            default:
            case fbx::FbxLayerElement::eNone:            return "no";
            case fbx::FbxLayerElement::eByControlPoint:  return "per-vertex";
            case fbx::FbxLayerElement::eByPolygonVertex: return "per-face-vertex";
            case fbx::FbxLayerElement::eByPolygon:       return "per-face";
            case fbx::FbxLayerElement::eByEdge:          return "per-edge";
            case fbx::FbxLayerElement::eAllSame:         return "uniform";
        }
    }

    static FINLINE rpp::Vector3 FbxToOpenGL(fbx::FbxVector4 v)
    {
        return {
             (float)v.mData[0],
             (float)v.mData[2],  // in OGL/D3D Y axis is up, so ourUpY = fbxUpZ
            -(float)v.mData[1],  // OGL Z is fwd, so ourFwdZ = fbxFwdY
        };
    }
    static FINLINE rpp::Vector3 FbxToOpenGL(fbx::FbxDouble3 v)
    {
        return {
             (float)v.mData[0],
             (float)v.mData[2],  // in OGL/D3D Y axis is up, so ourUpY = fbxUpZ
            -(float)v.mData[1],  // OGL Z is fwd, so ourFwdZ = fbxFwdY
        };
    }
    static FINLINE fbx::FbxVector4 GLToFbxVec4(rpp::Vector3 v)
    {
        return {
            (double)v.x,
           -(double)v.z,
            (double)v.y 
        };
    }
    static FINLINE fbx::FbxDouble3 GLToFbxDouble3(rpp::Vector3 v)
    {
        return {
            (double)v.x,
           -(double)v.z,
            (double)v.y 
        };
    }
    static FINLINE fbx::FbxDouble3 ToFbxColor3(rpp::Vector3 v)
    {
        return { (double)v.x, (double)v.y, (double)v.z };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    
    static void LoadVerticesAndFaces(
        Nano::MeshGroup& meshGroup, const fbx::FbxMesh* fbxMesh, 
        std::vector<int>& oldIndices)
    {
        int numVertices = fbxMesh->GetControlPointsCount();
        meshGroup.Verts.resize(numVertices);
        rpp::Vector3*    vertices = meshGroup.Verts.data();
        fbx::FbxVector4* fbxVertices = fbxMesh->GetControlPoints();

        for (int i = 0; i < numVertices; ++i)
            vertices[i] = FbxToOpenGL(fbxVertices[i]);

        int numPolygons = fbxMesh->GetPolygonCount();
        int* indices = fbxMesh->GetPolygonVertices(); // control point indices

        std::vector<Nano::Triangle>& triangles = meshGroup.Tris;
        triangles.reserve(numPolygons);
        oldIndices.reserve(numPolygons * 3);

        int oldPolyVertexId = 0;
        for (int polygonIndex = 0; polygonIndex < numPolygons; ++polygonIndex)
        {
            int numPolygonVertices = fbxMesh->GetPolygonSize(polygonIndex);
            int* vertexIds   = &indices[ fbxMesh->GetPolygonVertexIndex(polygonIndex) ];
            
            Assert(numPolygonVertices >= 3, "Not enough polygon vertices: %d. Expected at least 3.", numPolygonVertices);

            Nano::Triangle* f = &rpp::emplace_back(triangles);
            f->a.v = vertexIds[0];
            f->b.v = vertexIds[1];
            f->c.v = vertexIds[2];
            oldIndices.push_back(oldPolyVertexId + 0);
            oldIndices.push_back(oldPolyVertexId + 1);
            oldIndices.push_back(oldPolyVertexId + 2);

            // if we have Quads or Polygons, then force triangulation:
            for (int i = 3; i < numPolygonVertices; ++i)
            {
                // CCW
                // v[0], v[2], v[3]
                VertexDescr vd0 = f->a; // by value, because emplace_back may realloc
                VertexDescr vd2 = f->c;
                f = &rpp::emplace_back(triangles);
                f->a = vd0;
                f->b = vd2;
                f->c.v = vertexIds[i];

                int id0 = oldIndices[oldIndices.size() - 3];
                int id2 = oldIndices[oldIndices.size() - 1];
                oldIndices.push_back(id0);
                oldIndices.push_back(id2);
                oldIndices.push_back(oldPolyVertexId + i);
            }
            oldPolyVertexId += numPolygonVertices;
        }
    }

    static void LoadNormals(MeshGroup& meshGroup, 
        fbx::FbxGeometryElementNormal* elementNormal, 
        const std::vector<int>& oldIndices)
    {
        fbx::FbxLayerElement::EMappingMode mapMode = elementNormal->GetMappingMode();
        FbxReadLock<fbx::FbxVector4> normalsLock {elementNormal->GetDirectArray()};
        FbxReadLock<int>        indexLock   {elementNormal->GetIndexArray()};
        const fbx::FbxVector4* fbxNormals = normalsLock.data;
        const int*        indices    = indexLock.data; // if != null, normals are indexed

        //printf("  %5d  %s normals\n", normalsLock.count, toString(mapMode));

        const int numNormals = normalsLock.count;
        const int maxNormals = indices ? indexLock.count : numNormals;
        meshGroup.Normals.resize(numNormals);
        rpp::Vector3* normals = meshGroup.Normals.data();

        // copy all normals; at this point it's not important if they are indexed or not
        for (int i = 0; i < numNormals; ++i)
            normals[i] = FbxToOpenGL(fbxNormals[i]);

        const int numTriangles = meshGroup.NumTris();
        Triangle* faces = meshGroup.Tris.data();

        // each polygon vertex can have multiple normals, 
        // but if indices are used, most will be shared normals
        // eByPolygonVertex There will be one mapping coordinate for each vertex, 
        // for every polygon of which it is a part. This means that a vertex will 
        // have as many mapping coordinates as polygons of which it is a part.
        if (mapMode == fbx::FbxLayerElement::eByPolygonVertex)
        {
            meshGroup.NormalsMapping = MapMode::PerFaceVertex;

            const int* oldPolyVertexIds = oldIndices.data();
            for (int nextId = 0, faceId = 0; faceId < numTriangles; ++faceId)
            {
                for (VertexDescr& vd : faces[faceId])
                {
                    const int polyVertexId = oldPolyVertexIds[nextId++];
                    Assert(polyVertexId < maxNormals, "Normal index out of bounds: %d / %d", polyVertexId, maxNormals);
                    vd.n = indices ? indices[polyVertexId] : polyVertexId;
                }
            }
        }
        else if (mapMode == fbx::FbxLayerElement::eByControlPoint) // each mesh vertex has a single normal (best case)
        {
            meshGroup.NormalsMapping = MapMode::PerVertex;

            for (int faceId = 0; faceId < numTriangles; ++faceId)
                for (VertexDescr& vd : faces[faceId])
                {
                    const int vertexId = vd.v;
                    Assert(vertexId < maxNormals, "Normal index out of bounds: %d / %d", vertexId, maxNormals);
                    vd.n = indices ? indices[vertexId] : vertexId; // indexed by VertexId OR same as VertexId
                }
        }
        else if (mapMode == fbx::FbxLayerElement::eByPolygon) // each polygon has a single normal, OK case, but not ideal
        {
            meshGroup.NormalsMapping = MapMode::PerFace;

            // @todo indices[faceId] might be wrong
            for (int faceId = 0; faceId < numTriangles; ++faceId)
                for (VertexDescr& vd : faces[faceId])
                {
                    Assert(faceId < maxNormals, "Normal index out of bounds: %d / %d", faceId, maxNormals);
                    vd.n = indices ? indices[faceId] : faceId;
                }
        }
    }

    static void LoadCoords(MeshGroup& meshGroup, 
        fbx::FbxGeometryElementUV* elementUVs, 
        const std::vector<int>& oldIndices)
    {
        fbx::FbxLayerElement::EMappingMode mapMode = elementUVs->GetMappingMode();
        Assert(mapMode == fbx::FbxLayerElement::eByPolygonVertex, "Only ByPolygonVertex mapping is supported");

        FbxReadLock<fbx::FbxVector2> uvsLock   {elementUVs->GetDirectArray()};
        FbxReadLock<int>        indexLock {elementUVs->GetIndexArray()};
        const fbx::FbxVector2* fbxUVs  = uvsLock.data;
        const int*        indices = indexLock.data; // if != null, UVs are indexed

        const int numCoords = uvsLock.count;
        const int maxCoords = indices ? indexLock.count : numCoords;
        meshGroup.Coords.resize(numCoords);
        rpp::Vector2* coords = meshGroup.Coords.data();

        for (int i = 0; i < numCoords; ++i)
        {
            coords[i].x = (float)fbxUVs[i].mData[0];
            coords[i].y = (float)fbxUVs[i].mData[1];
        }

        const int numTriangles = meshGroup.NumTris();
        Triangle* faces = meshGroup.Tris.data();

        // each polygon vertex can have multiple UV coords,
        // this allows multiple UV shells, so UV-s aren't forced to be contiguous
        // if indices != null, then most of these UV-s coords will be shared
        if (mapMode == fbx::FbxLayerElement::eByPolygonVertex)
        {
            meshGroup.CoordsMapping = MapMode::PerFaceVertex;

            const int* oldPolyVertexIds = oldIndices.data();
            for (int nextId = 0, faceId = 0; faceId < numTriangles; ++faceId)
            {
                for (VertexDescr& vd : faces[faceId])
                {
                    const int polyVertexId = oldPolyVertexIds[nextId++];
                    Assert(polyVertexId < maxCoords, "UV index out of bounds: %d / %d", polyVertexId, maxCoords);
                    vd.t = indices ? indices[polyVertexId] : polyVertexId;
                }
            }
        }
        else if (mapMode == fbx::FbxLayerElement::eByControlPoint)
        {
            meshGroup.CoordsMapping = MapMode::PerVertex;

            for (int faceId = 0; faceId < numTriangles; ++faceId)
                for (VertexDescr& vd : faces[faceId])
                {
                    const int vertexId = vd.v;
                    Assert(vertexId < maxCoords, "UV index out of bounds: %d / %d", vertexId, maxCoords);
                    vd.t = indices ? indices[vertexId] : vertexId; // indexed separately OR same as VertexId
                }
        }
        else Assert(false, "Unsupported UV map mode");
    }

    static void LoadColors(MeshGroup& meshGroup, 
        fbx::FbxGeometryElementVertexColor* elementColors, 
        const std::vector<int>& oldIndices)
    {
        fbx::FbxLayerElement::EMappingMode mapMode = elementColors->GetMappingMode();
        FbxReadLock<fbx::FbxColor> colorsLock {elementColors->GetDirectArray()};
        FbxReadLock<int>      indexLock  {elementColors->GetIndexArray()};
        const fbx::FbxColor* fbxColors = colorsLock.data;
        const int*      indices   = indexLock.data; // if != null, colors are indexed

        const int numColors = colorsLock.count;
        const int maxColors = indices ? indexLock.count : numColors;
        meshGroup.Colors.resize(numColors);
        rpp::Vector3* colors = meshGroup.Colors.data();

        for (int i = 0; i < numColors; ++i)
        {
            colors[i].x = (float)fbxColors[i].mRed;
            colors[i].y = (float)fbxColors[i].mGreen;
            colors[i].z = (float)fbxColors[i].mBlue;
        }

        const int numTriangles = meshGroup.NumTris();
        Triangle* faces = meshGroup.Tris.data();

        // with eByPolygonVertex, each polygon vertex can have multiple colors,
        // this allows full face coloring with no falloff blending with neighboring faces
        // if indices != null, then most of these colors will be shared
        if (mapMode == fbx::FbxLayerElement::eByPolygonVertex)
        {
            meshGroup.ColorMapping = MapMode::PerFaceVertex;

            const int* oldPolyVertexIds = oldIndices.data();
            for (int nextId = 0, faceId = 0; faceId < numTriangles; ++faceId)
            {
                for (VertexDescr& vd : faces[faceId])
                {
                    const int polyVertexId = oldPolyVertexIds[nextId++];
                    Assert(polyVertexId < maxColors, "Color index out of bounds: %d / %d", polyVertexId, maxColors);
                    vd.c = indices ? indices[polyVertexId] : polyVertexId;
                }
            }
        }
        else if (mapMode == fbx::FbxLayerElement::eByControlPoint)
        {
            meshGroup.ColorMapping = MapMode::PerVertex;

            for (int faceId = 0; faceId < numTriangles; ++faceId)
                for (VertexDescr& vd : faces[faceId])
                    vd.c = indices ? indices[vd.v] : vd.v; // indexed separately OR same as VertexId
        }
        else if (mapMode == fbx::FbxLayerElement::eByPolygon)
        {
            meshGroup.ColorMapping = MapMode::PerFace;

            // @todo indices[faceId] might be wrong
            for (int faceId = 0; faceId < numTriangles; ++faceId)
                for (VertexDescr& vd : faces[faceId])
                    vd.c = indices ? indices[faceId] : faceId; // indexed separately OR same as FaceId
        }
    }

    static void RecurseSkeleton(Mesh& mesh, fbx::FbxNode* node, int parentIndex, int& boneIndex)
    {
        for (int i = node->GetChildCount() - 1; i >= 0; --i)
        {
            fbx::FbxNode* child = node->GetChild(i);
            MeshBone& bone = rpp::emplace_back(mesh.Bones);
            bone.BoneIndex = boneIndex;
            bone.ParentIndex = parentIndex;
            if (auto* name = child->GetName())
                bone.Name = name;
            fbx::FbxDouble3 offset = node->LclTranslation.Get();
            fbx::FbxDouble3 rot    = node->LclRotation.Get();   // @note Euler XYZ Degrees
            fbx::FbxDouble3 scale  = node->LclScaling.Get();
            bone.Pose.Translation = FbxToOpenGL(offset);
            bone.Pose.Rotation    = FbxToOpenGL(rot);
            bone.Pose.Scale       = FbxToOpenGL(scale);
            ++boneIndex;
            RecurseSkeleton(mesh, child, boneIndex, boneIndex);
        }
    }
    static void CreateSkeleton(Mesh& mesh, fbx::FbxNode* root)
    {
        int boneIndex = 0;
        RecurseSkeleton(mesh, root, -1, boneIndex);
    }

    static void LoadAnimation(MeshGroup& meshGroup)
    {
        
    }

    static std::shared_ptr<Nano::Material> LoadMaterial(fbx::FbxSurfaceMaterial* fbxMat)
    {
        auto mat = std::make_shared<Nano::Material>();
        mat->Name = fbxMat->GetName();
        //if (auto* lam = dynamic_cast<fbx::FbxSurfaceLambert*>(fbxMat))
        //{
        //    fbx::FbxTexture* texture = FbxCast<fbx::FbxTexture>(lam->Diffuse.GetSrcObject<fbx::FbxTexture>(i));


        //    mat->DiffuseColor = lam->Diffuse.GetSr
        //}
        //fbx::FbxSurfacePhong* mat = fbx::FbxSurfacePhong::Create(scene, m.Name.data());
        //materials[&m] = mat;
        //mat->Ambient.Set(ToFbxColor3(m.AmbientColor));
        //mat->Diffuse.Set(ToFbxColor3(m.DiffuseColor));
        //mat->Specular.Set(ToFbxColor3(m.SpecularColor));
        //mat->SpecularFactor.Set((double)m.Specular);
        //mat->TransparencyFactor.Set((double)m.Alpha);
        //if (!m.EmissiveColor.almostEqual(rpp::Color3::Black()))
        //    mat->Emissive.Set(ToFbxColor3(m.EmissiveColor));

        //if (auto* diffuse = NewTexture(scene, folder, m.DiffusePath, "Diffuse Texture")) {
        //    mat->Diffuse.ConnectSrcObject(diffuse);
        //}
        //if (auto* alpha = NewTexture(scene, folder, m.AlphaPath, "Alpha Texture")) {
        //    mat->TransparentColor.ConnectSrcObject(alpha);
        //}
        //if (auto* specular = NewTexture(scene, folder, m.SpecularPath, "Specular Texture")) {
        //    mat->Specular.ConnectSrcObject(specular);
        //}
        //if (auto* normal = NewTexture(scene, folder, m.NormalPath, "Normal Texture", fbx::FbxTexture::eBumpNormalMap)) {
        //    mat->NormalMap.ConnectSrcObject(normal);
        //}
        //if (auto* emissive = NewTexture(scene, folder, m.EmissivePath, "Emissive Texture")) {
        //    mat->Emissive.ConnectSrcObject(emissive);
        //    mat->EmissiveFactor.ConnectSrcObject(emissive);
        //}
        return mat;
    }

    static void SetTransformGL(MeshGroup& group, fbx::FbxNode* node)
    {
        fbx::FbxDouble3 offset = node->LclTranslation.Get();
        fbx::FbxDouble3 rot    = node->LclRotation.Get();   // @note Euler XYZ Degrees
        fbx::FbxDouble3 scale  = node->LclScaling.Get();
        group.Offset   = FbxToOpenGL(offset);
        group.Rotation = FbxToOpenGL(rot);
        group.Scale    = FbxToOpenGL(scale);
    }
    
    bool Mesh::IsFBXSupported() noexcept { return true; }

    bool Mesh::LoadFBX(rpp::strview meshPath, Options opt)
    {
        Clear();
        InitFbxManager();
        FbxPtr<fbx::FbxImporter> importer = fbx::FbxImporter::Create(SdkManager, "");
        //int format = SdkManager->GetIOPluginRegistry()->FindReaderIDByExtension("fbx");
        int format = -1;

        if (!importer->Initialize(meshPath.to_cstr(), format, SdkManager->GetIOSettings())) {
            NanoErr(opt, "Failed to open file '%s': %s\n", meshPath, importer->GetStatus().GetErrorString());
        }

        FbxPtr<fbx::FbxScene> scene = fbx::FbxScene::Create(SdkManager, "scene");
        if (!importer->Import(scene.get())) {
            NanoErr(opt, "Failed to load FBX '%s': %s\n", meshPath, importer->GetStatus().GetErrorString());
        }
        importer.reset();

        fbx::FbxArray<fbx::FbxSurfaceMaterial*> fbxMaterials;
        scene->FillMaterialArray(fbxMaterials);

        std::vector<std::shared_ptr<Nano::Material>> materials(scene->GetMaterialCount());
        for (size_t i = 0; i < materials.size(); ++i)
        {
            materials[i] = LoadMaterial(scene->GetMaterial(i));
        }

        if (fbx::FbxNode* root = scene->GetRootNode())
        {
            Name = file_name(meshPath);
            if (opt & Options::Log) {
                LogInfo("Load %-20s  %s", file_nameext(meshPath), to_string(opt));
            }

            // @note ConvertScene only affects the global/local matrices, it doesn't modify the vertices themselves
            fbx::FbxAxisSystem sceneAxisSys = scene->GetGlobalSettings().GetAxisSystem();
            //if (sceneAxisSys != FbxAxisSystem{ FbxAxisSystem::eOpenGL })
            //    LogWarning("Invalid AxisSystem! Please Re-Export the FBX in OpenGL Axis System");

            int numChildren = root->GetChildCount();
            for (int childIndex = 0; childIndex < numChildren; ++childIndex)
            {
                fbx::FbxNode* child = root->GetChild(childIndex);
                fbx::FbxNodeAttribute* attribute = child->GetNodeAttribute();

                if (fbx::FbxMesh* mesh = child->GetMesh())
                {
                    MeshGroup& group = CreateGroup(child->GetName());
                    SetTransformGL(group, child);

                    std::vector<int> oldIndices;
                    LoadVerticesAndFaces(group, mesh, oldIndices);
                    if (auto* normals = mesh->GetElementNormal())      LoadNormals(group, normals, oldIndices);
                    if (auto* uvs     = mesh->GetElementUV())          LoadCoords(group,  uvs,     oldIndices);
                    if (auto* colors  = mesh->GetElementVertexColor()) LoadColors(group,  colors,  oldIndices);
                }
                else if (attribute && attribute->GetAttributeType() == fbx::FbxNodeAttribute::eSkeleton)
                {
                    continue; // ignore skeleton nodes at this point
                }
                else if (opt & Options::EmptyGroups)
                {
                    MeshGroup& group = CreateGroup(child->GetName());
                    SetTransformGL(group, child);
                }
            }

            CreateSkeleton(*this, root);

            ApplyLoadOptions(opt);
            return true;
        }
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    static void SaveVertices(const MeshGroup& group, fbx::FbxMesh* mesh)
    {
        int numVertices = group.NumVerts();
        mesh->InitControlPoints(numVertices);
        fbx::FbxVector4* points = mesh->GetControlPoints();
        const rpp::Vector3* vertices = group.Verts.data();

        for (int i = 0; i < numVertices; ++i)
        {
            points[i] = GLToFbxVec4(vertices[i]);
        }
    }

    static void SaveNormals(const MeshGroup& group, fbx::FbxMesh* mesh)
    {
        if (const int numNormals = group.NumNormals())
        {
            //printf("  %5d  normals\n", numNormals);

            Assert((group.NormalsMapping == MapMode::PerVertex || group.NormalsMapping == MapMode::PerFaceVertex), 
                    "Only per-vertex or per-face-vertex normals are supported");

            fbx::FbxGeometryElementNormal* elementNormal = mesh->CreateElementNormal();
            elementNormal->SetMappingMode(toFbxMapping(group.NormalsMapping));
            elementNormal->SetReferenceMode(toFbxReference(group.NormalsMapping));

            auto& elements = elementNormal->GetDirectArray();
            const rpp::Vector3* normals = group.Normals.data();

            for (int i = 0; i < numNormals; ++i)
            {
                elements.Add(GLToFbxVec4(normals[i]));
            }

            if (group.NormalsMapping == MapMode::PerFaceVertex)
            {
                auto& indices = elementNormal->GetIndexArray();
                for (const Triangle& face : group)
                    for (const VertexDescr& vd : face)
                        indices.Add(vd.n);
            }
        }
    }

    static void SaveCoords(const MeshGroup& group, fbx::FbxMesh* mesh)
    {
        if (const int numCoords = group.NumCoords())
        {
            Assert((group.CoordsMapping == MapMode::PerVertex || group.CoordsMapping == MapMode::PerFaceVertex), 
                    "Only per-vertex or per-face-vertex UV coords are supported");

            fbx::FbxGeometryElementUV* elementUVs = mesh->CreateElementUV("DiffuseUV");
            elementUVs->SetMappingMode(toFbxMapping(group.CoordsMapping));
            elementUVs->SetReferenceMode(toFbxReference(group.CoordsMapping));

            auto& elements = elementUVs->GetDirectArray();
            const rpp::Vector2* uvs = group.Coords.data();

            for (int i = 0; i < numCoords; ++i)
            {
                elements.Add(fbx::FbxVector2{ uvs[i].x, uvs[i].y });
            }

            if (group.CoordsMapping == MapMode::PerFaceVertex)
            {
                auto& indices = elementUVs->GetIndexArray();
                for (const Triangle& face : group)
                    for (const VertexDescr& vd : face)
                        indices.Add(vd.t);
            }
        }
    }

    static void SaveColors(const MeshGroup& group, fbx::FbxMesh* mesh)
    {
        if (const int numColors = group.NumColors())
        {
            Assert((group.ColorMapping == MapMode::PerVertex || group.ColorMapping == MapMode::PerFaceVertex), 
                "Only per-vertex or per-face-vertex colors are supported");

            fbx::FbxGeometryElementVertexColor* elementColors = mesh->CreateElementVertexColor();
            elementColors->SetMappingMode(toFbxMapping(group.ColorMapping));
            elementColors->SetReferenceMode(toFbxReference(group.ColorMapping));

            auto& elements = elementColors->GetDirectArray();
            const rpp::Vector3* colors = group.Colors.data();
            for (int i = 0; i < numColors; ++i)
            {
                elements.Add(fbx::FbxColor{ colors[i].x, colors[i].y, colors[i].z });
            }

            if (group.ColorMapping == MapMode::PerFaceVertex)
            {
                auto& indices = elementColors->GetIndexArray();
                for (const Triangle& face : group)
                    for (const VertexDescr& vd : face)
                        indices.Add(vd.c);
            }
        }
    }

    static void CreatePolygons(const MeshGroup& group, fbx::FbxMesh* mesh)
    {
        for (const Triangle& face : group)
        {
            mesh->BeginPolygon(-1, -1, -1, false);
            for (const VertexDescr& vd : face)
            {
                mesh->AddPolygon(vd.v, -1);
            }
            mesh->EndPolygon();
        }
        mesh->BuildMeshEdgeArray();
    }

    using Materials = std::unordered_map<Material*, fbx::FbxSurfacePhong*>;

    static fbx::FbxFileTexture* NewTexture(fbx::FbxScene* scene,
                                           const std::string& absFolderPath,
                                           const std::string& relativePath,
                                           const char* name,
                                           fbx::FbxTexture::ETextureUse usage = fbx::FbxTexture::eStandard)
    {
        if (relativePath.empty())
            return nullptr;
        fbx::FbxFileTexture* tex = fbx::FbxFileTexture::Create(scene, name);

        // https://forums.autodesk.com/t5/fbx-forum/fbx-and-relative-paths/td-p/3968055
        // Always set the absolute path to the texture because
        // FBXSDK will always recalculate the relative paths, which is really annoying
        std::string absPath = rpp::path_combine(absFolderPath, relativePath);
        if (!tex->SetFileName(absPath.c_str())) {
            LogWarning("FbxFileTexture::SetFileName failed: %s", absPath);
        }
        //if (!tex->SetRelativeFileName(relativePath.c_str())) {
        //    LogWarning("FbxFileTexture::SetRelativeFileName failed: %s", relativePath);
        //}
        tex->SetTextureUse(usage);
        tex->SetMappingType(fbx::FbxTexture::eUV);
        tex->SetMaterialUse(fbx::FbxFileTexture::eModelMaterial);
        tex->SetSwapUV(false);
        tex->SetTranslation(0.0, 0.0);
        tex->SetScale(1.0, 1.0);
        tex->SetRotation(0.0, 0.0);
        return tex;
    }

    static Materials CreateMaterials(fbx::FbxScene* scene,
                                     const std::vector<MeshGroup>& groups,
                                     const std::string& meshSavePath)
    {
        std::string folder = rpp::folder_path(rpp::full_path(meshSavePath));
        Nano::Materials materials { {nullptr, nullptr} };

        for (const Nano::MeshGroup& g : groups)
        {
            if (g.Mat)
            {
                Material& m = *g.Mat;
                fbx::FbxSurfacePhong* mat = fbx::FbxSurfacePhong::Create(scene, m.Name.data());
                materials[&m] = mat;
                mat->Ambient.Set(ToFbxColor3(m.AmbientColor));
                mat->Diffuse.Set(ToFbxColor3(m.DiffuseColor));
                mat->Specular.Set(ToFbxColor3(m.SpecularColor));
                mat->SpecularFactor.Set((double)m.Specular);
                mat->TransparencyFactor.Set((double)m.Alpha);
                if (!m.EmissiveColor.almostEqual(rpp::Color3::Black()))
                    mat->Emissive.Set(ToFbxColor3(m.EmissiveColor));

                if (auto* diffuse = NewTexture(scene, folder, m.DiffusePath, "Diffuse Texture")) {
                    mat->Diffuse.ConnectSrcObject(diffuse);
                }
                if (auto* alpha = NewTexture(scene, folder, m.AlphaPath, "Alpha Texture")) {
                    mat->TransparentColor.ConnectSrcObject(alpha);
                }
                if (auto* specular = NewTexture(scene, folder, m.SpecularPath, "Specular Texture")) {
                    mat->Specular.ConnectSrcObject(specular);
                }
                if (auto* normal = NewTexture(scene, folder, m.NormalPath, "Normal Texture", fbx::FbxTexture::eBumpNormalMap)) {
                    mat->NormalMap.ConnectSrcObject(normal);
                }
                if (auto* emissive = NewTexture(scene, folder, m.EmissivePath, "Emissive Texture")) {
                    mat->Emissive.ConnectSrcObject(emissive);
                    mat->EmissiveFactor.ConnectSrcObject(emissive);
                }
            }
        }
        return materials;
    }

    static void WriteSkeleton(const Nano::Mesh& mesh, fbx::FbxScene* scene, fbx::FbxNode* root)
    {
        size_t count = mesh.Bones.size();
        if (count == 0)
            return;

        std::vector<fbx::FbxNode*> skeleton(count);
        for (size_t i = 0; i < count; ++i)
        {
            const Nano::MeshBone& bone = mesh.Bones[i];
            bool isRoot = bone.ParentIndex < 0;
            fbx::FbxNode* parentBone = isRoot ? root : skeleton[bone.ParentIndex];

            fbx::FbxNode* thisBone = fbx::FbxNode::Create(scene, bone.Name.c_str());
            skeleton[i] = thisBone;
            fbx::FbxSkeleton* fbxSkeleton = fbx::FbxSkeleton::Create(scene, bone.Name.c_str());
            fbxSkeleton->SetSkeletonType(isRoot ? fbx::FbxSkeleton::eRoot : fbx::FbxSkeleton::eLimbNode);
            
            thisBone->LclTranslation.Set(GLToFbxDouble3(bone.Pose.Translation));
            thisBone->LclRotation.Set(GLToFbxDouble3(bone.Pose.Rotation));
            thisBone->LclScaling.Set(GLToFbxDouble3(bone.Pose.Scale));
            thisBone->SetNodeAttribute(fbxSkeleton);
            parentBone->AddChild(thisBone);
        }
    }

    bool Mesh::SaveAsFBX(rpp::strview meshPath, Options opt) const
    {
        if (!NumGroups()) {
            NanoErr(opt, "No mesh groups to export to '%s'\n", meshPath);
        }
        if (!TotalTris()) {
            NanoErr(opt, "No faces to export to '%s'\n", meshPath);
        }

        InitFbxManager();
        FbxPtr<fbx::FbxExporter> exporter = nullptr;
        {
            std::lock_guard<std::mutex> lock{SdkMutex};
            exporter = fbx::FbxExporter::Create(SdkManager, "");
        }

        //int format = SdkManager->GetIOPluginRegistry()->FindWriterIDByDescription("FBX 6.0 binary (*.fbx)");
        int format = -1;
        if (!exporter->Initialize(meshPath.to_cstr(), format, IOSettings)) {
            NanoErr(opt, "Failed to open file '%s' for writing: %s\n", meshPath, exporter->GetStatus().GetErrorString());
        }
        if (!exporter->SetFileExportVersion("FBX201400", fbx::FbxSceneRenamer::eNone)) {
            NanoErr(opt, "Failed to set FBX export version: %s\n", exporter->GetStatus().GetErrorString());
        }

        FbxPtr<fbx::FbxScene> scene = nullptr;
        {
            std::string sceneName = rpp::file_name(meshPath).to_string();
            std::lock_guard<std::mutex> lock{SdkMutex};
            scene = fbx::FbxScene::Create(SdkManager, sceneName.c_str());
        }

        fbx::FbxAxisSystem axisSys = { fbx::FbxAxisSystem::eOpenGL };
        scene->GetGlobalSettings().SetAxisSystem(axisSys);
        scene->GetGlobalSettings().SetSystemUnit(fbx::FbxSystemUnit(100.0/*meters*/));

        Nano::Materials materials = CreateMaterials(scene.get(), Groups, meshPath);

        if (fbx::FbxNode* root = scene->GetRootNode())
        {
            if (opt & Options::Log) {
                LogInfo("Save %-33s  %5d verts  %5d tris", 
                    file_nameext(meshPath), TotalVerts(), TotalTris());
            }
            for (const MeshGroup& group : Groups)
            {
                group.Print();
                fbx::FbxMesh* mesh = fbx::FbxMesh::Create(scene.get(), "");
                SaveVertices(group, mesh);
                SaveNormals(group, mesh);
                SaveCoords(group, mesh);
                SaveColors(group, mesh);
                CreatePolygons(group, mesh);

                fbx::FbxNode* node = fbx::FbxNode::Create(scene.get(), group.Name.c_str());

                if (auto* material = materials[group.Mat.get()])
                    node->AddMaterial(material);

                fbx::FbxDouble3 pos   = GLToFbxDouble3(group.Offset);
                fbx::FbxDouble3 rot   = GLToFbxDouble3(group.Rotation);
                fbx::FbxDouble3 scale = GLToFbxDouble3(group.Scale);
                node->LclTranslation.Set(pos);
                node->LclRotation.Set(rot);
                node->LclScaling.Set(scale);

                node->SetNodeAttribute(mesh);
                root->AddChild(node);
            }
            WriteSkeleton(*this, scene.get(), root);
        }

        if (!exporter->Export(scene.get())) {
            NanoErr(opt, "Failed to export FBX '%s': %s\n", meshPath, exporter->GetStatus().GetErrorString());
        }
        return true;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
}
#else // not ENABLE_FBX_MESH_LOADER
namespace Nano
{
    bool Mesh::IsFBXSupported() noexcept { return false; }
    bool Mesh::LoadFBX(rpp::strview meshPath, Options opt)
    {
        NanoErr(opt, "FBX not supported in this build!\n%s", meshPath);
    }
    bool Mesh::SaveAsFBX(rpp::strview meshPath, Options opt) const
    {
        NanoErr(opt, "FBX not supported in this build!\n%s", meshPath);
    }
}
#endif
