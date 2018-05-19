import mama
class NanoMesh(mama.BuildTarget):
    local_workspace = 'build'
    def dependencies(self):
        self.add_git('ReCpp', 'https://github.com/RedFox20/ReCpp.git')
        self.add_local('FbxSdk', 'Nano/FBX')

    def configure(self):
        pass

    #def package(self):
    #    pass