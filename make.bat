@echo off
setlocal
setlocal enableextensions
setlocal enabledelayedexpansion

set NAME=Demo
set HLSLC=dxc.exe /Ges /O3 /WX /nologo
set SHADER_BEGIN=0
set SHADER_END=0

for /L %%G in (%SHADER_BEGIN%,1,%SHADER_END%) do (
    if exist Data\Shaders\%%G.vs.cso del Data\Shaders\%%G.vs.cso
    if exist Data\Shaders\%%G.ps.cso del Data\Shaders\%%G.ps.cso
    %HLSLC% /D VS_%%G /E vertexMain /Fo Data\Shaders\%%G.vs.cso /T vs_6_0 %NAME%.hlsl & if !ERRORLEVEL! neq 0 (goto :end)
    %HLSLC% /D PS_%%G /E pixelMain /Fo Data\Shaders\%%G.ps.cso /T ps_6_0 %NAME%.hlsl & if !ERRORLEVEL! neq 0 (goto :end)
)

set RELEASE=/Zi /O2 /DNDEBUG /MT
set DEBUG=/Zi /Od /D_DEBUG /MTd
if not defined CONFIG set CONFIG=%DEBUG%
::if not defined CONFIG set CONFIG=%RELEASE%
set CFLAGS=%CONFIG% /W4 /EHa- /GR- /Gw /Gy /nologo /Iexternal /wd4238 /wd4324 /wd4530

if exist %NAME%.exe del %NAME%.exe

if not exist External.pch (cl %CFLAGS% /c /YcExternal.h External.cpp)
cl %CFLAGS% /YuExternal.h Main.cpp /link /incremental:no /opt:ref External.obj kernel32.lib user32.lib gdi32.lib /out:%NAME%.exe

if exist Main.obj del Main.obj
if "%1" == "run" if exist %NAME%.exe %NAME%.exe
:end
