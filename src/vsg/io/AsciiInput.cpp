/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/AsciiInput.h>
#include <vsg/io/Logger.h>
#include <vsg/io/ReaderWriter.h>

#include <cstring>

using namespace vsg;

// 构造函数：使用输入流、对象工厂和选项创建ASCII输入对象
// input: 输入流对象
// in_objectFactory: 对象工厂对象（用于创建反序列化的对象）
// in_options: 选项对象
// ASCII输入用于从ASCII格式的输入流读取VSG对象的序列化数据
AsciiInput::AsciiInput(std::istream& input, ref_ptr<ObjectFactory> in_objectFactory, ref_ptr<const Options> in_options) :
    Input(in_objectFactory, in_options),
    _input(input)
{
}

// 匹配属性名称
// propertyName: 期望的属性名称
// 返回: 如果匹配成功则返回true
// 从输入流读取属性名称并与期望的名称进行比较
bool AsciiInput::matchPropertyName(const char* propertyName)
{
    _input >> _readPropertyName;
    if (_readPropertyName != propertyName)
    {
        error("Unable to match ", propertyName, " got ", _readPropertyName, " instead.");
        return false;
    }
    return true;
}

// 读取对象ID
// 返回: 可选对象ID（如果找到ID则返回true和ID值，否则返回false）
// 从输入流读取"id=数字"格式的对象ID
AsciiInput::OptionalObjectID AsciiInput::objectID()
{
    std::string token;
    _input >> token;
    if (token.compare(0, 3, "id=") == 0)
    {
        token.erase(0, 3);
        std::stringstream str(token);
        ObjectID id;
        str >> id;
        return OptionalObjectID{true, id};
    }
    else
    {
        return OptionalObjectID(false, 0);
    }
}

// 读取字符串（内部方法）
// value: 输出参数，用于存储读取的字符串
// 支持引号字符串（处理转义字符）和普通字符串
void AsciiInput::_read(std::string& value)
{
    value.clear();

    char c;
    _input >> c;
    if (_input.good())
    {
        // 如果以引号开始，读取引号字符串（处理转义）
        if (c == '"')
        {
            _input.get(c);
            while (_input.good())
            {
                if (c == '\\')
                {
                    _input.get(c);
                    if (c == '"')
                        value.push_back(c);
                    else
                    {
                        value.push_back('\\');
                        value.push_back(c);
                    }
                }
                else if (c != '"')
                {
                    value.push_back(c);
                }
                else
                {
                    break;
                }
                _input.get(c);
            }
        }
        else
        {
            // 普通字符串，使用流操作符读取
            _input >> value;
        }
    }
}

// 读取字符串数组
// num: 要读取的字符串数量
// value: 字符串数组指针
// 读取指定数量的字符串
void AsciiInput::read(size_t num, std::string* value)
{
    if (num == 1)
    {
        _read(*value);
    }
    else
    {
        for (; num > 0; --num, ++value)
        {
            _read(*value);
        }
    }
}

// 读取宽字符串（内部方法）
// value: 输出参数，用于存储读取的宽字符串
// 先读取UTF-8字符串，然后转换为宽字符串
void AsciiInput::_read(std::wstring& value)
{
    std::string string_value;
    _read(string_value);
    convert_utf(string_value, value);
}

// 读取宽字符串数组
// num: 要读取的宽字符串数量
// value: 宽字符串数组指针
// 读取指定数量的宽字符串
void AsciiInput::read(size_t num, std::wstring* value)
{
    if (num == 1)
    {
        _read(*value);
    }
    else
    {
        for (; num > 0; --num, ++value)
        {
            _read(*value);
        }
    }
}

// 读取路径数组
// num: 要读取的路径数量
// value: 路径数组指针
// 读取指定数量的路径（先读取字符串，然后转换为路径）
void AsciiInput::read(size_t num, Path* value)
{
    if (num == 1)
    {
        std::string str_value;
        _read(str_value);
        *value = str_value;
    }
    else
    {
        for (; num > 0; --num, ++value)
        {
            std::string str_value;
            _read(str_value);
            *value = str_value;
        }
    }
}

// 读取对象
// 返回: 读取的对象，如果失败则返回空指针
// 从ASCII输入流读取对象，支持对象ID引用和递归读取
vsg::ref_ptr<vsg::Object> AsciiInput::read()
{
    auto result = objectID();
    if (result.first)
    {
        ObjectID id = result.second;
        //debug("   matched result=", id);

        // 如果对象ID已存在，返回已读取的对象（处理引用）
        if (auto itr = objectIDMap.find(id); itr != objectIDMap.end())
        {
            //debug("Returning existing object ", itr->second);
            return itr->second;
        }
        else
        {
            // 读取类名并创建新对象
            std::string className;
            _input >> className;

            //debug("Loading new object ", className);

            if (className != "nullptr")
            {
                auto object = objectFactory->create(className.c_str());
                objectIDMap[id] = object;
                if (object)
                {
                    // 匹配开始大括号
                    matchPropertyName("{");

                    // 递归读取对象内容
                    object->read(*this);

                    //debug("Loaded object, assigning to objectIDMap.", object);

                    // 匹配结束大括号
                    matchPropertyName("}");
                }
                else
                {
                    warn("Unable to create instance of class : ", className);
                }
                return object;
            }
            else
            {
                return objectIDMap[id] = {};
            }
        }
    }
    return {};
}
