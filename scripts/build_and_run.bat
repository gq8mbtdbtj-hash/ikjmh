@echo off
setlocal
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
cd /d "%~dp0.."
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON || exit /b 1
cmake --build build || exit /b 1
echo.
echo Built: %CD%\build\tray_demo.exe
echo Starting tray app (look for icon in notification area)...
start "" "%CD%\build\tray_demo.exe"
endlocal
