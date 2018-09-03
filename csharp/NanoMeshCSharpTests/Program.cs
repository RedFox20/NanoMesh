using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Nano;

namespace NanoMeshCSharpTests
{
    class Program
    {
        static void TestMissingMesh()
        {
            try
            {
                var mesh = MeshLoader.Load("does_not_exists.obj");
            }
            catch (Exception e)
            {
                Console.WriteLine(e);
            }
        }

        static void TestMeshLoader()
        {
            try
            {
                var mesh = MeshLoader.Load("head_male.obj");
            }
            catch (Exception e)
            {
                Console.WriteLine(e);
            }
        }

        static void Main(string[] args)
        {
            TestMissingMesh();
            TestMeshLoader();
            Console.ReadKey(true);
        }
    }
}
