/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/mem_stream.h>

using namespace vsg;

// 构造函数：使用内存指针和长度创建内存流对象
// ptr: 内存缓冲区指针
// length: 缓冲区长度
// 内存流用于从内存缓冲区读取数据，就像从文件读取一样
mem_stream::mem_stream(const uint8_t* ptr, size_t length) :
    std::istream(&_buffer),
    _buffer(ptr, length)
{
    rdbuf(&_buffer);
}

// 构造函数：使用字符串视图创建内存流对象
// sv: 字符串视图对象
// 从字符串视图创建内存流
mem_stream::mem_stream(const std::string_view& sv) :
    mem_stream(reinterpret_cast<const uint8_t*>(sv.data()), sv.size())
{
}

// 构造函数：使用字符串的子串创建内存流对象
// str: 字符串对象
// pos: 起始位置
// length: 子串长度
// 从字符串的指定位置和长度创建内存流
mem_stream::mem_stream(const std::string& str, std::string::size_type pos, std::string::size_type length) :
    mem_stream(reinterpret_cast<const uint8_t*>(&(str[pos])), length)
{
}

// 设置内存缓冲区
// ptr: 内存缓冲区指针
// length: 缓冲区长度
// 重新设置内存流的缓冲区并清除流状态
void mem_stream::set(const uint8_t* ptr, size_t length)
{
    _buffer.set(ptr, length);
    clear();
}

// 内存缓冲区构造函数
// ptr: 内存缓冲区指针
// length: 缓冲区长度
// 设置流缓冲区的开始、当前位置和结束位置
mem_stream::mem_buffer::mem_buffer(const uint8_t* ptr, size_t length)
{
    setg((char*)(ptr), (char*)(ptr), (char*)(ptr) + length);
}

// 查找位置（相对偏移）
// offset: 偏移量
// dir: 查找方向（开始、当前位置、结束）
// 返回: 新的位置
// 根据查找方向设置流缓冲区的位置
std::streambuf::pos_type mem_stream::mem_buffer::seekoff(std::streambuf::off_type offset, std::ios_base::seekdir dir, std::ios_base::openmode /*mode*/)
{
    if (dir == std::ios_base::beg)
    {
        setg(eback(), eback() + offset, egptr());
    }
    else if (dir == std::ios_base::end)
    {
        setg(eback(), egptr() - offset, egptr());
    }
    else // dir == std::ios_base::cur
    {
        setg(eback(), gptr() + offset, egptr());
    }

    return pos_type(gptr() - eback());
}

// 查找位置（绝对位置）
// pos: 绝对位置
// 返回: 新的位置
// 设置流缓冲区到指定的绝对位置
std::streambuf::pos_type mem_stream::mem_buffer::seekpos(std::streambuf::pos_type pos, std::ios_base::openmode /*mode*/)
{
    setg(eback(), eback() + pos, egptr());
    return pos_type(pos);
}
