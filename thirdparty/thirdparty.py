import os
import platform
import sys
import argparse
import shutil
import subprocess
from enum import Enum, auto
from urllib.request import Request, urlopen
from zipfile import ZipFile
import tarfile
from typing import List, Dict, BinaryIO

# -------------------------------------------------------------------------
# Script made to download thirdparty stuff quickly
# maybe i can improve this later to be able to build it for you automatically
# not everything uses cmake here anyway so
#
# TODO: split up into more files, this is already like a mini build system,
#  but very messy as it's one file
# -------------------------------------------------------------------------

try:
    import py7zr
except ImportError:
    print("Error importing py7zr: install with pip and run this again")
    quit(2)


# Win32 Console Color Printing

_win32_legacy_con = False
_win32_handle = None

if os.name == "nt":
    if platform.release().startswith("10"):
        # hack to enter virtual terminal mode,
        # could do it properly, but that's a lot of lines and this works just fine
        import subprocess

        subprocess.call('', shell=True)
    else:
        import ctypes

        _win32_handle = ctypes.windll.kernel32.GetStdHandle(-11)
        _win32_legacy_con = True


class Color(Enum):
    if _win32_legacy_con:
        RED = "4"
        DGREEN = "2"
        GREEN = "10"
        YELLOW = "6"
        BLUE = "1"
        MAGENTA = "13"
        CYAN = "3"  # or 9

        DEFAULT = "7"
    else:  # ansi escape chars
        RED = "\033[0;31m"
        DGREEN = "\033[0;32m"
        GREEN = "\033[1;32m"
        YELLOW = "\033[0;33m"
        BLUE = "\033[0;34m"
        MAGENTA = "\033[1;35m"
        CYAN = "\033[0;36m"

        DEFAULT = "\033[0m"


class Severity(Enum):
    WARNING = Color.YELLOW
    ERROR = Color.RED


WARNING_COUNT = 0


def warning(*text):
    _print_severity(Severity.WARNING, "\n          ", *text)
    global WARNING_COUNT
    WARNING_COUNT += 1


def error(*text):
    _print_severity(Severity.ERROR, "\n        ", *text, "\n")
    quit(1)


def verbose(*text):
    print("".join(text))


def verbose_color(color: Color, *text):
    print_color(color, "".join(text))


def _print_severity(level: Severity, spacing: str, *text):
    print_color(level.value, f"[{level.name}] {spacing.join(text)}")


def win32_set_fore_color(color: int):
    ctypes.windll.kernel32.SetConsoleTextAttribute(_win32_handle, color)
    # if not ctypes.windll.kernel32.SetConsoleTextAttribute(_win32_handle, color):
    #     print(f"[ERROR] WIN32 Changing Colors Failed, Error Code: {str(ctypes.GetLastError())},"
    #           f" color: {color}, handle: {str(_win32_handle)}")


def stdout_color(color: Color, *text):
    if _win32_legacy_con:
        win32_set_fore_color(int(color.value))
        sys.stdout.write("".join(text))
        win32_set_fore_color(int(Color.DEFAULT.value))
    else:
        sys.stdout.write(color.value + "".join(text) + Color.DEFAULT.value)


def print_color(color: Color, *text):
    stdout_color(color, *text, "\n")


def set_con_color(color: Color):
    if _win32_legacy_con:
        win32_set_fore_color(int(color.value))
    else:
        sys.stdout.write(color.value)


# TODO: make enum flags or something later on if you add another platform here
class OS(Enum):
    Any = auto(),
    Windows = auto(),
    Linux = auto(),


# maybe obsolete
PUBLIC_DIR = "../public"
BUILD_DIR = "build"

if sys.platform in {"linux", "linux2"}:
    SYS_OS = OS.Linux
elif sys.platform == "win32":
    SYS_OS = OS.Windows
else:
    SYS_OS = None
    print("Error: unsupported/untested platform: " + sys.platform)
    quit(1)


