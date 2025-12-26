/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/stream.h>
#include <vsg/nodes/Layer.h>

using namespace vsg;

// 构造函数：创建层节点
// 层节点用于将子节点分配到特定的渲染bin中，支持分层渲染和排序
Layer::Layer()
{
}

// 拷贝构造函数：从另一个层节点创建新的层节点
// rhs: 要拷贝的层节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝掩码、bin编号、值和子节点
Layer::Layer(const Layer& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    mask(rhs.mask),
    binNumber(rhs.binNumber),
    value(rhs.value),
    child(copyop(rhs.child))
{
}

// 构造函数：使用bin编号、值和子节点创建层节点
// in_binNumber: bin编号（用于指定渲染顺序）
// in_value: 排序值（用于在同一bin内排序）
// in_child: 子节点（将被分配到指定的bin中）
Layer::Layer(int32_t in_binNumber, double in_value, ref_ptr<Node> in_child) :
    binNumber(in_binNumber),
    value(in_value),
    child(in_child)
{
}

// 析构函数：销毁层节点
Layer::~Layer()
{
}

// 比较两个层节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、掩码、bin编号、值和子节点
int Layer::compare(const Object& rhs_object) const
{
    int result = Node::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(mask, rhs.mask)) != 0) return result;
    if ((result = compare_value(binNumber, rhs.binNumber)) != 0) return result;
    if ((result = compare_value(value, rhs.value)) != 0) return result;
    return compare_pointer(child, rhs.child);
}

// 从输入流读取层节点对象
// input: 输入流对象
// 读取掩码、bin编号、值和子节点
void Layer::read(Input& input)
{
    Node::read(input);

    input.read("mask", mask);
    input.read("binNumber", binNumber);
    input.read("value", value);
    input.readObject("child", child);
}

// 将层节点对象写入输出流
// output: 输出流对象
// 写入掩码、bin编号、值和子节点
void Layer::write(Output& output) const
{
    Node::write(output);

    output.write("mask", mask);
    output.write("binNumber", binNumber);
    output.write("value", value);
    output.writeObject("child", child.get());
}
