using System;
using System.Runtime.InteropServices;
using System.IO;
#if UNITY_2018_1_OR_NEWER
using UnityEngine;
#endif
#pragma warning disable 169 // field is not initialized
#pragma warning disable 649 // field is never assigned to

namespace Nano
{
    #if !UNITY_2018_1_OR_NEWER
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public float X, Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public float X, Y, Z;
    }

    public class Mesh
    {
        public string Name;
        public Vector3[] Vertices;
        public Vector3[] Normals;
        public Vector2[] Coords;
        public int[] Triangles; // triangle indices, [ A,A,A, B,B,B, ... ]
    }
    #endif

    [StructLayout(LayoutKind.Sequential)]
    public struct Options
    {
        /// <summary>
        /// If true, then all named meshgroups will be ignored
        /// and all verts/faces will be put into the first object group instead
        /// @note This will break multi-material support, so only use this if
        ///       you have 1 or 0 materials.
        /// </summary>
        public bool ForceSingleGroup;

        /// <summary>
        /// If true, empty groups will not be discarded and will
        /// be treated as metadata instead.
        /// Check MeshGroup::Offset for position meta
        /// </summary>
        public bool CreateEmptyGroups;

        /// <summary>
        /// Log mesh group stats during loading
        /// </summary>
        public bool LogMeshGroupInfo;

        /// <summary>
        /// split non-contiguous UV shell vertices.
        /// this MAY increase vertex count, but if UV's are contiguous then vertexcount+order will remain the same
        /// needed in game engines which use AOS Vertex { vec3 pos; vec3 norm; vec2 uv; };
        /// </summary>
        public bool SplitUVSeams;

        /// <summary>
        /// flatten Normals and UV's to match vertex count
        /// </summary>
        public bool PerVertexFlatten;

        /// <summary>
        /// Prepare mesh data so it's suitable to Unity
        /// </summary>
        public bool Unity;
    }

    public unsafe class MeshLoader
    {
        // rpp::strview
        struct StrView
        {
            private readonly sbyte* Str;
            private readonly int Length;
            public string AsString => Length != 0 ? new string(Str, 0, Length) : string.Empty;
            public bool Empty      => Length == 0;
            public bool NotEmpty   => Length > 0;
            public override string ToString() { return AsString; }
        }

        // NanoArrayView<Vector3>
        struct ArrayVec3
        {
            private readonly Vector3* Data;
            private readonly int Size;
            public Vector3[] AsArray
            {
                get
                {
                    var arr = new Vector3[Size];
                    for (int i = 0; i < arr.Length; ++i)
                        arr[i] = Data[i];
                    return arr;
                }
            }
        }

        // NanoArrayView<Vector2>
        struct ArrayVec2
        {
            private readonly Vector2* Data;
            private readonly int Size;
            public Vector2[] AsArray
            {
                get
                {
                    var arr = new Vector2[Size];
                    for (int i = 0; i < arr.Length; ++i)
                        arr[i] = Data[i];
                    return arr;
                }
            }
        }

        // NanoArrayView<int>
        struct ArrayInt
        {
            private readonly int* Data;
            private readonly int Size;
            public int[] AsArray
            {
                get
                {
                    var arr = new int[Size];
                    for (int i = 0; i < arr.Length; ++i)
                        arr[i] = Data[i];
                    return arr;
                }
            }
        }

        struct NanoMesh
        {
            public readonly StrView Name;
            public readonly int NumGroups;
            public readonly int NumVerts;
            public readonly int NumTris;
        }

        struct NanoMaterial
        {
            public readonly StrView Name; // name of the material instance
            public readonly StrView MaterialFile;
            public readonly StrView DiffusePath;
            public readonly StrView AlphaPath;
            public readonly StrView SpecularPath;
            public readonly StrView NormalPath;
            public readonly StrView EmissivePath;
            public readonly Vector3 AmbientColor;
            public readonly Vector3 DiffuseColor;
            public readonly Vector3 SpecularColor;
            public readonly Vector3 EmissiveColor;
            public readonly float Specular;
            public readonly float Alpha;
        }

        struct NanoMeshGroup
        {
            public readonly int GroupId;
            public readonly StrView Name;
            public readonly NanoMaterial Mat;

            public readonly ArrayVec3 Vertices;
            public readonly ArrayVec3 Normals;
            public readonly ArrayVec2 Coords;
            public readonly ArrayInt Indices;

            public readonly Vector3 Offset;
            public readonly Vector3 Rotation; // Euler XYZ DEGREES
            public readonly Vector3 Scale;
        }

        #if UNITY_IOS && !UNITY_EDITOR
            public const string MeshLib = "__Internal";
        #elif UNITY_2018_1_OR_NEWER
		    public const string MeshLib = "FaceOne";
        #else
		    public const string MeshLib = "NanoMeshDynamic";
        #endif

        [return: MarshalAs(UnmanagedType.LPStr)]
        [DllImport(MeshLib)] static extern string NanoGetLastError();

        [DllImport(MeshLib)] static extern 
        NanoMesh* NanoMeshOpen([MarshalAs(UnmanagedType.LPStr)] string fileName, Options options);

        [DllImport(MeshLib)] static extern 
        void NanoMeshClose(NanoMesh* mesh);

        [DllImport(MeshLib)] static extern
        NanoMeshGroup* NanoMeshGetGroup(NanoMesh* mesh, int groupId);

        [DllImport(MeshLib)] static extern 
        NanoMesh* NanoMeshCreateEmpty([MarshalAs(UnmanagedType.LPStr)] string meshName);

        [DllImport(MeshLib)] static extern
        bool NanoMeshSave(NanoMesh* mesh, [MarshalAs(UnmanagedType.LPStr)] string fileName);

        [DllImport(MeshLib)] static extern
        NanoMeshGroup* NanoMeshNewGroup(NanoMesh* mesh, [MarshalAs(UnmanagedType.LPStr)] string groupName);

        /// <summary>
        /// Attempts to load an .OBJ file.
        /// TODO: Currently only 1 meshgroup is supported
        /// </summary>
        /// <param name="meshPath">.OBJ file to load</param>
        /// <param name="options">MeshLoader options control processing steps during loading</param>
        /// <returns>Loaded mesh or throws exception</returns>
        public static Mesh Load(string meshPath, Options options)
        {
            if (!File.Exists(meshPath)) 
                throw new FileNotFoundException($"Mesh file does not exist: {meshPath}");

            NanoMesh* nanoMesh = null;
            try
            {
                nanoMesh = NanoMeshOpen(meshPath, options);
                if (nanoMesh == null)
                    throw new IOException($"Mesh open failed: {meshPath}\n{NanoGetLastError()}");
                
                // @todo We only support 1 meshgroup
                return LoadSubmesh(nanoMesh, 0);
            }
            finally
            {
                if (nanoMesh != null) NanoMeshClose(nanoMesh);
            }
        }

        static Mesh LoadSubmesh(NanoMesh* sdmesh, int index)
        {
            NanoMeshGroup* g = NanoMeshGetGroup(sdmesh, index);
            if (g == null) throw new IndexOutOfRangeException($"Invalid W3D submesh index: {index}");

        #if UNITY_2018_1_OR_NEWER
            var mesh = new Mesh
            {
                name      = g->Name.AsString,
                vertices  = g->Vertices.AsArray,
                normals   = g->Normals.AsArray,
                uv        = g->Coords.AsArray,
                triangles = g->Indices.AsArray
            };
            mesh.RecalculateTangents();
            mesh.RecalculateBounds();
        #else
            var mesh = new Mesh
            {
                Name      = g->Name.AsString,
                Vertices  = g->Vertices.AsArray,
                Normals   = g->Normals.AsArray,
                Coords    = g->Coords.AsArray,
                Triangles = g->Indices.AsArray
            };
        #endif
            return mesh;
        }

        #if UNITY_2018_1_OR_NEWER
        static Vector3[] FromCentimetres(Vector3[] vertices)
        {
            for (int i = 0; i < vertices.Length; i++)
                vertices[i] *= 0.01f;
            return vertices;
        }
        #endif
    }

    #if UNITY_2018_1_OR_NEWER
    // @todo Refactor the interface
    #endif
}
