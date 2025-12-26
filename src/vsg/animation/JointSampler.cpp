/* <editor-fold desc="MIT License">
`
Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/AnimationGroup.h>
#include <vsg/animation/Joint.h>
#include <vsg/animation/JointSampler.h>
#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/state/DescriptorBuffer.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// JointSampler - 关节采样器，用于计算骨骼动画的关节变换矩阵
//

// JointSampler类的默认构造函数
// 创建关节采样器，用于计算骨骼动画中所有关节的最终变换矩阵
JointSampler::JointSampler()
{
}

// JointSampler类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
JointSampler::JointSampler(const JointSampler& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    jointMatrices(copyop(rhs.jointMatrices)),  // 关节矩阵数组（输出）
    offsetMatrices(rhs.offsetMatrices),  // 关节偏移矩阵数组
    subgraph(copyop(rhs.subgraph))  // 包含关节的子图
{
}

// 比较两个JointSampler对象
// 首先比较基类，然后比较关节矩阵、偏移矩阵和子图
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int JointSampler::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较关节矩阵
    if ((result = compare_pointer(jointMatrices, rhs.jointMatrices)) != 0) return result;
    // 比较偏移矩阵容器
    if ((result = compare_value_container(offsetMatrices, rhs.offsetMatrices)) != 0) return result;
    // 比较子图
    return compare_pointer(subgraph, rhs.subgraph);
}

// 更新关节矩阵
// 遍历关节树，计算所有关节的最终变换矩阵
// time: 动画时间（未使用，关节动画由场景图状态控制）
void JointSampler::update(double)
{
    if (!jointMatrices) return;

    // 初始化矩阵栈（从单位矩阵开始）
    _matrixStack.clear();
    _matrixStack.push_back(dmat4());

    // 遍历子图，计算关节矩阵
    if (subgraph)
    {
        subgraph->accept(*this);
    }

    // 标记关节矩阵为已修改
    jointMatrices->dirty();
}

// 计算最大时间
// 关节采样器不使用时间，返回0
// 返回值：最大时间（始终为0）
double JointSampler::maxTime() const
{
    double maxTime = 0.0;
    return maxTime;
}

// 从输入流读取JointSampler对象
// 读取关节矩阵、偏移矩阵和子图
void JointSampler::read(Input& input)
{
    AnimationSampler::read(input);
    input.read("jointMatrices", jointMatrices);  // 关节矩阵数组
    input.readValues("offsetMatrices", offsetMatrices);  // 偏移矩阵数组
    input.read("subgraph", subgraph);  // 包含关节的子图
}

// 将JointSampler对象写入输出流
// 写入关节矩阵、偏移矩阵和子图
void JointSampler::write(Output& output) const
{
    AnimationSampler::write(output);
    output.write("jointMatrices", jointMatrices);
    output.writeValues("offsetMatrices", offsetMatrices);
    output.write("subgraph", subgraph);
}

// 应用访问者到Node对象
// 继续遍历节点树
void JointSampler::apply(Node& node)
{
    node.traverse(*this);
}

// 应用访问者到Transform对象
// 将变换应用到矩阵栈，然后遍历子节点
void JointSampler::apply(Transform& transform)
{
    if (!transform.children.empty())
    {
        // 将变换应用到当前矩阵栈顶
        _matrixStack.push_back(transform.transform(_matrixStack.back()));

        // 遍历子节点
        transform.traverse(*this);

        // 恢复矩阵栈
        _matrixStack.pop_back();
    }
}

// 应用访问者到MatrixTransform对象
// 将矩阵变换应用到矩阵栈，然后遍历子节点
void JointSampler::apply(MatrixTransform& mt)
{
    if (!mt.children.empty())
    {
        // 将矩阵变换应用到当前矩阵栈顶
        _matrixStack.push_back(_matrixStack.back() * mt.matrix);

        // 遍历子节点
        mt.traverse(*this);

        // 恢复矩阵栈
        _matrixStack.pop_back();
    }
}

// 应用访问者到Joint对象
// 计算关节的最终变换矩阵并存储到关节矩阵数组中
void JointSampler::apply(Joint& joint)
{
    // 计算关节的最终变换矩阵（当前矩阵栈顶 * 关节矩阵）
    auto matrix = _matrixStack.back() * joint.matrix;
    // 应用偏移矩阵并存储到关节矩阵数组
    jointMatrices->set(joint.index, mat4(matrix * offsetMatrices[joint.index]));

    // 如果有子关节，继续遍历
    if (!joint.children.empty())
    {
        // 将当前关节矩阵压入栈
        _matrixStack.push_back(matrix);

        // 遍历子关节
        for (auto& child : joint.children)
        {
            // apply(*child);
            child->accept(*this);
        }

        // 恢复矩阵栈
        _matrixStack.pop_back();
    }
}
