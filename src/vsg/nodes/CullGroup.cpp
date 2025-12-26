/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/stream.h>
#include <vsg/nodes/CullGroup.h>

using namespace vsg;

// 构造函数：创建剔除组节点
// 剔除组是一个包含边界球的组节点，用于对多个子节点进行视锥体剔除优化
CullGroup::CullGroup()
{
}

// 拷贝构造函数：从另一个剔除组创建新的剔除组
// rhs: 要拷贝的剔除组对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝边界球和所有子节点（通过基类Group）
CullGroup::CullGroup(const CullGroup& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    bound(rhs.bound)
{
}

// 构造函数：使用边界球创建剔除组
// in_bound: 边界球（用于视锥体剔除测试）
// 如果边界球在视锥体内，则渲染所有子节点
CullGroup::CullGroup(const dsphere& in_bound) :
    bound(in_bound)
{
}

// 析构函数：销毁剔除组节点
CullGroup::~CullGroup()
{
}

// 比较两个剔除组对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类Group，然后比较边界球
int CullGroup::compare(const Object& rhs_object) const
{
    int result = Group::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_value(bound, rhs.bound);
}

// 从输入流读取剔除组对象
// input: 输入流对象
// 读取边界球和所有子节点（通过基类Group）
void CullGroup::read(Input& input)
{
    Group::read(input);

    input.read("bound", bound);
}

// 将剔除组对象写入输出流
// output: 输出流对象
// 写入边界球和所有子节点（通过基类Group）
void CullGroup::write(Output& output) const
{
    Group::write(output);

    output.write("bound", bound);
}
