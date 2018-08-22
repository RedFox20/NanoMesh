import mama
class FbxSdk(mama.BuildTarget):

    def dependencies(self):
        self.nothing_to_build()
    
    def package(self):
        self.export_libs('lib', ['.lib', '.a', '.dll', '.so'], src_dir=True)
        self.export_include('include', build_dir=False)
