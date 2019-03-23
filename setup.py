from setuptools import Extension, setup

ext = Extension(
    name='terrain',
    sources=['terrain.cpp'],
)

setup(
    name='terrain',
    version='0.1.0',
    ext_modules=[ext],
)
