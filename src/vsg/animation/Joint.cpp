/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/Joint.h>
#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>

using namespace vsg;

// Joint类的默认构造函数
// 创建关节节点，用于骨骼动画系统中的骨骼节点
Joint::Joint() :
    Inherit()
{
}

// Joint类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
Joint::Joint(const Joint& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    index(rhs.index),  // 关节索引
    name(rhs.name),  // 关节名称
    matrix(rhs.matrix),  // 关节变换矩阵
    children(copyop(rhs.children))  // 子关节列表
{
}

// Joint类的析构函数
Joint::~Joint()
{
}

// 比较两个Joint对象
// 首先比较基类，然后比较索引、名称、矩阵和子节点列表
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int Joint::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较关节索引
    if ((result = compare_value(index, rhs.index)) != 0) return result;
    // 比较关节名称
    if ((result = compare_value(name, rhs.name)) != 0) return result;
    // 比较变换矩阵
    if ((result = compare_value(matrix, rhs.matrix)) != 0) return result;
    // 比较子节点列表
    return compare_pointer_container(children, rhs.children);
}

// 从输入流读取Joint对象
// 读取关节的索引、名称、矩阵和子节点列表
void Joint::read(Input& input)
{
    Node::read(input);

    // 读取关节属性
    input.read("index", index);  // 关节索引
    input.read("name", name);  // 关节名称
    input.read("matrix", matrix);  // 关节变换矩阵
    input.readObjects("children", children);  // 子关节列表
}

// 将Joint对象写入输出流
// 写入关节的索引、名称、矩阵和子节点列表
void Joint::write(Output& output) const
{
    Node::write(output);

    // 写入关节属性
    output.write("index", index);
    output.write("name", name);
    output.write("matrix", matrix);
    output.writeObjects("children", children);
}
