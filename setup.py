import os
import re
import sys
import platform
import subprocess
from setuptools import setup, find_packages, Extension
from setuptools.command.build_ext import build_ext
from distutils.version import LooseVersion

class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)
        
class CMakeBuild(build_ext):
    def run(self):
        try:
            out = subprocess.check_output(['cmake', '--version'])
        except OSError:
            raise RuntimeError("CMake must be installed to build the following extensions: " +
                               ", ".join(e.name for e in self.extensions))

        if platform.system() == "Windows":
            raise NotImplementedError

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
        cmake_args = ['-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + extdir,
                      '-DPYTHON_EXECUTABLE=' + sys.executable]

        cfg = 'Release'
        build_args = ['--config', cfg]

        if platform.system() == "Windows":
            raise NotImplementedError
        else:
            cmake_args += ['-DCMAKE_BUILD_TYPE=' + cfg]
            build_args += ['--', '-j']

        self.build_temp = self.build_temp.replace("temp", "lib")

        env = os.environ.copy()
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
            
        vcpkg_dir = os.path.join(self.build_temp, 'vcpkg')
        vcpkg_dir = os.path.abspath(vcpkg_dir)
        if not os.path.exists(vcpkg_dir):
            subprocess.check_call(['git', 'clone', 'https://github.com/microsoft/vcpkg.git', vcpkg_dir])
            subprocess.check_call(['./bootstrap-vcpkg.sh'], cwd=vcpkg_dir)
        
        vcpkg_triplet = 'x64-linux'
        subprocess.check_call(['./vcpkg', 'install',
            'eigen3',
            'spectra',
            'directx-headers',
            'directxmath',
            'directxtex',
            'directxmesh',
                               '--triplet', vcpkg_triplet], cwd=vcpkg_dir)
        subprocess.check_call(['./vcpkg', 'integrate', 'install'], cwd=vcpkg_dir)
      
        cmake_args += [
            '-DCMAKE_TOOLCHAIN_FILE={}/scripts/buildsystems/vcpkg.cmake'.format(vcpkg_dir),
            '-DVCPKG_TARGET_TRIPLET={}'.format(vcpkg_triplet)]
            
        subprocess.check_call(['cmake', ext.sourcedir] + cmake_args, cwd=self.build_temp, env=env)
        subprocess.check_call(['cmake', '--build', '.'] + build_args, cwd=self.build_temp)

setup(
    name='pyatlas',
    version='0.0.1',
    author='Xiang Feng',
    author_email='xfeng.cg@gmail.com',
    description='Minimal wrapper of UVAtlas on linux',
    long_description='',
    ext_modules=[CMakeExtension('pyatlas', '.')],
    cmdclass=dict(build_ext=CMakeBuild),
    zip_safe=False,
)