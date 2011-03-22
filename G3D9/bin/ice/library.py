# library.py
#
# Definition of C libraries available for linking

from .topsort import *
from .utils import *
from .variables import *
from platform import machine
import os, glob

# Files can trigger additional linker options.  This is used to add
# libraries to the link list based on what is #included.  Used by
# getLinkerOptions.

STATIC    = 1      # .a or .lib
DYNAMIC   = 2      # .so or .dll, or .dylib
FRAMEWORK = 3      # .dylib in a framework directory

class Library:
    name             = None
    
    # STATIC or DYNAMIC.  If it only exists as a framework, then FRAMEWORK
    type             = STATIC
    releaseLib       = None
    debugLib         = None

    # None if there is no OS X framework.  If there is a framework,
    # it will be preferred on OS X
    releaseFramework = None
    debugFramework   = None

    # List of all headers that should trigger a link
    # to this library
    headerList       = None

    # If any of these symbols are unbound in the final object files then
    # trigger a link to this library.  This helps us find dependencies
    # of static libraries, which are otherwise not revealed by headers.
    symbolList       = None

    # Other libraries, by canonical name, on which this library depends.
    # This is both a table of known dependencies and input to the topological
    # sort for link ordering
    dependsOnList    = None

    # If true, when deployed this dynamic lib or framework should be copied
    # to the user's machine
    deploy           = None

    def __init__(self, name, type, releaseLib, debugLib, 
                 releaseFramework, debugFramework, headerList,
                 symbolList, dependsOnList, deploy = True):
        self.name             = name
        self.type             = type
        self.releaseLib       = releaseLib
        self.debugLib         = debugLib
        self.releaseFramework = releaseFramework
        self.debugFramework   = debugFramework
        self.headerList       = headerList
        self.symbolList       = symbolList
        self.dependsOnList    = dependsOnList

        if type == STATIC:
            # never need to deploy static libraries
            deploy = False

        self.deploy           = deploy
        
    def __str__(self):
        return '$' + self.name + '$'
#
# Create a table mapping canonical library names to descriptions of the library
#
libraryTable = {}

# header name to lists of canonical names of libraries
headerToLibraryTable = {}

# symbol name to lists of canonical names of libraries
symbolToLibraryTable = {}

def defineLibrary(lib):
    global libraryTable, headerToLibraryTable, symbolToLibraryTable

    if (verbosity >= TRACE): print('defineLibrary() adding ' + str(lib) + ' to libraryTable.')
    
    if lib.name in libraryTable:
        colorPrint("ERROR: Library '" + lib.name + "' defined twice.", WARNING_COLOR)
        sys.exit(-1)

    libraryTable[lib.name] = lib

    for header in lib.headerList:
        if header in headerToLibraryTable:
            headerToLibraryTable[header] += [lib.name]
        else:
            headerToLibraryTable[header] = [lib.name]

    for symbol in lib.symbolList:
        if symbol in symbolToLibraryTable:
           symbolToLibraryTable[symbol] += [lib.name]
        else:
           symbolToLibraryTable[symbol] = [lib.name]

isOSX = (os.uname()[0] == 'Darwin')

# On non-OSX unix systems G3D needs X11 and SDL.  On OS X, GL is a framework
if not isOSX:
    maybeG3DX11 = ['X11']
    maybeG3DSDL = ['SDL']
    maybeAppleGL = []
    maybeFwk    = DYNAMIC
else:
    maybeG3DSDL = []
    maybeG3DX11 = []
    maybeAppleGL = ['AppleGL']
    maybeFwk    = FRAMEWORK


GLG3DDepend = ['G3D', 'OpenGL', 'GLU', 'FFMPEG-util', 'FFMPEG-codec', 'FFMPEG-format', 'FFMPEG-swscale'] + maybeG3DSDL + maybeAppleGL