if SYS_OS == OS.Windows:
    DLL_EXT = ".dll"
    EXE_EXT = ".exe"
    LIB_EXT = ".lib"
elif SYS_OS == OS.Linux:
    DLL_EXT = ".so"
    EXE_EXT = ""
    LIB_EXT = ".a"


VS_PATH = ""
VS_DEVENV = ""
VS_MSBUILD = ""
VS_MSVC = "v143"


ERROR_LIST = []
CUR_PROJECT = "PROJECT"
ROOT_DIR = os.path.dirname(os.path.realpath(__file__))
DIR_STACK = []


def set_project(name: str):
    global CUR_PROJECT
    CUR_PROJECT = name
    print(f"\n"
          f"---------------------------------------------------------\n"
          f" Current Project: {name}\n"
          f"---------------------------------------------------------\n")


def push_dir(path: str):
    abs_dir = os.path.abspath(path)
    DIR_STACK.append(abs_dir)
    os.chdir(abs_dir)


def pop_dir():
    if len(DIR_STACK) == 0:
        print("ERROR: DIRECTORY STACK IS EMPTY!")
        return

    abs_dir = DIR_STACK.pop()
    os.chdir(abs_dir)


def reset_dir():
    os.chdir(ROOT_DIR)


# System Command Failed!
def syscmd_err(ret: int, string: str):
    global CUR_PROJECT
    error = f"{CUR_PROJECT}: {string}: return code {ret}\n"
    ERROR_LIST.append(error)
    sys.stderr.write(error)


def syscmd(cmd: str, string: str) -> bool:
    ret = os.system(cmd)
    if ret == 0:
        return True

    # System Command Failed!
    syscmd_err(ret, string)
    return False


def syscall(cmd: list, string: str) -> bool:
    ret = subprocess.call(cmd)
    if ret == 0:
        return True

    # System Command Failed!
    syscmd_err(ret, string)
    return False


def setup_vs_env():
    #cmd = "vswhere.exe -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe"
    cmd = "vswhere.exe -latest"

    global VS_PATH
    global VS_DEVENV
    global VS_MSBUILD

    vswhere = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    for line in vswhere.stdout:
        if line.startswith("productPath"):
            VS_DEVENV = line[len("productPath: "):-1]
            print(f"Set VS_DEVENV to \"{VS_DEVENV}\"")

        elif line.startswith("installationPath"):
            VS_PATH = line[len("installationPath: "):-1]
            print(f"Set VS_PATH to \"{VS_PATH}\"")

    if VS_PATH == "" or VS_DEVENV == "":
        print("FATAL ERROR: Failed to find VS_DEVENV or VS_PATH?")
        quit(3)

    # VS_MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\Msbuild\Current\Bin\amd64\msbuild.exe
    VS_MSBUILD = VS_PATH + "\\Msbuild\\Current\\Bin\\amd64\\msbuild.exe"


def parse_args() -> argparse.Namespace:
    args = argparse.ArgumentParser()
    args.add_argument("-nb", "--no-build", action="store_true", help="Don't build the libraries")
    # args.add_argument("-d", "--no-download", action="store_true", help="Don't download the libraries")
    args.add_argument("-t", "--target", nargs="+", help="Only download and build specific libraries")
    args.add_argument("-f", "--force", help="Force Run Everything")
    args.add_argument("-c", "--clean", help="Clean Everything in the thirdparty folder")
    return args.parse_args()
    

# =================================================================================================


def post_freetype_extract():
    set_project("Freetype")
    os.chdir("freetype")

    if SYS_OS == OS.Windows:
        for cfg in {"Debug Static", "Release Static"}:
            # cmd = f"\"{VS_MSBUILD}\" \"{fix_proj}\" -property:Configuration={cfg} -property:Platform=x64"
            cmd = [VS_MSBUILD, "builds\\windows\\vc2010\\freetype.vcxproj", f"-property:Configuration={cfg}", "-property:Platform=x64"]
            subprocess.call(cmd)

    else:
        print(" ------------------------ TODO: BUILD FREETYPE ON LINUX !!! ------------------------ ")

    pass


