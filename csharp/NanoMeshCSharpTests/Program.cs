using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Nano;

namespace NanoMeshCSharpTests
{
    class Program
    {
        private static readonly ConsoleColor DefaultColor = Console.ForegroundColor;
        private static int AssertsFailed;

        static void Print(ConsoleColor color, string message)
        {
            Console.ForegroundColor = color;
            Console.WriteLine(message);
            Console.ForegroundColor = DefaultColor;
        }

        static void Assert(bool condition, string message)
        {
            if (!condition)
            {
                Print(ConsoleColor.Red, $"Assert failed: {message}");
                ++AssertsFailed;
            }
            else
                Print(ConsoleColor.Green, $"Assert passed: {message}");
        }

        static Options DefaultOptions =>
            Options.SingleGroup | Options.SplitSeams | Options.Flatten;

        static void TestMissingMesh()
        {
            try
            {
                var mesh = MeshLoader.Load("does_not_exists.obj", DefaultOptions);
                Assert(false, "Expected exception for files that don't exist");
            }
            catch (Exception e)
            {
                Assert(true, $"Caught expected error {e.Message}");
            }
        }

        static void TestMeshLoader()
        {
            try
            {
                var mesh = MeshLoader.Load("head_male.obj", DefaultOptions);
                Assert(mesh != null, "MeshLoader.Load(head_male.obj) not null");
                Assert(mesh.Vertices.Length > 0, $"Mesh has vertices {mesh.Vertices.Length}");
                Assert(mesh.Normals.Length > 0,  $"Mesh has normals  {mesh.Normals.Length}");
                Assert(mesh.Coords.Length > 0,   $"Mesh has coords   {mesh.Coords.Length}");
                Assert(mesh.Triangles.Length > 0, $"Mesh has triangles {mesh.Triangles.Length}");
            }
            catch (Exception e)
            {
                Assert(false, $"Unexpected failure {e}");
            }
        }

        static void Main(string[] args)
        {
            Console.WriteLine("=== NanoMesh C# Tests ===");
            TestMissingMesh();
            TestMeshLoader();

            if (AssertsFailed > 0)
                Print(ConsoleColor.Red, $"\n{AssertsFailed} asserts failed");
            else
                Print(ConsoleColor.Green, "\nAll asserts passed!");

            if (Debugger.IsAttached)
            {
                Console.Write("\nPress any key to continue . . . ");
                Console.ReadKey(true);
            }
        }
    }
}
