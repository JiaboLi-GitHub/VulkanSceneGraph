/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/nodes/QuadGroup.h>

using namespace vsg;

// 构造函数：创建四叉组节点
// 四叉组是一个特殊的组节点，固定包含4个子节点（常用于四叉树结构）
QuadGroup::QuadGroup()
{
}

// 拷贝构造函数：从另一个四叉组创建新的四叉组
// rhs: 要拷贝的四叉组对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝所有4个子节点
QuadGroup::QuadGroup(const QuadGroup& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop)
{
    children[0] = copyop(rhs.children[0]);
    children[1] = copyop(rhs.children[1]);
    children[2] = copyop(rhs.children[2]);
    children[3] = copyop(rhs.children[3]);
}

// 析构函数：销毁四叉组节点
QuadGroup::~QuadGroup()
{
}

// 比较两个四叉组对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较子节点容器
int QuadGroup::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_pointer_container(children, rhs.children);
}

// 从输入流读取四叉组对象
// input: 输入流对象
// 读取所有4个子节点
void QuadGroup::read(Input& input)
{
    Node::read(input);

    for (auto& child : children)
    {
        input.readObject("Child", child);
    }
}

// 将四叉组对象写入输出流
// output: 输出流对象
// 写入所有4个子节点
void QuadGroup::write(Output& output) const
{
    Node::write(output);

    for (const auto& child : children)
    {
        output.writeObject("Child", child.get());
    }
}
