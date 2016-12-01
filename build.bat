REM Building libgit2

pushd libgit2

mkdir build_32
pushd build_32

cmake -DBUILD_CLAR=OFF -DLIBGIT2_FILENAME=git2_32 -G "Visual Studio 14" ..
cmake --build . --config Release
cmake --build . --config Debug

popd

mkdir build_64
pushd build_64

cmake -DBUILD_CLAR=OFF -DLIBGIT2_FILENAME=git2_64 -G "Visual Studio 14 Win64" ..
cmake --build . --config Release
cmake --build . --config Debug

popd

popd


REM Building plugin

pushd src

msbuild /p:Configuration=Debug /p:Platform=x86
msbuild /p:Configuration=Release /p:Platform=x86
msbuild /p:Configuration=Debug /p:Platform=x64
msbuild /p:Configuration=Release /p:Platform=x64

popd


REM Building distribs

mkdir dist
pushd dist

call :build_dist 32 ..\build\product\Release.Win32.v14.0
call :build_dist 64 ..\build\product\Release.x64.v14.0

pushd 32 && zip -r ..\GitAutoComp-32.zip . && popd
pushd 64 && zip -r ..\GitAutoComp-64.zip . && popd
pushd universal && zip -r ..\GitAutoComp.zip . && popd

popd

goto :EOF

:build_dist
set BITNESS=%1
set BUILD_DIR=%2

xcopy /i %BUILD_DIR%\plugins\GitAutoComp %BITNESS%\Plugins\GitAutoComp
del %BITNESS%\Plugins\GitAutoComp\*.map
del %BITNESS%\Plugins\GitAutoComp\*.pdb
xcopy ..\SampleMacro.lua %BITNESS%\Macros\scripts\
ren %BITNESS%\Macros\scripts\SampleMacro.lua GitAutoComp.lua
xcopy /i /e /y %BITNESS% universal

goto :EOF
