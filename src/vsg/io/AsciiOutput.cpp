/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Version.h>

#include <vsg/io/AsciiOutput.h>

#include <cstring>

using namespace vsg;

// 构造函数：使用输出流和选项创建ASCII输出对象
// output: 输出流对象
// in_options: 选项对象
// ASCII输出用于将VSG对象序列化为ASCII格式
AsciiOutput::AsciiOutput(std::ostream& output, ref_ptr<const Options> in_options) :
    Output(in_options),
    _output(output)
{
    _maximumIndentation = std::strlen(_indentationString);
}

// 写入属性名称
// propertyName: 属性名称
// 写入带缩进的属性名称
void AsciiOutput::writePropertyName(const char* propertyName)
{
    indent() << propertyName;
}

// 写入字符串数组
// num: 要写入的字符串数量
// value: 字符串数组指针
// 写入指定数量的字符串（每个字符串前加空格）
void AsciiOutput::write(size_t num, const std::string* value)
{
    if (num == 1)
    {
        _output << ' ';
        _write(*value);
    }
    else
    {
        for (; num > 0; --num, ++value)
        {
            _output << ' ';
            _write(*value);
        }
    }
}

// 写入宽字符串（内部方法）
// str: 要写入的宽字符串
// 将宽字符串转换为UTF-8字符串后写入
void AsciiOutput::_write(const std::wstring& str)
{
    std::string string_value;
    convert_utf(str, string_value);
    _write(string_value);
}

// 写入宽字符串数组
// num: 要写入的宽字符串数量
// value: 宽字符串数组指针
// 写入指定数量的宽字符串（每个字符串前加空格）
void AsciiOutput::write(size_t num, const std::wstring* value)
{
    if (num == 1)
    {
        _output << ' ';
        _write(*value);
    }
    else
    {
        for (; num > 0; --num, ++value)
        {
            _output << ' ';
            _write(*value);
        }
    }
}

// 写入路径数组
// num: 要写入的路径数量
// value: 路径数组指针
// 写入指定数量的路径（每个路径前加空格）
void AsciiOutput::write(size_t num, const Path* value)
{
    if (num == 1)
    {
        _output << ' ';
        _write(value->string());
    }
    else
    {
        for (; num > 0; --num, ++value)
        {
            _output << ' ';
            _write(value->string());
        }
    }
}

// 写入对象
// object: 要写入的对象
// 将对象序列化为ASCII格式，支持对象ID引用和递归写入
void AsciiOutput::write(const vsg::Object* object)
{
    // 如果对象已写入，只写入对象ID引用
    if (auto itr = objectIDMap.find(object); itr != objectIDMap.end())
    {
        // 写入对象ID
        _output << " id=" << itr->second << "\n";
        return;
    }

    // 分配新的对象ID并写入对象
    ObjectID id = objectID++;
    objectIDMap[object] = id;

    if (object)
    {
#if 0
        _output<<"id="<<id<<" "<<vsg::type_name(*object)<<"\n";
#else
        _output << " id=" << id << " " << object->className() << "\n";
#endif
        // 写入开始大括号并增加缩进
        indent() << "{\n";
        _indentation += _indentationStep;
        // 递归写入对象内容
        object->write(*this);
        _indentation -= _indentationStep;
        // 写入结束大括号
        indent() << "}\n";
    }
    else
    {
        _output << " id=" << id << " nullptr\n";
    }
}