# =================================================================================================


def compile_nativefiledialog():
    set_project("Native File Dialog")
    os.chdir("nativefiledialog")

    if not syscmd(f"cmake -B build .", "Failed to run cmake"):
        return

    print("Building nativefiledialog - RelWithDebInfo\n")
    if not syscmd(f"cmake --build ./build --config RelWithDebInfo", "Failed to build in RelWithDebInfo"):
        return

    print("Building nativefiledialog - Release\n")
    if not syscmd(f"cmake --build ./build --config Release", "Failed to build in Release"):
        return

    print("Building nativefiledialog - Debug\n")
    if not syscmd(f"cmake --build ./build --config Debug", "Failed to build in Debug"):
        return


# =================================================================================================


'''
Basic Structure for a file here:
{
    # URL for downloading the file
    "url":  "ENTRY",
    
    # Filename that gets downloaded, most stuff from github is different from the url filename
    "file": "FILENAME.zip/7z",
    
    # Doubles as the folder name and the "package" name in this script, so you can skip it or only use it, etc.
    "name": "ENTRY",
    
    # OPTIONAL: A compile function to run after extracting it
    "func": function_pointer,
    
    # OPTIONAL: Name of the folder the file is extracted to, and will be renamed to what "name" is, usually not needed
    "extracted_folder": "",
    
    # OPTIONAL: If we can't extract this, tell the user to extract it manually, thanks p7zr and mpv...
    "user_extract": False,
},
'''


FILE_LIST = {
    # All Platforms
    OS.Any: [
        {
            "url":  "https://nongnu.askapache.com/freetype/freetype-2.14.1.tar.xz",
            "file": "freetype-2.14.1.tar.xz",
            "name": "freetype",
            "func": post_freetype_extract,
        },
        {
            "url":  "https://github.com/btzy/nativefiledialog-extended/archive/refs/tags/v1.2.1.zip",
            "file": "nativefiledialog-extended-1.2.1.zip",
            "name": "nativefiledialog",
            "func": compile_nativefiledialog,
        },
        {
            "url":  "https://github.com/shinchiro/mpv-winbuild-cmake/releases/download/20251017/mpv-dev-x86_64-v3-20251017-git-233e896.7z",
            "file": "mpv-dev-x86_64-v3-20251017-git-233e896.7z",
            "name": "mpv",
            "user_extract": True,
        },
    ],

    # Windows Only
    OS.Windows: [

        # MUST BE FIRST FOR VSWHERE !!!!!
        {
            "url":  "https://github.com/microsoft/vswhere/releases/download/2.8.4/vswhere.exe",
            "name": "vswhere",
            "file": "vswhere.exe",
            "func": setup_vs_env,
        },
        {
            "url":  "https://github.com/libsdl-org/SDL/releases/download/release-3.2.24/SDL3-devel-3.2.24-VC.zip",
            "name": "SDL3",
            "file": "SDL3-3.2.24.zip",
        },
    ],

    # Linux Only
    OS.Linux: [
    ],
}


# =================================================================================================


def download_file(url: str) -> bytes:
    req = Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    try:
        # time_print("sending request")
        # time_print("Attempting Download: " + url)
        response = urlopen(req, timeout=120)
        file_data: bytes = response.read()
        # time_print("received request")
    except Exception as F:
        error("Error opening url: ", url, "\n", str(F), "\n")
        return b""

    return file_data


def write_file(file: str, file_data: bytes) -> bool:
    try:
        file_io: BinaryIO
        with open(file, mode="wb") as file_io:
            file_io.write(file_data)
    except Exception as E:
        print(f"Failed to write \"{file}\", {E}")
        return False

    return True