# OS X frameworks are automatically ignored on linux
for lib in [
#       Canonical name  Type       Release    Debug      F.Release   F.Debug  Header List       Symbol list                                    Depends on    Deploy?
Library('SDL',         maybeFwk,  'SDL',     'SDL',     'SDL',      'SDL',    ['SDL.h'],        ['SDL_GetMouseState'],                         ['OpenGL', 'Cocoa', 'pthread'], True),
Library('curses',      DYNAMIC,   'curses',  'curses',   None,       None,    ['curses.h'],     [],                                            [],           False),
Library('zlib',        DYNAMIC,   'z',       'z',        None,       None,    ['zlib.h'],       ['compress2'],                                 [],           False),
Library('zip',         STATIC,    'zip',     'zip',      None,       None,    ['zip.h'],        ['zip_close'],                                  ['zlib'],    False),
Library('glut',        maybeFwk,  'glut',    'glut',    'GLUT',     'GLUT',   ['glut.h'],       [],                                            [],           False),
Library('OpenGL',      maybeFwk,  'GL',      'GL',      'OpenGL',   'OpenGL', ['gl.h'],         ['glBegin', 'glVertex3'],                      [],           False),
Library('jpeg',        DYNAMIC,   'jpeg',    'jpeg',     None,       None,    ['jpeg.h'],       ['jpeg_memory_src', 'jpeg_CreateCompress'],    []),
Library('png',         DYNAMIC,   'png',     'png',      None,       None,    ['png.h'],        ['png_create_info_struct'],                    []),
Library('GLU',         maybeFwk,  'GLU',     'GLU',      None,       None,    ['glu.h'],        ['gluBuild2DMipmaps'],                         ['OpenGL'],   False),
Library('Cocoa',       FRAMEWORK,  None,      None,     'Cocoa',    'Cocoa',  ['Cocoa.h'],      ['DebugStr'],                                  [],           False),
Library('Carbon',      FRAMEWORK,  None,      None,     'Carbon',   'Carbon', ['Carbon.h'],     ['ShowWindow'],                                [],           False),
Library('AppleGL',     FRAMEWORK,  None,      None,     'AGL',      'AGL',    ['agl.h'],        ['_aglChoosePixelFormat'],                     [],           False),
Library('G3D',         STATIC,    'G3D',     'G3Dd',     None,       None,    ['G3D.h'], [],                                                   ['zlib', 'jpeg', 'png', 'zip', 'Cocoa', 'pthread', 'Carbon'] + maybeG3DX11),
Library('GLG3D',       STATIC,    'GLG3D',   'GLG3Dd',   None,       None,    ['GLG3D.h', 'RenderDevice.h'],      [],                          GLG3DDepend),
Library('pthread',     DYNAMIC,   'pthread', 'pthread',  None,       None,    ['pthread.h'],    [],                                            [],           False),
Library('math',        DYNAMIC,   'm', 'm',  None,       None,       [],    [],                                            [],           False),
Library('QT',          DYNAMIC,   'qt-mt',   'qt-mt',    None,       None,    ['qobject.h'],    [],                                            [],           True),
Library('IOKit',       FRAMEWORK,  None,     None,       'IOKit',    'IOKit', ['IOHIDKeys.h', 'IOKitLib.h', 'IOHIDLib.h'],  ['IOMasterPort'],  [],           False),
Library('X11',         DYNAMIC,   'X11',     'X11',      None,       None,    ['x11.h'],        ['XSync', 'XFlush'],                           [],           False),
Library('ANN',         STATIC,    'ANN',     'ANN',      None,       None,    ['ANN.h'],        [],                                            []),
Library('OpenCV',      STATIC,    'cv',      'cv',       None,       None,    ['cv.h'],         [],                                            ['OpenCV-Aux', 'OpenCV-Core']),
Library('OpenCV-Aux',  STATIC,    'cvaux',   'cvaux',    None,       None,    [],               [],                                            ['OpenCV-Core']),
Library('OpenCV-Core', STATIC,    'cxcore',  'cxcore',   None,       None,    [],               [],                                            []),
Library('FFMPEG-util', DYNAMIC,    'avutil',  'avutil',  None,       None,    ['avutil.h'],     ['av_malloc'],                                 []),
Library('FFMPEG-codec', DYNAMIC,   'avcodec', 'avcodec', None,       None,    ['avcodec.h'],    ['avcodec_open'],                              ['zlib']),
Library('FFMPEG-format', DYNAMIC,  'avformat','avformat',None,       None,    ['avformat.h'],   ['av_register_all'],                           ['FFMPEG-util']),
Library('FFMPEG-swscale', DYNAMIC, 'swscale', 'swscale', None,       None,    ['swscale.h'], ['sws_scale'], ['FFMPEG-util']),
Library('FMOD',        DYNAMIC,   'fmodex',  'fmodex',   None,       None,    ['fmod.hpp', 'fmod.h'], [],                                      ['FFMPEG-codec', 'FFPMEG-util']),
Library('mongoose',    STATIC,    'mongoose', 'mongoose', None,      None,    ['mongoose.h'],   [], []),
Library('irrKlang',    DYNAMIC,   'irrklang','irrklang', None,       None,    ['irrKlang.h'],   ['createIrrKlangDevice'],                                            [])]:
#Library('Box2D',       STATIC, 'box2d','box2d', None,       None,    ['Box2D.h'],   [],  [])]:
    defineLibrary(lib)

