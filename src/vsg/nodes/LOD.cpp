
/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/stream.h>
#include <vsg/nodes/LOD.h>

using namespace vsg;

// 构造函数：创建LOD（细节级别）节点
// LOD节点根据观察距离自动选择不同细节级别的子节点，用于性能优化
LOD::LOD()
{
}

// 拷贝构造函数：从另一个LOD节点创建新的LOD节点
// rhs: 要拷贝的LOD节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝边界球和所有子节点（包括最小屏幕高度比例）
LOD::LOD(const LOD& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    bound(rhs.bound)
{
    children.reserve(rhs.children.size());
    for (auto child : rhs.children)
    {
        children.push_back(Child{child.minimumScreenHeightRatio, copyop(child.node)});
    }
}

// 析构函数：销毁LOD节点
LOD::~LOD()
{
}

// 比较两个LOD节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较边界球，最后比较子节点向量（最小屏幕高度比例和节点指针）
int LOD::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(bound, rhs.bound)) != 0) return result;

    // 比较子节点向量
    if (children.size() < rhs.children.size()) return -1;
    if (children.size() > rhs.children.size()) return 1;
    if (children.empty()) return 0;

    auto rhs_itr = rhs.children.begin();
    for (auto lhs_itr = children.begin(); lhs_itr != children.end(); ++lhs_itr, ++rhs_itr)
    {
        if ((result = compare_value(lhs_itr->minimumScreenHeightRatio, rhs_itr->minimumScreenHeightRatio)) != 0) return result;
        if ((result = compare_pointer(lhs_itr->node, rhs_itr->node)) != 0) return result;
    }
    return 0;
}

// 从输入流读取LOD节点对象
// input: 输入流对象
// 读取边界球和所有子节点（包括最小屏幕高度比例）
void LOD::read(Input& input)
{
    Node::read(input);

    input.read("bound", bound);

    children.resize(input.readValue<uint32_t>("children"));
    for (auto& child : children)
    {
        input.read("child.minimumScreenHeightRatio", child.minimumScreenHeightRatio);
        input.read("child.node", child.node);
    }
}

// 将LOD节点对象写入输出流
// output: 输出流对象
// 写入边界球和所有子节点（包括最小屏幕高度比例）
void LOD::write(Output& output) const
{
    Node::write(output);

    output.write("bound", bound);

    output.writeValue<uint32_t>("children", children.size());
    for (auto& child : children)
    {
        output.write("child.minimumScreenHeightRatio", child.minimumScreenHeightRatio);
        output.write("child.node", child.node);
    }
}
