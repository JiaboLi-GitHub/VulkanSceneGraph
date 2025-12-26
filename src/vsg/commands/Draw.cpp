/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/Draw.h>
#include <vsg/core/compare.h>

using namespace vsg;

// 比较两个绘制命令对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较绘制参数区域（顶点数量、首实例）
int Draw::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_region(vertexCount, firstInstance, rhs.vertexCount);
}

// 从输入流读取绘制命令对象
// input: 输入流对象
// 读取绘制参数：顶点数量、实例数量、首顶点、首实例
void Draw::read(Input& input)
{
    Command::read(input);

    input.read("vertexCount", vertexCount);
    input.read("instanceCount", instanceCount);
    input.read("firstVertex", firstVertex);
    input.read("firstInstance", firstInstance);
}

// 将绘制命令对象写入输出流
// output: 输出流对象
// 写入绘制参数：顶点数量、实例数量、首顶点、首实例
void Draw::write(Output& output) const
{
    Command::write(output);

    output.write("vertexCount", vertexCount);
    output.write("instanceCount", instanceCount);
    output.write("firstVertex", firstVertex);
    output.write("firstInstance", firstInstance);
}
