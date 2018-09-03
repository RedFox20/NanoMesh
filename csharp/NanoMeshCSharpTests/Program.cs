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
                var model = MeshLoader.Load("does_not_exists.obj", new Options());
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
                var o = new Options { ForceSingleGroup = true };
                var model = MeshLoader.Load("head_male.obj", o);
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
        }
    }
}
