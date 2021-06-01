@echo off

if not "%Platform%" == "X64" if not "%Platform%" == "x64" call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set WarningsFlags=-W4 -WX -wd4062 -wd4514 -wd4710 -wd4711 -wd4820 -wd5045

set CompilerFlags=-arch:AVX2 -EHa- -FC -fp:except- -fp:fast -Gm- -GR- -MT -nologo -O2 -Oi -Zo -Z7
REM Set PALETTIZE_CIELAB to zero to use sRGB colors instead
set CompilerFlags=-DPALETTIZE_CIELAB=1 -DPALETTIZE_DEBUG=0 -D_CRT_SECURE_NO_WARNINGS %CompilerFlags%

set LinkerFlags=-incremental:no -opt:ref

if not exist dist mkdir dist
pushd dist

cl %WarningsFlags% %CompilerFlags% ..\src\palettize.cpp /link %LinkerFlags%

popd
