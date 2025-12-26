/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/Commands.h>
#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>

using namespace vsg;

// 构造函数：创建命令集合节点
// numChildren: 子命令的初始数量
// 命令集合节点用于组织和管理多个Vulkan命令
Commands::Commands(size_t numChildren) :
    children(numChildren)
{
}

// 析构函数：销毁命令集合节点
Commands::~Commands()
{
}

// 比较两个命令集合对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较子命令容器
int Commands::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_pointer_container(children, rhs.children);
}

// 从输入流读取命令集合对象
// input: 输入流对象
// 读取所有子命令
void Commands::read(Input& input)
{
    Node::read(input);

    input.readObjects("children", children);
}

// 将命令集合对象写入输出流
// output: 输出流对象
// 写入所有子命令
void Commands::write(Output& output) const
{
    Node::write(output);

    output.writeObjects("children", children);
}

// 编译命令集合
// context: 编译上下文对象
// 为所有子命令调用编译方法，准备GPU资源
void Commands::compile(Context& context)
{
    for (auto& command : children)
    {
        command->compile(context);
    }
}

// 记录命令集合到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 按顺序将所有子命令记录到命令缓冲区中
void Commands::record(CommandBuffer& commandBuffer) const
{
    for (auto& command : children)
    {
        command->record(commandBuffer);
    }
}
