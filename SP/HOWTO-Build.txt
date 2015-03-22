

***Compiling under Linux (and other *NIX)***: Requires SDL2 development packages...Check with your disto

Type:
make

Default is to build for your architecture, if you want to specify which architecture you want to build for use:
ARCH=x86 make
or
ARCH-x86_64 make

To cross-compile for Windows, use the provided scripts:
./cross-make-mingw.sh
or
./cross-make-mingw64.sh


***Compiling in a Windows environment***:

This is a short tutorial on how to set up a mingw-w64 build environment for iortcw using Cygwin. One of the major downfalls of mingw-w64 is the fragmentation...i.e. lots of different builds, installers, scripts, etc. floating around the internet. I found it very confusing to figure out which version to download and how to install it etc. That's where Cygwin comes to the rescue.

Not only are the Cygwin builds up to date but it is also very easy to install and maintain.

Cygwin is very much like a 'rolling' Linux distribution in that it is always updating packages within itself rather than putting out periodic 'releases'

1. Install Cygwin
First off you want to download the Cygwin setup package from here:
http://cygwin.com/install.html

Choose either the 32-bit or 64-bit environment (32-bit will work fine on both 32 and 64 bit versions of Windows)

A note here about the setup program. This is not the kind of setup program you just delete after installing. This is also your Cygwin environment updater. If you have an existing Cygwin environment, the setup program will, by default, update your existing packages.

Choose where you want to install Cygwin. The entire environment is self-contained in it's own folder, but you can also interact with files from outside the environment if you want to as well.

Default is C:\Cygwin

Choose a mirror to download packages from. (I usually use the kernel.org mirrors)

Choose a "storage area" for your package downloads.

2. Package selection.

The next screen you see will be the package selections screen. In the upper left is a search box. This is where you will want to search for the necessary packages.

These are the package names you'll want to search for:

1.) mingw64-i686-gcc-core (For building 32bit binaries)
2.) mingw64-i686-gcc-g++ (Also for32bit...C++ support)
3.) mingw64-x86_64-gcc-core (For building 64bit binaries)
4.) mingw64-x86_64-gcc-g++ (For 64bit, same as above)
5.) make
6.) bison
7.) git-core

When you search for your packages you'll see category listings. These packages would all be under the 'Devel' category.

To select a package, change the 'Skip' to the version of the package you want to install. The first click will be the newest version and subsequent clicks will allow you to choose older versions of the package. In our case here, you're probably good choosing the latest and greatest.

3. Install packages.

After you have selected your packages, just hit 'Next' in the lower right. Cygwin will automatically add package dependencies. Hit next again to let the install run. Done.

The entire environment uses about 1GB of disk space (as opposed to about 6GB for a Visual Studio install)

4. Using Cygwin to check out and build

After the install has completed you should have a 'Cygwin Terminal' icon on your Desktop. This is the bash shell for Cygwin, so go ahead and run it.

At the command prompt type:
git clone https://github.com/iortcw/iortcw.git iortcw

This will pull the iortcw trunk source.

'cd' into the iortcw folder that was created...

cd iortcw/MP (Multi-Player)
or
cd iortcw/SP (Single-Player)

ARCH=x86 make (to build 32bit binaries)
or
ARCH=x86_64 make (for 64bit binaries)

Wait for build to complete.

Output files will be in the 'build' folder.
(So for a default Cygwin install the path would be: C:\Cygwin\home\Your_Username\iortcw\<game>\build\release-mingw32-<arch>)


***Compiling under Mac***: (Universal binary)...Requires XCode3 to build PPC support, ideally under OSX 10.6.
Use the provided script:
./make-macosx-ub.sh


**Compiling under Mac***: (Terminal based apps)
Use the appropriate build script based on the XCode version installed.

./make-macosx_xcode3.sh <arch> (x86, x86_64, ppc)
./make-macosx_xcode4.sh <arch> (x86, x86_64)
./make-macosx_xcode5.sh <arch> (x86, x86_64)


In all cases, output binaries will be located in the 'build' folder.

