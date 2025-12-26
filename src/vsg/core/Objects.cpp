/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Objects.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>

using namespace vsg;

// Objects类的构造函数
// 创建包含指定数量子对象的Objects对象
// numChildren: 子对象的数量
Objects::Objects(size_t numChildren) :
    children(numChildren)
{
}

// Objects类的析构函数
Objects::~Objects()
{
}

// 从输入流读取Objects对象
// 读取子对象列表
void Objects::read(Input& input)
{
    Object::read(input);

    // 读取子对象列表
    input.readObjects("children", children);
}

// 将Objects对象写入输出流
// 写入子对象列表
void Objects::write(Output& output) const
{
    Object::write(output);

    // 写入子对象列表
    output.writeObjects("children", children);
}
