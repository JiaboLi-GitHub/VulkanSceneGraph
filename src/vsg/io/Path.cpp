/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/FileSystem.h>
#include <vsg/io/Path.h>

#include <cctype>
#include <list>

using namespace vsg;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Path - 路径类，提供跨平台的文件路径操作
//
// 构造函数：创建路径对象（默认）
// 路径类用于处理文件系统路径，支持UTF-8和宽字符编码
Path::Path()
{
}

// 拷贝构造函数：从另一个路径对象创建新的路径对象
// path: 要拷贝的路径对象
Path::Path(const Path& path) :
    _string(path._string)
{
}

// 构造函数：使用C字符串创建路径对象
// str: C风格字符串（UTF-8编码）
Path::Path(const char* str)
{
    assign(str);
}

// 构造函数：使用标准字符串创建路径对象
// str: 标准字符串（UTF-8编码）
Path::Path(const std::string& str)
{
    assign(str);
}

// 构造函数：使用宽字符C字符串创建路径对象
// str: 宽字符C风格字符串
Path::Path(const wchar_t* str)
{
    assign(str);
}

// 构造函数：使用宽字符标准字符串创建路径对象
// str: 宽字符标准字符串
Path::Path(const std::wstring& str)
{
    assign(str);
}

// 赋值操作符：从另一个路径对象赋值
// path: 源路径对象
// 返回: 当前路径对象的引用
Path& Path::assign(const Path& path)
{
    if (&path != this) _string = path._string;
    return *this;
}

// 赋值操作符：从标准字符串赋值
// str: 标准字符串（UTF-8编码）
// 返回: 当前路径对象的引用
// 将UTF-8字符串转换为内部字符串格式
Path& Path::assign(const std::string& str)
{
    convert_utf(str, _string);
    return *this;
}

// 赋值操作符：从C字符串赋值
// str: C风格字符串（UTF-8编码）
// 返回: 当前路径对象的引用
// 将UTF-8字符串转换为内部字符串格式
Path& Path::assign(const char* str)
{
    convert_utf(str, _string);
    return *this;
}

// 赋值操作符：从宽字符标准字符串赋值
// str: 宽字符标准字符串
// 返回: 当前路径对象的引用
// 将宽字符字符串转换为内部字符串格式
Path& Path::assign(const std::wstring& str)
{
    convert_utf(str, _string);
    return *this;
}

// 赋值操作符：从宽字符C字符串赋值
// str: 宽字符C风格字符串
// 返回: 当前路径对象的引用
// 将宽字符字符串转换为内部字符串格式
Path& Path::assign(const wchar_t* str)
{
    convert_utf(str, _string);
    return *this;
}

Path& Path::replace(size_type pos, size_type n, const Path& str)
{
    _string.replace(pos, n, str._string);
    return *this;
}
Path& Path::replace(size_type pos, size_type n, const std::string& str)
{
    _string.replace(pos, n, convert_utf<string_type>(str));
    return *this;
}
Path& Path::replace(size_type pos, size_type n, const std::wstring& str)
{
    _string.replace(pos, n, convert_utf<string_type>(str));
    return *this;
}
Path& Path::replace(size_type pos, size_type n, const char* str)
{
    _string.replace(pos, n, convert_utf<string_type>(str));
    return *this;
}
Path& Path::replace(size_type pos, size_type n, const wchar_t* str)
{
    _string.replace(pos, n, convert_utf<string_type>(str));
    return *this;
}

// 追加路径
// right: 要追加的路径
// 返回: 当前路径对象的引用
// 智能处理路径分隔符，确保路径正确连接
Path& Path::append(const Path& right)
{
    if (empty())
    {
        return assign(right);
    }

    auto lastChar = _string[_string.size() - 1];
    // 如果最后一个字符是首选分隔符，直接连接
    if (lastChar == preferred_separator)
    {
        concat(right._string);
    }
    // 如果最后一个字符是备用分隔符，替换为首选分隔符后连接
    else if (lastChar == alternate_separator)
    {
        _string.erase(_string.size() - 1, 1);
        _string.push_back(preferred_separator);
        _string.append(right._string);
    }
    else // 最后一个字符不是分隔符，添加分隔符后连接
    {
        _string.push_back(preferred_separator);
        _string.append(right._string);
    }

    return *this;
}

// 擦除路径的一部分
// pos: 起始位置
// len: 要擦除的长度
// 返回: 当前路径对象的引用
Path& Path::erase(size_t pos, size_t len)
{
    _string.erase(pos, len);
    return *this;
}

// 获取文件类型
// 返回: 文件类型（文件、目录、未知等）
FileType Path::type() const
{
    return vsg::fileType(*this);
}

