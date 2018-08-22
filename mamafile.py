import mama
class NanoMesh(mama.BuildTarget):

    # global workspace is used for sharing intermediate build
    # products between multiple projects
    global_workspace = 'mama_wolf3d'

    def enable_fbxsdk(self):
        return not ('NO_FBX' in self.args) and self.windows

    def dependencies(self):
        self.add_git('ReCpp', 'https://github.com/RedFox20/ReCpp.git')
        if self.enable_fbxsdk():
            self.add_local('FbxSdk', 'src/FBX')

    def configure(self):
        if self.enable_fbxsdk(): self.add_cmake_options('NANO_ENABLE_FBX=ON')
        if self.config.test:     self.add_cmake_options('NANO_BUILD_TESTS=ON')

    def package(self):
        self.export_include('include')
        self.export_libs('lib', ['NanoMesh.lib', 'libNanoMesh.a'], src_dir=True)
        if self.enable_fbxsdk():
            if self.windows:
                self.export_libs('lib', ['libfbxsdk.dll'], src_dir=True)
        #if self.windows:
        #    self.export_syslib('')

    def test(self, args):
        self.gdb(f'bin/NanoMeshTests {args}', src_dir=True)
        