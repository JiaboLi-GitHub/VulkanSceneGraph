/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/state/StateSwitch.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// 构造函数：创建状态切换对象（默认）
// 状态切换用于根据掩码条件性地记录状态命令（允许根据遍历掩码选择性地应用状态）
StateSwitch::StateSwitch()
{
}

// 拷贝构造函数：从另一个状态切换对象创建新的状态切换对象
// rhs: 要拷贝的状态切换对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝子节点列表（每个子节点包含掩码和状态命令）
StateSwitch::StateSwitch(const StateSwitch& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop)
{
    children.reserve(rhs.children.size());
    for (auto child : rhs.children)
    {
        children.push_back(Child{child.mask, copyop(child.stateCommand)});
    }
}

// 比较两个状态切换对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较子节点容器（比较每个子节点的掩码和状态命令）
int StateSwitch::compare(const Object& rhs_object) const
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
        if ((result = compare_pointer(lhs_itr->stateCommand, rhs_itr->stateCommand)) != 0) return result;
    }
    return 0;
}

// 编译状态切换
// context: 编译上下文对象
// 编译所有子节点的状态命令
void StateSwitch::compile(Context& context)
{
    for (auto& child : children) child.stateCommand->compile(context);
}

// 记录状态切换命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 根据遍历掩码和覆盖掩码条件性地记录子节点的状态命令
void StateSwitch::record(CommandBuffer& commandBuffer) const
{
    for (auto& child : children)
    {
        // 如果掩码匹配，记录状态命令
        if ((commandBuffer.traversalMask & (commandBuffer.overrideMask | child.mask)) != MASK_OFF)
        {
            child.stateCommand->record(commandBuffer);
        }
    }
}

void StateSwitch::read(Input& input)
{
    StateCommand::read(input);

    children.resize(input.readValue<uint32_t>("children"));
    for (auto& child : children)
    {
        input.read("child.mask", child.mask);
        input.read("child.stateCommand", child.stateCommand);
    }
}

void StateSwitch::write(Output& output) const
{
    StateCommand::write(output);

    output.writeValue<uint32_t>("children", children.size());
    for (auto& child : children)
    {
        output.write("child.mask", child.mask);
        output.write("child.stateCommand", child.stateCommand);
    }
}
