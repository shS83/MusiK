cd "$env:OneDrive\Code\MusiK\CPP"
$env:Path = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.12.0\mingw_64\bin;$env:Path"
cmake -S . -B build-win-mingw -G Ninja -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-win-mingw
move build-win-mingw\MusikCPP.exe build-win-mingw\MusiK.exe
move build-win-mingw\MusikCpp.exe.manifest build-win-mingw\MusiK.exe.manifest
