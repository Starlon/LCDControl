
import subprocess
import os
import glob
import sys

def recursiveDirs(root) :
	return filter( (lambda a : a.rfind( "CVS")==-1 ),  [ a[0] for a in os.walk(root)]  )

def unique(list) :
	return dict.fromkeys(list).keys()

def scanFiles(dir, accept=["*.cpp"], reject=[]) :
	sources = []
	paths = recursiveDirs(dir)
	for path in paths :
		for pattern in accept :
			sources+=glob.glob(path+"/"+pattern)
	for pattern in reject :
		sources = filter( (lambda a : a.rfind(pattern)==-1 ),  sources )
	return unique(sources)

def subdirsContaining(root, patterns):
	dirs = unique(map(os.path.dirname, scanFiles(root, patterns)))
	dirs.sort()
	return dirs

magickCflags, magickCflagse = subprocess.Popen(["Magick++-config", "--cppflags"], stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

xmms2Cflags, xmms2Cflagse = subprocess.Popen(["pkg-config", "--cflags", "xmms2-client-cpp"],
	stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

visualCflags, visualCflagse = subprocess.Popen(["pkg-config", "--cflags", "libvisual-0.4"],
	stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

sdlCflags, sdlCflagse = subprocess.Popen(["pkg-config", "--cflags", "sdl"],
	stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

qjsonCflags, qsonCflagse = subprocess.Popen(["pkg-config", "--cflags", "QJson"],
    stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

gtkCflags, gtkCflagse = subprocess.Popen(["pkg-config", "--cflags", "gtk-2.0"],
    stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

gdkCflags, gdkCflagse = subprocess.Popen(["pkg-config", "--cflags", "gdk-2.0"],
    stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

atkCflags, atkCflagse = subprocess.Popen(["pkg-config", "--cflags", "atk"],
    stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

x11Cflags, x11Cflagse = subprocess.Popen(["pkg-config", "--cflags", "x11"],
    stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

output = magickCflags + " " + xmms2Cflags + " " + visualCflags + " " + sdlCflags + " " + qjsonCflags + " " + x11Cflags

env = Environment(tools=['default', 'qt4'], 
	CCFLAGS = Split("-g -O0 -Wall -std=c++0x -Ijsoncpp/include/ -I/usr/include/xmms2/") + Split(output)) 

magickLdflags, magickLdflagse = subprocess.Popen(["Magick++-config", "--libs"], 
	stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()
print "Hmm " + magickLdflags

xmms2Ldflags, xmms2Ldflagse = subprocess.Popen(["pkg-config", "--libs", "xmms2-client-cpp"],
	stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

visualLdflags, visualLdflagse = subprocess.Popen(["pkg-config", "--libs", "libvisual-0.4"],
	stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

sdlLdflags, sdlLdflagse = subprocess.Popen(["pkg-config", "--libs", "sdl"],
	stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

qjsonLdflags, qsonLdflagse = subprocess.Popen(["pkg-config", "--libs", "QJson"],
    stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

gtkLdflags, gtkLdflagse = subprocess.Popen(["pkg-config", "--libs", "gtk-2.0"],
    stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()

x11Ldflags, x11Ldflagse = subprocess.Popen(["pkg-config", "--libs", "x11"],
    stderr=subprocess.PIPE, stdout=subprocess.PIPE).communicate()


output = magickLdflags + " " + xmms2Ldflags + " " + visualLdflags + " " + sdlLdflags + " " + qjsonLdflags + " " + x11Ldflags

libs = env.ParseFlags(["jsoncpp/libs/linux-gcc-4.6.1/libjson_linux-gcc-4.6.1_libmt.a",'-lboost_regex', '-lusb-1.0', '-lX11'] + Split(output))
env.MergeFlags(libs)

#Valid Qt4 modules are: ['QtCore', 'QtGui', 'QtOpenGL', 'Qt3Support', 
#'QtAssistant', 'QtScript', 'QtDBus', 'QtSql', 'QtNetwork', 'QtSvg', 
#'QtTest', 'QtXml', 'QtXmlPatterns', 'QtUiTools', 'QtDesigner', 
#'QtDesignerComponents', 'QtWebKit', 'QtHelp', 'QtScript']:

env.EnableQt4Modules([
	"QtCore",
	"QtGui",
	"QtScript"
	])

libs_env = env.Clone()

files = Split("""
LCDControl.cpp 
Main.cpp
LCDCore.cpp
GenericSerial.cpp 
BufferedReader.cpp 
QtDisplay.cpp
DrvQt.cpp
DrvQtGraphic.cpp
QtGraphicDisplay.cpp
DrvCrystalfontz.cpp
DrvPertelian.cpp
DrvPicoGraphic.cpp
DrvSDL.cpp
DrvLCDProc.cpp
Evaluator.cpp
LCDWrapper.cpp
LCDText.cpp
LCDGraphic.cpp
debug.cpp
qprintf.cpp
SpecialChar.cpp
Property.cpp
CFG.cpp
RGBA.cpp
Widget.cpp
WidgetText.cpp
WidgetIcon.cpp
WidgetBar.cpp
WidgetHistogram.cpp
WidgetBignums.cpp
WidgetKey.cpp
WidgetTimer.cpp
WidgetScript.cpp
Hash.cpp
Font_8x16.cpp
Font_6x8.cpp
Font_6x8_bold.cpp
PluginLCD.cpp
XmmsQt4.cpp
""")

if not magickCflagse and not magickLdflagse:
	files += ["WidgetGif.cpp"]

if not (visualCflagse or visualLdflagse) and not (xmms2Cflagse or xmms2Ldflagse):
	files += ["WidgetVisualization.cpp"]

interfaces = [env.Uic4(uic) for uic in scanFiles("./", ['*.ui'])]

print interfaces

LCDControl = env.Program(files)

libPluginUptime = libs_env.SharedLibrary(['PluginUptime.cpp', 'Evaluator.cpp', 'debug.cpp'])
libPluginCpuinfo = libs_env.SharedLibrary(['PluginCpuinfo.cpp', 'Evaluator.cpp', 
    'Hash.cpp', 'debug.cpp'])
libPluginNetDev = libs_env.SharedLibrary(['PluginNetDev.cpp', 'Evaluator.cpp', 
    'Hash.cpp', 'qprintf.cpp', 'debug.cpp'])
libPluginMeminfo = libs_env.SharedLibrary(['PluginMeminfo.cpp', 
    'Evaluator.cpp', 'Hash.cpp', 'debug.cpp'])
libPluginUname = libs_env.SharedLibrary(['PluginUname.cpp', 'Evaluator.cpp', 'debug.cpp'])
libPluginProcStat = libs_env.SharedLibrary(['PluginProcStat.cpp', 
    'Evaluator.cpp', 'Hash.cpp', 'qprintf.cpp', 'debug.cpp'])
libPluginNetinfo = libs_env.SharedLibrary(['PluginNetinfo.cpp', 
    'Evaluator.cpp', 'qprintf.cpp', 'debug.cpp'])
libPluginStatfs = libs_env.SharedLibrary(['PluginStatfs.cpp', 'Evaluator.cpp', 
    'debug.cpp'])
libPluginFile = libs_env.SharedLibrary(['PluginFile.cpp', 'Evaluator.cpp', 'debug.cpp'])
libPluginDiskstats = libs_env.SharedLibrary(['PluginDiskstats.cpp', 
    'Evaluator.cpp', 'Hash.cpp', 'debug.cpp'])
libPluginFifo = libs_env.SharedLibrary(['PluginFifo.cpp', 'Evaluator.cpp', 
    'CFG.cpp', 'debug.cpp'])
libPluginLoadavg = libs_env.SharedLibrary(['PluginLoadavg.cpp', 
    'Evaluator.cpp', 'debug.cpp'])
libPluginNetStat = libs_env.SharedLibrary(['PluginNetStat.cpp', 
    'Evaluator.cpp', 'Hash.cpp', 'qprintf.cpp', 'debug.cpp'])
libPluginTime = libs_env.SharedLibrary(['PluginTime.cpp', 'Evaluator.cpp', 'debug.cpp'])
libPluginExec = libs_env.SharedLibrary(['PluginExec.cpp', 'qprintf.cpp', 
    'Hash.cpp', 'debug.cpp'])

libs_env.Install("plugins", [libPluginUptime, libPluginCpuinfo, libPluginNetDev,
    libPluginMeminfo, libPluginUname, libPluginProcStat, libPluginNetinfo,
    libPluginStatfs, libPluginFile, libPluginDiskstats, libPluginFifo,
    libPluginLoadavg, libPluginNetStat, libPluginTime, libPluginExec])
