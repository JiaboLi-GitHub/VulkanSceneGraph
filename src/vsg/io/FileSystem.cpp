/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/FileSystem.h>
#include <vsg/io/Logger.h>
#include <vsg/io/Options.h>
#include <vsg/io/stream.h>

#include <cstdio>

#if defined(_WIN32) && !defined(__CYGWIN__)
#    include <cstdlib>
#    include <direct.h>
#    include <io.h>
#    include <windows.h>

#    ifdef _MSC_VER
#        ifndef PATH_MAX
#            define PATH_MAX MAX_PATH
#        endif
#    endif

#    ifdef __MINGW32__
#        include <sys/stat.h>
#    endif

#else
#    include <errno.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

#ifdef __APPLE__
#    include <TargetConditionals.h>
#    include <libgen.h>
#    include <mach-o/dyld.h>
#endif

#include <limits.h>

using namespace vsg;

#if defined(_MSC_VER) || defined(__MINGW32__)
const char envPathDelimiter = ';';
#else
const char envPathDelimiter = ':';
#endif

// 获取环境变量值
// env_var: 环境变量名称
// 返回: 环境变量的值，如果不存在则返回空字符串
// 跨平台实现：Windows使用getenv_s，其他平台使用getenv
std::string vsg::getEnv(const char* env_var)
{
#if defined(_MSC_VER) || defined(__MINGW32__)
    char env_value[4096];
    std::size_t len;
    if (auto error = getenv_s(&len, env_value, sizeof(env_value) - 1, env_var); error != 0 || len == 0)
    {
        return {};
    }
#else
    const char* env_value = getenv(env_var);
    if (env_value == nullptr) return {};
#endif
    return std::string(env_value);
}

// 获取环境变量路径列表
// env_var: 环境变量名称（如PATH）
// 返回: 路径列表
// 解析环境变量中的路径分隔符（Windows使用';'，其他平台使用':'），返回路径列表
Paths vsg::getEnvPaths(const char* env_var)
{
    if (!env_var) return {};

#if defined(_MSC_VER) || defined(__MINGW32__)
    char env_value[4096];
    std::size_t len;
    if (auto error = getenv_s(&len, env_value, sizeof(env_value) - 1, env_var); error != 0 || len == 0)
    {
        return {};
    }
#else
    const char* env_value = getenv(env_var);
    if (env_value == nullptr) return {};
#endif

    Paths filepaths;
    std::string paths(env_value);

    // 按分隔符分割路径
    std::string::size_type start = 0;
    std::string::size_type end;
    while ((end = paths.find_first_of(envPathDelimiter, start)) != std::string::npos)
    {
        filepaths.push_back(paths.substr(start, end - start));
        start = end + 1;
    }

    // 添加最后一个路径
    std::string lastPath(paths, start, std::string::npos);
    if (!lastPath.empty())
        filepaths.push_back(lastPath);

    return filepaths;
}

#if !defined(S_ISDIR)
#    if defined(_S_IFDIR) && !defined(__S_IFDIR)
#        define __S_IFDIR _S_IFDIR
#    endif
#    define S_ISDIR(mode) (mode & __S_IFDIR)
#endif

// 获取文件类型
// path: 文件路径
// 返回: 文件类型（DIRECTORY、REGULAR_FILE或FILE_NOT_FOUND）
// 跨平台实现：使用stat系列函数检查文件类型
FileType vsg::fileType(const Path& path)
{
#if defined(_MSC_VER) || defined(__MINGW32__)
    struct __stat64 stbuf;
    if (_wstat64(path.c_str(), &stbuf) != 0) return FILE_NOT_FOUND;
#elif defined(__APPLE__)
    struct stat stbuf;
    if (stat(path.c_str(), &stbuf) != 0) return FILE_NOT_FOUND;
#else
    struct stat64 stbuf;
    if (stat64(path.c_str(), &stbuf) != 0) return FILE_NOT_FOUND;
#endif

    if ((stbuf.st_mode & S_IFDIR) != 0)
        return DIRECTORY;
    else if ((stbuf.st_mode & S_IFREG) != 0)
        return REGULAR_FILE;
    else
        return FILE_NOT_FOUND;
}

// 检查文件是否存在
// path: 文件路径
// 返回: 如果文件存在则返回true
// 跨平台实现：Windows使用_waccess，其他平台使用access
bool vsg::fileExists(const Path& path)
{
#if defined(_MSC_VER) || defined(__MINGW32__)
    return _waccess(path.c_str(), 0) == 0;
#else
    return access(path.c_str(), F_OK) == 0;
#endif
}

// 在路径列表中查找文件
// filename: 文件名
// paths: 搜索路径列表
// 返回: 找到的完整路径，如果未找到则返回空路径
// 在指定的路径列表中搜索文件，返回第一个找到的完整路径
Path vsg::findFile(const Path& filename, const Paths& paths)
{
    for (auto path : paths)
    {
        Path fullpath = path / filename;
        if (fileExists(fullpath))
        {
            return fullpath;
        }
    }
    return {};
}

