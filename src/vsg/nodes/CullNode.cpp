/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/stream.h>
#include <vsg/nodes/CullNode.h>

using namespace vsg;

// 构造函数：创建剔除节点
// 剔除节点包含一个边界球和一个子节点，用于视锥体剔除优化
CullNode::CullNode()
{
}

// 拷贝构造函数：从另一个剔除节点创建新的剔除节点
// rhs: 要拷贝的剔除节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝边界球和子节点
CullNode::CullNode(const CullNode& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    bound(rhs.bound),
    child(copyop(rhs.child))
{
}

// 构造函数：使用边界球和子节点创建剔除节点
// in_bound: 边界球（用于视锥体剔除测试）
// in_child: 子节点（如果边界球在视锥体内则渲染）
CullNode::CullNode(const dsphere& in_bound, Node* in_child) :
    bound(in_bound),
    child(in_child)
{
}

// 析构函数：销毁剔除节点
CullNode::~CullNode()
{
}

// 比较两个剔除节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较边界球，最后比较子节点
int CullNode::compare(const Object& rhs_object) const
{
    int result = Node::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(bound, rhs.bound)) != 0) return result;
    return compare_pointer(child, rhs.child);
}

// 从输入流读取剔除节点对象
// input: 输入流对象
// 读取边界球和子节点
void CullNode::read(Input& input)
{
    Node::read(input);

    input.read("bound", bound);
    input.read("child", child);
}

// 将剔除节点对象写入输出流
// output: 输出流对象
// 写入边界球和子节点
void CullNode::write(Output& output) const
{
    Node::write(output);

    output.write("bound", bound);
    output.write("child", child);
}
