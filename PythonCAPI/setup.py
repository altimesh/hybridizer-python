import os
import distutils
from distutils.core import setup, Extension, Distribution

# https://docs.python.org/2/distutils/apiref.html
hybridcuda = Extension('hybridcuda', 
            language='c++',
            extra_compile_args=['/ZI', '/Od', '/EHa'],
            define_macros = [('PYTHONCAPI_EXPORTS',None),('NDEBUG',None)], 
            undef_macros  = [('_DEBUG')],
            extra_link_args= ['/DEBUG','/INCREMENTAL','/OPT:NOLBR','/LTCG:OFF'],

            include_dirs = [ os.environ['CUDA_PATH'] + '/include' ],

            #probably OS-dependent...
            library_dirs = [ os.environ['CUDA_PATH'] + '/lib/x64' ],
            libraries = [ 'cuda', 'nvrtc' ],

            sources = [ \
                'dllmain.cpp', \
                'CUDAPython.cpp', \
                'PythonCAPI.cpp', \
                'Transcoder/Hybridizer.cpp', \
                'Transcoder/HybType.cpp', \
                'Transcoder/CUDAKernel.cpp', \
                'Transcoder/CUDAKernelWriter.cpp', \
                'Transcoder/HybridPythonWriter.cpp', \
                'CUDAJIT.cpp' \
                ])

setup(name='hybridcuda', 
      options = {'build' : {'debug' : True}, 'link' : {'debug' : True}},
      version='0.1',
      description = 'This is hybridcuda',
      author = 'ALTIMESH',
      author_email = 'contact@altimesh.com',
      ext_modules=[hybridcuda])
