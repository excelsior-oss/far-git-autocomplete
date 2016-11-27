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

msbuild /p:Configuration=Debug /p:Platform=x86
msbuild /p:Configuration=Release /p:Platform=x86
msbuild /p:Configuration=Debug /p:Platform=x64
msbuild /p:Configuration=Release /p:Platform=x64


REM Building distribs

mkdir dist
pushd dist

mkdir 32
xcopy /i ..\build\product\Release.Win32.v14.0\plugins\GitAutoComp 32\GitAutoComp
del 32\GitAutoComp\*.map 32\GitAutoComp\*.pdb

mkdir 64
xcopy /i ..\build\product\Release.x64.v14.0\plugins\GitAutoComp 64\GitAutoComp
del 64\GitAutoComp\*.map 64\GitAutoComp\*.pdb

mkdir universal
xcopy /i /e /y 32 universal
xcopy /i /e /y 64 universal

popd
