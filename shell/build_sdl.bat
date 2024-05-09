@echo off

cmake >NUL 2>&1
if %ERRORLEVEL%==9009 (
echo Cmake not found.
goto end
)

set CmakeParams=-DSDL_STATIC=OFF -DSDL_SHARED=ON -DFORCE_STATIC_VCRT=ON -DALTIVEC=OFF -DDISCAUDIO=OFF -DHIDAPI=ON -DDIRECTX=OFF -DRENDER_D3D=OFF -DSDL_AUDIO=OFF -DSDL_HAPTIC=OFF -DSDL_JOYSTICK=ON -DSDL_LOADSO=ON -DSDL_RENDER=OFF -DSDL_SENSOR=OFF -DSDL_VIDEO=ON -DVIDEO_KMSDRM=OFF -DVIDEO_OPENGL=ON -DVIDEO_OPENGLES=OFF -DVIDEO_VULKAN=OFF -DWASAPI=OFF -DJOYSTICK_VIRTUAL=ON -D3DNOW=ON -DMMX=ON
echo Generating projects for SDL2 x64 ...
rmdir /S /Q build\sdl_msvc_x64\ > NUL
cmake -Sext/SDL2 -Bbuild/sdl_msvc_x64 -G"Visual Studio 15 2017 Win64" %CmakeParams%

echo Building SDL2 (x64 Debug) ...
cmake --build build/sdl_msvc_x64 --config Debug

echo Building SDL2 (x64 Release) ...
cmake --build build/sdl_msvc_x64 --config Release

mkdir lib\ > NUL
copy /Y build\sdl_msvc_x64\Debug\SDL2d.lib lib\x64__SDL2d.lib
copy /Y build\sdl_msvc_x64\Debug\SDL2d.dll lib\x64__SDL2d.dll
copy /Y build\sdl_msvc_x64\Release\SDL2.lib lib\x64__SDL2.lib
copy /Y build\sdl_msvc_x64\Release\SDL2.dll lib\x64__SDL2.dll

echo Generating projects for SDL2 x86 ...
rmdir /S /Q build\sdl_msvc_x86\ > NUL
cmake -Sext/SDL2 -Bbuild/sdl_msvc_x86 -G"Visual Studio 15 2017" -A Win32 %CmakeParams%

echo Building SDL2 (x86 Debug) ...
cmake --build build/sdl_msvc_x86 --config Debug

echo Building SDL2 (x86 Release) ...
cmake --build build/sdl_msvc_x86 --config Release

mkdir lib\ > NUL
copy /Y build\sdl_msvc_x86\Debug\SDL2d.lib lib\x86__SDL2d.lib
copy /Y build\sdl_msvc_x86\Debug\SDL2d.dll lib\x86__SDL2d.dll
copy /Y build\sdl_msvc_x86\Release\SDL2.lib lib\x86__SDL2.lib
copy /Y build\sdl_msvc_x86\Release\SDL2.dll lib\x86__SDL2.dll
:end