// 词法规范化路径
// 返回: 规范化后的路径
// 移除路径中的"."和".."引用，规范化路径分隔符
Path Path::lexically_normal() const
{
    if (_string.empty()) return {};

#if defined(_MSC_VER) || defined(__MINGW32__)
    const value_type* dot_str = L".";
    const value_type* doubledot_str = L"..";
    const value_type colon = L':';
#else
    const value_type* dot_str = ".";
    const value_type* doubledot_str = "..";
    const value_type colon = ':';
#endif

    size_type start_pos = 0;
    if ((start_pos + 2) < _string.size())
    {
        if (_string[start_pos + 1] == colon)
        {
            start_pos += 2;
        }
    }

    auto c0 = _string[start_pos];
    if (c0 == posix_separator || c0 == windows_separator)
    {
        start_pos += 1;
    }

    using string_view = std::basic_string_view<value_type>;
    auto str = _string.data();

    string_view prefix(str, start_pos);

    std::list<string_view> path_segments;

    auto pos = start_pos;
    while (pos < _string.size())
    {
        auto prev_pos = pos;
        pos = _string.find_first_of(separators, prev_pos);
        if (pos != npos)
        {
            path_segments.emplace_back(str + prev_pos, pos - prev_pos);
            ++pos;
            if (pos == _string.size())
            {
                // last character was seperatator
                path_segments.emplace_back();
                break;
            }
        }
        else
        {
            // last segment
            path_segments.emplace_back(str + prev_pos, _string.size() - prev_pos);
            break;
        }
    }

    if (path_segments.size() < 2) return *this;

    auto itr = path_segments.begin();
    auto prev_itr = itr++;

    bool last_segment_was_double_dot = path_segments.back().compare(doubledot_str) == 0;
    while (itr != path_segments.end())
    {
        if (prev_itr->compare(dot_str) == 0)
        {
            // prune previous
            path_segments.erase(prev_itr);
            if (itr != path_segments.begin())
            {
                prev_itr = itr;
                --prev_itr;
            }
            else
            {
                prev_itr = itr;
                ++itr;
            }
        }
        else if (itr->compare(doubledot_str) == 0)
        {
            if (prev_itr->compare(doubledot_str) == 0)
            {
                // NOP
                prev_itr = itr;
                ++itr;
            }
            else
            {
                // prune previous
                path_segments.erase(prev_itr);
                path_segments.erase(itr);

                // reset to start
                itr = prev_itr = path_segments.begin();
                if (itr != path_segments.end()) ++itr;
            }
        }
        else
        {
            // NOP
            prev_itr = itr;
            ++itr;
        }
    }

    Path new_path;
    if (!prefix.empty()) new_path = string_type(prefix);
    for (auto& sv : path_segments)
    {
        new_path /= string_type(sv);
    }

    if (last_segment_was_double_dot && !path_segments.empty() && path_segments.back().compare(doubledot_str) != 0)
    {
        // if the last segment was a double dot and it's no longer a double dot then treat it as a directory so add separator
        new_path.concat(preferred_separator);
    }

    return new_path;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Path manipulation functions - 路径操作函数
//

// 获取文件路径（不包含文件名）
// path: 完整路径
// 返回: 文件路径部分（不包含文件名）
// 如果路径以相对路径结尾（"."或".."），返回原路径
Path vsg::filePath(const Path& path)
{
    if (trailingRelativePath(path)) return path;

    auto slash = path.find_last_of(Path::separators);
    if (slash != vsg::Path::npos)
    {
        return path.substr(0, slash);
    }
    else
    {
        return {};
    }
}

// 获取文件扩展名
// path: 完整路径
// 返回: 文件扩展名（包含点号，如".txt"）
// 如果路径中没有扩展名或扩展名在最后一个分隔符之前，返回空路径
Path vsg::fileExtension(const Path& path)
{
    auto dot = path.find_last_of('.');
    if (dot == Path::npos || (dot + 1) == path.size()) return {};

    auto slash = path.find_last_of(Path::separators);
    if (slash != Path::npos && dot < slash) return {};

    return path.substr(dot);
}

// 获取小写文件扩展名
// path: 完整路径
// pruneExtras: 是否修剪额外字符（如查询参数）
// 返回: 小写的文件扩展名
// 将文件扩展名转换为小写，可选择性地移除查询参数等额外字符
Path vsg::lowerCaseFileExtension(const Path& path, bool pruneExtras)
{
    Path ext = fileExtension(path);

    if (pruneExtras)
    {
        auto question_mark = ext.find_first_of('?');
        if (question_mark != ext.npos) ext.erase(question_mark, Path::npos);
    }

    for (auto& c : ext) c = std::tolower(c);
    return ext;
}

// 获取简单文件名（不包含扩展名）
// path: 完整路径
// 返回: 文件名部分（不包含扩展名）
// 如果路径以相对路径结尾，返回空路径
Path vsg::simpleFilename(const Path& path)
{
    if (trailingRelativePath(path)) return {};

    auto dot = path.find_last_of('.');
    auto slash = path.find_last_of(Path::separators);
    if (slash != Path::npos)
    {
        if ((dot == Path::npos) || (dot < slash))
            return path.substr(slash + 1);
        else
            return path.substr(slash + 1, dot - slash - 1);
    }
    else
    {
        if (dot == Path::npos)
            return path;
        else
            return path.substr(0, dot);
    }
}

// 检查路径是否以相对路径结尾
// path: 要检查的路径
// 返回: 如果路径以"."或".."结尾则返回true
// 检查路径是否以相对路径标记（"."或".."）结尾
bool vsg::trailingRelativePath(const Path& path)
{
    if (path == ".") return true;
    if (path == "..") return true;
    if (path.size() >= 2)
    {
        if (path.compare(path.size() - 2, 2, "/.") == 0) return true;
        if (path.compare(path.size() - 2, 2, "\\.") == 0) return true;

        if (path.size() >= 3)
        {
            if (path.compare(path.size() - 3, 3, "/..") == 0) return true;
            if (path.compare(path.size() - 3, 3, "\\..") == 0) return true;
        }
    }
    return false;
}

// 移除文件扩展名
// path: 完整路径
// 返回: 移除扩展名后的路径
// 如果路径以相对路径结尾，返回原路径
Path vsg::removeExtension(const Path& path)
{
    if (trailingRelativePath(path)) return path;

    auto dot = path.find_last_of('.');
    if (dot == Path::npos) return path;

    auto slash = path.find_last_of(Path::separators);
    if (slash != Path::npos && dot < slash)
        return path;
    else if (dot > 1)
        return path.substr(0, dot);
    else
        return {};
}