""" Constructs a dictionary mapping a library name to its
    relative dependency order in a library list. """
def _makeLibOrder():
    # Make a set of partial order pairs from the
    # dependencies in the library table
    
    pairs = []
    
    for parent in libraryTable:
        for child in libraryTable[parent].dependsOnList:
            pairs.append( (parent, child) )

    # These control the linker order.  Put the library you want first on the left in the pair and
    # the library you want second on the right
    pairs = [('GLG3D', 'G3D'), ('G3D', 'Cocoa'), ('Cocoa', 'SDL'), ('SDL', 'OpenGL'), ('GLU', 'OpenGL'), 
            ('GLG3D', 'GLU'), ('G3D', 'zlib'), ('G3D', 'zip'), ('G3D', 'png'), ('G3D', 'jpeg'), ('Cocoa', 'pthread'), 
            ('Cocoa', 'zlib'), ('OpenGL', 'png'), ('OpenGL', 'jpeg'), ('OpenGL', 'pthread'), ('Cocoa', 'Carbon'),
            ('FFMPEG-format', 'FFMPEG-codec'), ('FFMPEG-codec', 'FFMPEG-util'), ('FFMPEG-format', 'zlib'), 
            ('GLG3D', 'FFMPEG-format')]

    E, V = pairsToVertexEdgeGraph(pairs)
    L = topSort(E, V)

    order = {}
    for i in range(0, len(L)):
        order[L[i]] = i

    return order

_libOrder = None

""" Sort predicate for library dependencies. """
def _libSorter(x, y):
    global _libOrder

    hasX = x in _libOrder
    hasY = y in _libOrder

    if hasX and hasY:
        return _libOrder[x] - _libOrder[y]

    elif hasX:
        # x is known, y is known.  Decide that x > y to put all unknown libraries first
        # since the known libraries can't depend on them (known libraries
        # have known dependencies)
       return 1

    elif hasY:

       return -1

    else:

       # Two libraries with no known dependencies; put them in alphabetical order
       # (since we have no other metric!) to guarantee a stable sort
       return cmp(x, y)

"""
Converts a cmp= function into a key= function
"""
def cmp2key(cmpf):
    class K:
        def __init__(self, obj, *args):
            self.obj = obj
        def __lt__(self, other):
            return (cmpf(self.obj, other.obj) < 0)
    return K

"""
Accepts a list of library canonical names and sorts it in place.
"""
def sortLibraries(liblist):
    global _libOrder
    _libOrder = _makeLibOrder()
    liblist.sort(key=cmp2key(_libSorter))
    _libOrder = None
    

""" Given a library name (e.g. "G3D") finds the library file and
    returns the fully qualified path to it. 

    type must be STATIC or DYNAMIC
"""
def findLibrary(_lfile, type, libraryPaths):
    ext = '.a'
    ext2 = None

    if type == DYNAMIC:
        ext = '.so'
        if (os.uname()[0] == 'Darwin'):
            ext2 = '.dylib'

    lfile = 'lib' + _lfile + ext
    lfile2 = None
    if ext2 != None: lfile2 = 'lib' + _lfile + ext2

    # Find the library 
    for path in libraryPaths:
        if os.path.exists(pathConcat(path, lfile)):
            return pathConcat(path, lfile)
        elif (lfile2 != None) and os.path.exists(pathConcat(path, lfile2)):
            return pathConcat(path, lfile2)

    # We couldn't find the library.  Try looking for the library
    # with a version number appended.
    wildlfile = 'lib' + _lfile + '-*' + ext
    wildlfile2 = None
    if ext != None: wildlfile2 = 'lib' + _lfile + '-*' + ext

    bestVersion = 0
    bestFile = None
    for path in libraryPaths:
        files = glob.glob(pathConcat(path, wildlfile))
        if wildlfile2 != None:
            files += glob.glob(pathConcat(path, wildlfile2))

        # Choose the latest version from those found
        for file in files:

            # Parse the version from the filename
            i = file.rfind('-', 0, -2)
            if i >= 0:
                version = file[(i+1):-3]

                try:
                    version = float(version)
                    if (version > bestVersion):
                        bestVersion = version
                        bestFile = file
                except ValueError:
                    version = -1

   
    if bestFile != None:
        return bestFile

    # Still not found; return the generic name
    return lfile
