cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL adler32.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL compress.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL crc32.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL gzio.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL uncompr.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL deflate.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL trees.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL zutil.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL inflate.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL infblock.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL inftrees.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL infcodes.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL infutil.c
cl -c  -D_MT -D_DLL -MD -O -nologo -DZLIB_DLL inffast.c
link  /INCREMENTAL:NO /NOLOGO -entry:_DllMainCRTStartup@12 -dll -out:zlib.dll -def:zlib.dnt adler32.obj compress.obj crc32.obj gzio.obj uncompr.obj deflate.obj  trees.obj zutil.obj inflate.obj infblock.obj inftrees.obj infcodes.obj  infutil.obj inffast.obj kernel32.lib advapi32.lib user32.lib gdi32.lib comdlg32.lib winspool.lib
del *.obj
move *.dll ..