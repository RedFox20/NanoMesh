import mama
class NanoMesh(mama.BuildTarget):
    local_workspace = 'build'

    def enable_fbxsdk(self):
        return not 'NO_FBX' in self.args

    def dependencies(self):
        self.add_git('ReCpp', 'https://github.com/RedFox20/ReCpp.git')
        if self.enable_fbxsdk():
            self.add_local('FbxSdk', 'Nano/FBX')

    def configure(self):
        if not self.enable_fbxsdk():
            self.add_cxx_flags('-DNANOMESH_NO_FBX=1')

    def package(self):
        self.export_include('.')
        self.export_libs('.')
