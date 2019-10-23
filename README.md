# NanoMesh [![Build Status](https://travis-ci.org/RedFox20/NanoMesh.svg?branch=master)](https://travis-ci.org/RedFox20/NanoMesh) [![CircleCI](https://circleci.com/gh/RedFox20/NanoMesh.svg?style=svg)](https://circleci.com/gh/RedFox20/NanoMesh) [![Build status](https://ci.appveyor.com/api/projects/status/02dnqbcspoxpxcob/branch/master?svg=true)](https://ci.appveyor.com/project/RedFox20/nanomesh/branch/master)
A minimal OBJ and FBX mesh loading utility written in C++17

This project uses the Mama Build tool: https://github.com/RedFox20/Mama


# Building for Development
1. Get python 3.6
2. `pip install mama`
3. `mama build` - Build the library
4. `mama build linux` - Build the library for a specific platform
5. `mama test` - Run the unit tests
6. `mama open` - Open NanoMesh in your favorite IDE


# Using as a Library
Just add this git repository into your main project mamafile: `{YourProject}/mamafile.py`
```py
import mama
class YourProject(mama.BuildTarget):
    local_workspace = 'build'
    def dependencies(self):
        # If you want to link against FBX sdk on Windows, Mac, iOS, Linux then remove NO_FBX
        self.add_git('NanoMesh', 'https://github.com/RedFox20/NanoMesh.git', args=['NO_FBX'])

```
Read more at: https://github.com/RedFox20/Mama
