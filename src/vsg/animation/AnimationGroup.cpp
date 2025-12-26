/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/AnimationGroup.h>
#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>

using namespace vsg;

// AnimationGroup类的构造函数
// 创建动画组，用于管理多个动画和子节点
// numChildren: 子节点数量
AnimationGroup::AnimationGroup(size_t numChildren) :
    Inherit(numChildren)
{
}

// AnimationGroup类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
AnimationGroup::AnimationGroup(const AnimationGroup& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    animations(copyop(rhs.animations))  // 动画列表
{
}

// AnimationGroup类的析构函数
AnimationGroup::~AnimationGroup()
{
}

// 比较两个AnimationGroup对象
// 首先比较基类，然后比较动画列表和子节点列表
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int AnimationGroup::compare(const Object& rhs_object) const
{
    int result = Node::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较动画列表
    if ((result = compare_pointer_container(animations, rhs.animations)) != 0) return result;
    // 比较子节点列表
    return compare_pointer_container(children, rhs.children);
}

// 从输入流读取AnimationGroup对象
// 读取动画列表和子节点列表
void AnimationGroup::read(Input& input)
{
    Node::read(input);

    // 读取动画列表和子节点列表
    input.readObjects("animations", animations);
    input.readObjects("children", children);
}

// 将AnimationGroup对象写入输出流
// 写入动画列表和子节点列表
void AnimationGroup::write(Output& output) const
{
    Node::write(output);

    // 写入动画列表和子节点列表
    output.writeObjects("animations", animations);
    output.writeObjects("children", children);
}
