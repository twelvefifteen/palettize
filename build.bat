@echo off

if not "%Platform%" == "X64" if not "%Platform%" == "x64" call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set BaseName=palettize

set WarningsFlags=-W4 -WX -wd4062 -wd4189 -wd4505 -wd4514 -wd4710 -wd4711 -wd4820 -wd5045

set CompilerFlags=-arch:AVX2 -EHa- -FC -fp:except- -fp:fast -Gm- -GR- -MT -nologo -Zo -Z7
set CompilerFlags=-D_CRT_SECURE_NO_WARNINGS %CompilerFlags%

set LinkerFlags=-incremental:no -opt:ref

if not exist dist mkdir dist
pushd dist

echo -----------------
echo Building debug...
cl %WarningsFlags% %CompilerFlags% -Fe%BaseName%_debug_msvc.exe -Od -DPALETTIZE_DEBUG=1 ..\src\palettize.cpp /link %LinkerFlags%

echo -----------------
echo Building release...
cl %WarningsFlags% %CompilerFlags% -Fe%BaseName%_release_msvc.exe -Oi -O2 ..\src\palettize.cpp /link %LinkerFlags%

del *.obj

popd