def extract_file_user(file: str, folder: str) -> bool:
    # play bell/error sound
    print("\007")

    print(f"Please extract the file \"{file}\" yourself, current libraries don't support extracting it")
    input("Press Enter when finished")

    if not os.path.exists(folder):
        print("Extracted folder does not exist? Skipping")
        return False

    return True


def extract_file(tmp_file: str, file_ext: str, tmp_folder: str, folder: str, user_extract: bool) -> bool:
    if not os.path.isdir(folder):
        os.makedirs(folder)

    return_value = False
    if user_extract:
        return_value = extract_file_user(tmp_file, tmp_folder)
    else:
        if file_ext == "7z":
            py7zr.unpack_7zarchive(tmp_file, folder)
            return_value = True
        elif file_ext == "xz":
            with tarfile.open(tmp_file, "r:xz") as tar:
                tar.extractall(path=folder)
            return_value = True
        elif file_ext == "zip":
            zf = ZipFile(tmp_file)
            zf.extractall(folder)
            zf.close()
            return_value = True
        elif file_ext == "exe":
            pass
        else:
            return_value = extract_file_user(tmp_file, tmp_folder)

    os.remove(tmp_file)
    return return_value


def handle_item(item: dict):
    url = item["url"] if "url" in item else ""
    file = item["file"] if "file" in item else ""
    name = item["name"] if "name" in item else ""
    func = item["func"] if "func" in item else None
    extracted_folder = item["extracted_folder"] if "extracted_folder" in item else None
    user_extract = item["user_extract"] if "user_extract" in item else False

    # Check if we want this item
    if ARGS.target is not None:
        if name not in ARGS.target:
            return

    if file:
        # folder, file_ext = os.path.splitext(file)
        folder, file_ext = file.rsplit(".", 1)
        is_zip = file_ext in ("zip", "7z", "xz")

        # HACK
        if folder.endswith(".tar"):
            folder = folder.rsplit(".", 1)[0]

        if extracted_folder:
            folder = extracted_folder

        if not os.path.isdir(name) or ARGS.force or not is_zip:
            print_color(Color.CYAN, f"Downloading \"{name}\"")

            file_data: bytes = download_file(url)
            if file_data == b"":
                return

            if user_extract or not is_zip:
                tmp_file = file
            else:
                tmp_file = "tmp." + file

            if not write_file(tmp_file, file_data):
                return False

            if is_zip:
                if not extract_file(tmp_file, file_ext, folder,".", user_extract):
                    return

                # rename it
                if folder != name:
                    os.rename(folder, name)

        else:
            print_color(Color.CYAN, f"Already Downloaded: {name}")

    if func:
        print_color(Color.CYAN, f"Running Post Build Func: \"{name}\"")
        func()


def main():
    if ARGS.clean:
        print("NOT IMPLEMENTED YET!!!")
        return

    # Do your platform first (need to be first on windows for vswhere.exe)
    for item in FILE_LIST[SYS_OS]:
        handle_item(item)

    print("\n---------------------------------------------------------\n")

    # Do all platforms last
    for item in FILE_LIST[OS.Any]:
        handle_item(item)
        reset_dir()

    # check for any errors and print them
    errors_str = "Error" if len(ERROR_LIST) == 1 else "Errors"
    if len(ERROR_LIST) > 0:
        print("\n---------------------------------------------------------")
        sys.stderr.write(f"\n{len(ERROR_LIST)} {errors_str}:\n")
        for error in ERROR_LIST:
            sys.stderr.write(error)

    print(f"\n"
          f"---------------------------------------------------------\n"
          f" Finished - {len(ERROR_LIST)} {errors_str}\n"
          f"---------------------------------------------------------\n")

    if len(ERROR_LIST) > 1:
        exit(-1)


if __name__ == "__main__":
    ARGS = parse_args()
    main()

