#TODO(Vidar) Add option for release build. For now: -Ox, -MD, #-D DYNAMIC
#                                    and for debug: -Od, -MDd, -D DYNAMIC
source_dir=.\\
lib_dir=lib\ 
output_dir=build\ 

release 		 =2016
optimization     = -Od
warnings         = -WX -W4 -wd4201 -wd4100 -wd4189 -wd4127 
include_path = -I"C:\Program Files\Autodesk\3ds Max $(release) SDK\maxsdk\include" -I$(source_dir)
lib_path     = -LIBPATH:"C:\Program Files\Autodesk\3ds Max $(release) SDK\maxsdk\lib\x64\Release"
defines      = -D "_USRDLL" -D "WINVER=0x0502" -D "_WIN32_WINNT=0x0502" -D "_WIN32_WINDOWS=0x0502" -D "_WIN32_IE=0x0800" -D "_WINDOWS" -D "_CRT_SECURE_NO_DEPRECATE" -D "_CRT_NONSTDC_NO_DEPRECATE" -D "_SCL_SECURE_NO_DEPRECATE" -D "_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1" -D "_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT =1" -D "_CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES=1" -D "ISOLATION_AWARE_ENABLED=1" -D "_DEBUG" -D "WIN32" -D "WIN64" -D "_WINDLL" -D "_UNICODE" -D "UNICODE"
compiler_flags = -MP -GS -W4 -Zc:wchar_t  -Zi -Gm- -fp:fast -errorReport:prompt -Zc:forScope -GR -Gd -MDd -FC  -EHa -nologo -DEBUG
linker_flags = -MANIFEST -NXCOMPAT -DYNAMICBASE:NO -DEBUG -DLL -MACHINE:X64 -SUBSYSTEM:WINDOWS -ERRORREPORT:PROMPT -NOLOGO -TLBID:1 
plugin_dir ="C:\Program Files\Autodesk\3ds Max $(release)\plugins\\"
libs = bmm.lib core.lib geom.lib gfx.lib mesh.lib maxutil.lib maxscrpt.lib manipsys.lib paramblk2.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib
options = -D MAX_REL_$(release) -D DYNAMIC 


update:dynamic

all:cacher dynamic

cacher_dll = Cacher
cacher_cpp = $(source_dir)Cacher.cpp $(source_dir)common.cpp $(source_dir)DllEntry.cpp $(source_dir)Dynamic.cpp $(lib_dir)lz4.c
cacher_def = $(source_dir)Cacher.def 
cacher_out = $(plugin_dir)$(cacher_dll).dlo
cacher_defines = -D "MODULE_NAME=$(cacher_dll).dll" $(defines)

dynamic_dll = cacher_dynamic
dynamic_cpp = $(source_dir)Dynamic.cpp $(source_dir)dllmain.cpp $(lib_dir)lz4.c
dynamic_def = $(source_dir)Dynamic.def 
dynamic_out = $(plugin_dir)$(dynamic_dll).dll
dynamic_defines = -D "MODULE_NAME=$(dynamic_dll).dll" $(defines)

cacher:
	@rc $(source_dir)Cacher.rc
	@cl $(compiler_flags) $(optimization) $(warnings) $(options) $(include_path) $(cacher_defines) $(cacher_cpp) /link $(linker_flags) $(lib_path) $(libs) $(source_dir)Cacher.res -DEF:$(cacher_def) -out:$(cacher_out)

dynamic:
	@cl $(compiler_flags) $(optimization) $(warnings) $(options) $(include_path) $(dynamic_defines) $(dynamic_cpp) /link $(linker_flags) $(lib_path) $(libs) -DEF:$(dynamic_def) -out:$(dynamic_out)
