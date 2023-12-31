@echo off

pushd build\
attrib +r *.exe
del /q * > nul 2> nul
attrib -r *.exe
popd
