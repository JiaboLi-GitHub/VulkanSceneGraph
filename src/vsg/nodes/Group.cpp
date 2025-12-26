/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/nodes/Group.h>

using namespace vsg;

// Group类的构造函数
// 初始化指定数量的子节点容器
Group::Group(size_t numChildren) :
    children(numChildren)
{
}

// Group类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作，包括子节点的拷贝
Group::Group(const Group& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    children(copyop(rhs.children))
{
}

// Group类的析构函数
Group::~Group()
{
}

// 比较两个Group对象
// 首先比较基类，然后比较子节点容器
int Group::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较子节点容器
    return compare_pointer_container(children, rhs.children);
}

// 从输入流读取Group对象
// 读取子节点列表
void Group::read(Input& input)
{
    Node::read(input);

    input.readObjects("children", children);
}

// 将Group对象写入输出流
// 写入子节点列表
void Group::write(Output& output) const
{
    Node::write(output);

    output.writeObjects("children", children);
}
