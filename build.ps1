#check if vcpkg is bootstrapped, if not bootstrap it
if(!(Test-Path -Path "./vcpkg/vcpkg.exe"))
{
    Start-Process -FilePath "./vcpkg/bootstrap-vcpkg.bat" -Wait
}

$buildcmd = "cmake -S . -B ./build -DCMAKE_TOOLCHAIN_FILE=`"vcpkg/scripts/buildsystems/vcpkg.cmake`""

#run the cmake command
Invoke-Expression $buildcmd

#compile the application
$buildCmd = "cmake --build ./build --config Release"
Invoke-Expression $buildCmd
