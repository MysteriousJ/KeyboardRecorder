@echo off
cl -Zi /EHsc /MT /D"WIN32" "src\main.cpp" /link "opengl32.lib" "glu32.lib" "kernel32.lib" "user32.lib" "gdi32.lib" /SUBSYSTEM:WINDOWS /OUT:"Keyboard Recorder.exe"