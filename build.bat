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

msbuild /p:Configuration=Debug /p:Platform=Win32
msbuild /p:Configuration=Release /p:Platform=Win32
msbuild /p:Configuration=Debug /p:Platform=x64
msbuild /p:Configuration=Release /p:Platform=x64

popd


REM Building distribs

mkdir dist
pushd dist

call :build_dist 32 ..\build\product\Release.Win32.v14.0
call :build_dist 64 ..\build\product\Release.x64.v14.0

pushd 32 && zip -r ..\GitAutocomplete-32.zip . && popd
pushd 64 && zip -r ..\GitAutocomplete-64.zip . && popd
pushd universal && zip -r ..\GitAutocomplete-universal.zip . && popd

call :build_plugring 32 ..\build\product\Release.Win32.v14.0
call :build_plugring 64 ..\build\product\Release.x64.v14.0

popd

goto :EOF

:build_dist
set BITNESS=%1
set BUILD_DIR=%2

set DST=%BITNESS%

xcopy /i %BUILD_DIR%\plugins\GitAutocomplete %DST%\Plugins\GitAutocomplete
del %DST%\Plugins\GitAutocomplete\*.map
del %DST%\Plugins\GitAutocomplete\*.pdb
xcopy ..\SampleMacro.lua %DST%\Macros\scripts\
ren %DST%\Macros\scripts\SampleMacro.lua GitAutocomplete.lua
xcopy /i /e /y %DST% universal

goto :EOF

:build_plugring
set BITNESS=%1
set BUILD_DIR=%2

set DST=%BITNESS%-plugring

xcopy /i %BUILD_DIR%\plugins\GitAutocomplete %DST%
del %DST%\*.map
del %DST%\*.pdb
pushd %DST% && zip -r ..\GitAutocomplete-%BITNESS%-plugring.zip . && popd

goto :EOF
