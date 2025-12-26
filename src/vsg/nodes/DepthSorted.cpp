/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/stream.h>
#include <vsg/nodes/DepthSorted.h>

using namespace vsg;

// 构造函数：创建深度排序节点
// 深度排序节点根据子节点的深度值将其分配到bin中，用于实现正确的透明物体渲染顺序
DepthSorted::DepthSorted()
{
}

// 拷贝构造函数：从另一个深度排序节点创建新的深度排序节点
// rhs: 要拷贝的深度排序节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝bin编号、边界球和子节点
DepthSorted::DepthSorted(const DepthSorted& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    binNumber(rhs.binNumber),
    bound(rhs.bound),
    child(copyop(rhs.child))
{
}

// 构造函数：使用bin编号、边界球和子节点创建深度排序节点
// in_binNumber: bin编号（用于指定渲染顺序）
// in_bound: 边界球（用于计算深度值）
// in_child: 子节点（将根据深度值排序）
DepthSorted::DepthSorted(int32_t in_binNumber, const dsphere& in_bound, ref_ptr<Node> in_child) :
    binNumber(in_binNumber),
    bound(in_bound),
    child(in_child)
{
}

// 析构函数：销毁深度排序节点
DepthSorted::~DepthSorted()
{
}

// 比较两个深度排序节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、bin编号、边界球和子节点
int DepthSorted::compare(const Object& rhs_object) const
{
    int result = Node::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(binNumber, rhs.binNumber)) != 0) return result;
    if ((result = compare_value(bound, rhs.bound)) != 0) return result;
    return compare_pointer(child, rhs.child);
}

// 从输入流读取深度排序节点对象
// input: 输入流对象
// 读取bin编号、边界球和子节点
void DepthSorted::read(Input& input)
{
    Node::read(input);

    input.read("binNumber", binNumber);
    input.read("bound", bound);
    input.readObject("child", child);
}

// 将深度排序节点对象写入输出流
// output: 输出流对象
// 写入bin编号、边界球和子节点
void DepthSorted::write(Output& output) const
{
    Node::write(output);

    output.write("binNumber", binNumber);
    output.write("bound", bound);
    output.writeObject("child", child.get());
}
