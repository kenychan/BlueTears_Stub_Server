@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>nul
cl /nologo /O2 /MT /LD /Fe:Res\stubs\build\NpSlayer_exp.dll /Fo:Res\stubs\build\NpSlayer_exp.obj Res\stubs\NpSlayer_exp.c /link /DEF:Res\stubs\NpSlayer_exp.def /NOLOGO
echo BUILD_EXIT=%errorlevel%
