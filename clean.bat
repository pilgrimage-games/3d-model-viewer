@echo off

pushd build\
attrib +r *.exe
attrib +r *.pga
del /q * > nul 2> nul
attrib -r *.exe
attrib -r *.pga
popd
