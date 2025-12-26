/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/MipmapLayout.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>

using namespace vsg;

// MipmapLayout类的默认构造函数
// 创建空的Mipmap布局对象
MipmapLayout::MipmapLayout()
{
}

// MipmapLayout类的构造函数
// 创建包含指定数量Mipmap级别的布局对象
// size: Mipmap级别的数量
MipmapLayout::MipmapLayout(std::size_t size) :
    mipmaps(size)
{
}

// MipmapLayout类的析构函数
MipmapLayout::~MipmapLayout()
{
}

// 从输入流读取MipmapLayout对象
// 读取Mipmap级别信息
void MipmapLayout::read(Input& input)
{
    Object::read(input);

    // 读取Mipmap级别数组
    input.readValues("mipmaps", mipmaps);
}

// 将MipmapLayout对象写入输出流
// 写入Mipmap级别信息
void MipmapLayout::write(Output& output) const
{
    Object::write(output);

    // 写入Mipmap级别数组
    output.writeValues("mipmaps", mipmaps);
}