// 使用选项查找文件
// filename: 文件名
// options: 选项对象（包含搜索路径和回调函数）
// 返回: 找到的完整路径，如果未找到则返回空路径
// 首先检查是否有自定义查找回调，然后根据选项的路径列表和检查提示查找文件
Path vsg::findFile(const Path& filename, const Options* options)
{
    if (options)
    {
        // 如果选项有findFileCallback，使用它
        if (options->findFileCallback) return options->findFileCallback(filename, options);

        if (!options->paths.empty())
        {
            // 如果合适，首先直接使用文件名（如果存在）
            if (options->checkFilenameHint == Options::CHECK_ORIGINAL_FILENAME_EXISTS_FIRST && fileExists(filename)) return filename;

            // 在选项特定的路径中搜索文件
            if (auto path = findFile(filename, options->paths)) return path;

            // 如果合适，最后直接使用文件名（如果存在）
            if (options->checkFilenameHint == Options::CHECK_ORIGINAL_FILENAME_EXISTS_LAST && fileExists(filename))
                return filename;
            else
                return {};
        }
    }

    return fileExists(filename) ? filename : Path();
}

// 创建目录（包括所有父目录）
// path: 要创建的目录路径
// 返回: 如果成功创建所有目录则返回true
// 递归创建目录结构，从最深层开始创建（确保父目录存在）
bool vsg::makeDirectory(const Path& path)
{
    // 收集需要创建的目录列表（从最深到最浅）
    std::vector<vsg::Path> directoriesToCreate;
    Path trimmed_path = path;
    while (trimmed_path && !vsg::fileExists(trimmed_path))
    {
        directoriesToCreate.push_back(trimmed_path);
        trimmed_path = vsg::filePath(trimmed_path);
    }

    // 从最浅到最深创建目录
    for (auto itr = directoriesToCreate.rbegin(); itr != directoriesToCreate.rend(); ++itr)
    {
        vsg::Path directory_to_create = *itr;

        // 忽略Windows驱动器前缀（如"C:"）
        if (directory_to_create.size() == 2 && directory_to_create[1] == ':')
        {
            // 忽略C:样式的驱动器前缀
            continue;
        }

#if defined(_MSC_VER) || defined(__MINGW32__)
        if (int status = _wmkdir(directory_to_create.c_str()); status != 0)
#else // POSIX
        if (int status = mkdir(directory_to_create.c_str(), 0755); status != 0)
#endif
        {
            if (errno != EEXIST)
            {
                // 静默忽略已存在文件的mkdir操作，这在填充文件缓存时可能安全发生
                debug("mkdir(", directory_to_create, ") failed. errno = ", errno);
            }
            return false;
        }
    }

    return true;
}

Path vsg::executableFilePath()
{
    Path path;

#if defined(_WIN32)
    TCHAR buf[PATH_MAX + 1];
    DWORD result = GetModuleFileName(NULL, buf, static_cast<DWORD>(std::size(buf) - 1));
    if (result && result < std::size(buf))
        path = buf;
#elif defined(__linux__)

    std::vector<char> buffer(1024);
    ssize_t len = 0;
    while ((len = ::readlink("/proc/self/exe", buffer.data(), buffer.size())) == static_cast<ssize_t>(buffer.size()))
    {
        buffer.resize(buffer.size() * 2);
    }

    // add terminator to string.
    buffer[len] = '\0';

    return buffer.data();

#elif defined(__APPLE__)
#    if TARGET_OS_MAC
    char realPathName[PATH_MAX + 1];
    char buf[PATH_MAX + 1];
    uint32_t size = (uint32_t)sizeof(buf);

    if (!_NSGetExecutablePath(buf, &size))
    {
        realpath(buf, realPathName);
        path = realPathName;
    }
#    elif TARGET_IPHONE_SIMULATOR
    // iOS, tvOS, or watchOS Simulator
    // Not currently implemented
#    elif TARGET_OS_MACCATALYST
    // Mac's Catalyst (ports iOS API into Mac, like UIKit).
    // Not currently implemented
#    elif TARGET_OS_IPHONE
    // iOS, tvOS, or watchOS device
    // Not currently implemented
#    else
#        error "Unknown Apple platform"
#    endif
#elif defined(__ANDROID__)
    // Not currently implemented
#endif
    return path;
}

// 打开文件（跨平台实现）
// path: 文件路径
// mode: 打开模式（如"r"、"w"等）
// 返回: 文件指针，如果失败则返回nullptr
// Windows使用_wfopen_s，其他平台使用标准fopen
FILE* vsg::fopen(const Path& path, const char* mode)
{
#if defined(_MSC_VER) || defined(__MINGW32__)
    std::wstring wMode;
    convert_utf(mode, wMode);

    FILE* file = nullptr;
    auto errorNo = _wfopen_s(&file, path.c_str(), wMode.c_str());
    if (errorNo == 0)
        return file;
    else
        return nullptr;
#else
    return ::fopen(path.c_str(), mode);
#endif
}

#if defined(_MSC_VER) || defined(__MINGW32__)
// Microsoft API for reading directories
Paths vsg::getDirectoryContents(const Path& directoryName)
{
    WIN32_FIND_DATAW ffd;

    auto searchFolder = directoryName / "*";
    auto handle = FindFirstFileW(searchFolder.c_str(), &ffd);
    if (handle == INVALID_HANDLE_VALUE) return {};

    Paths paths;
    do
    {
        paths.push_back(ffd.cFileName);
    } while (FindNextFileW(handle, &ffd) != 0);

    FindClose(handle);

    return paths;
}
#else
// posix API for reading directories
#    include <dirent.h>
Paths vsg::getDirectoryContents(const Path& directoryName)
{
    auto handle = opendir(directoryName.c_str());
    if (handle == 0) return {};

    Paths paths;
    dirent* rc = nullptr;
    while ((rc = readdir(handle)) != nullptr)
    {
        paths.push_back(rc->d_name);
    }

    closedir(handle);

    return paths;
}
#endif
