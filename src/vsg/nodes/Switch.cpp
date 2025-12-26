/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/nodes/Switch.h>

using namespace vsg;

// 构造函数：创建开关节点
// 开关节点可以根据掩码选择性地启用或禁用子节点，用于场景切换和条件渲染
Switch::Switch()
{
}

// 拷贝构造函数：从另一个开关节点创建新的开关节点
// rhs: 要拷贝的开关节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝所有子节点及其掩码
Switch::Switch(const Switch& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop)
{
    children.reserve(rhs.children.size());
    for (auto child : rhs.children)
    {
        children.push_back(Child{child.mask, copyop(child.node)});
    }
}

// 析构函数：销毁开关节点
Switch::~Switch()
{
}

// 比较两个开关节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较子节点数量和内容（掩码和节点指针）
int Switch::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    auto& rhs = static_cast<decltype(*this)>(rhs_object);

    // 比较子节点向量
    if (children.size() < rhs.children.size()) return -1;
    if (children.size() > rhs.children.size()) return 1;
    if (children.empty()) return 0;

    auto rhs_itr = rhs.children.begin();
    for (auto lhs_itr = children.begin(); lhs_itr != children.end(); ++lhs_itr, ++rhs_itr)
    {
        if ((result = compare_value(lhs_itr->mask, rhs_itr->mask)) != 0) return result;
        if ((result = compare_pointer(lhs_itr->node, rhs_itr->node)) != 0) return result;
    }
    return 0;
}

// 从输入流读取开关节点对象
// input: 输入流对象
// 读取子节点数量和每个子节点的掩码和节点
void Switch::read(Input& input)
{
    Node::read(input);

    children.resize(input.readValue<uint32_t>("children"));
    for (auto& child : children)
    {
        input.read("child.mask", child.mask);
        input.read("child.node", child.node);
    }
}

// 将开关节点对象写入输出流
// output: 输出流对象
// 写入子节点数量和每个子节点的掩码和节点
void Switch::write(Output& output) const
{
    Node::write(output);

    output.writeValue<uint32_t>("children", children.size());
    for (auto& child : children)
    {
        output.write("child.mask", child.mask);
        output.write("child.node", child.node);
    }
}

// 添加子节点（使用掩码）
// mask: 子节点的掩码（用于控制是否渲染）
// child: 要添加的子节点
// 将子节点添加到列表中，使用指定的掩码
void Switch::addChild(vsg::Mask mask, ref_ptr<Node> child)
{
    children.push_back(Child{mask, child});
}

// 添加子节点（使用布尔值）
// enabled: 是否启用子节点
// child: 要添加的子节点
// 将布尔值转换为掩码后添加子节点
void Switch::addChild(bool enabled, ref_ptr<Node> child)
{
    children.push_back(Child{boolToMask(enabled), child});
}

// 设置所有子节点的启用状态
// enabled: 是否启用所有子节点
// 将所有子节点的掩码设置为相同的值
void Switch::setAllChildren(bool enabled)
{
    Mask mask = boolToMask(enabled);
    for (auto& child : children) child.mask = mask;
}

// 设置单个子节点启用（其他禁用）
// index: 要启用的子节点索引
// 将指定索引的子节点启用，其他子节点禁用
void Switch::setSingleChildOn(size_t index)
{
    for (size_t i = 0; i < children.size(); ++i)
    {
        children[i].mask = boolToMask(i == index);
    }
}
