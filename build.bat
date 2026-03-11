@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake --build "c:\Users\marcd\source\repos\SkyrimNVDA\build\debug" --config Debug
