# Third-party components

This application dynamically links unmodified Qt 5.15.2 libraries: Qt Core, Gui, Widgets, Sql,
the Windows platform/style plugins, SQLite driver, and an offscreen plugin used by tests.
Qt copyright belongs to The Qt Company Ltd. and its contributors. Qt is used under LGPL v3.
Full LGPL v3 and GPL v3 texts are in `licenses/`.

You may replace the dynamically linked Qt DLLs with compatible modified versions and debug
those modifications. The full application source and build instructions are available in this repository; no restriction on reverse engineering for debugging
modifications to these LGPL libraries is imposed by this delivery.

Exact upstream Qt base source (5.15.2), including third-party notices and build instructions:
https://download.qt.io/archive/qt/5.15/5.15.2/submodules/qtbase-everywhere-src-5.15.2.tar.xz
https://code.qt.io/cgit/qt/qtbase.git/tree/?h=v5.15.2

Qt base also incorporates third-party components; corresponding copyright and license notices
are available in that exact source tree under `src/3rdparty` and `qt_attribution.json` files.
SQLite is in the public domain. Qt's bundled components include FreeType, HarfBuzz, libpng,
PCRE2, double-conversion and zlib; their upstream notices accompany the Qt source.

The MinGW-w64 GCC 8.1 runtime DLLs (`libgcc_s_seh-1`, `libstdc++-6`, `libwinpthread-1`)
are supplied unchanged. Their licenses and GCC Runtime Library Exception are copied under
`licenses/gcc`, `licenses/mingw-w64`, and `licenses/winpthreads`.
Corresponding GCC source: https://gcc.gnu.org/releases.html
MinGW-w64 source: https://www.mingw-w64.org/

No Qt runtime, compiler runtime, or platform font has been embedded into the application executable.
