/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/nodes/StateGroup.h>

using namespace vsg;

// 构造函数：创建状态组节点
// 状态组节点用于管理渲染状态命令（如管道状态、描述符集等），并应用到所有子节点
StateGroup::StateGroup()
{
}

// 拷贝构造函数：从另一个状态组创建新的状态组
// rhs: 要拷贝的状态组对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝状态命令和原型数组状态
StateGroup::StateGroup(const StateGroup& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    stateCommands(copyop(rhs.stateCommands)),
    prototypeArrayState(rhs.prototypeArrayState)
{
}

// 析构函数：销毁状态组节点
StateGroup::~StateGroup()
{
}

// 比较两个状态组对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类Group，然后比较状态命令容器和原型数组状态
int StateGroup::compare(const Object& rhs_object) const
{
    int result = Group::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_pointer_container(stateCommands, rhs.stateCommands))) return result;
    return compare_pointer(prototypeArrayState, rhs.prototypeArrayState);
}

// 从输入流读取状态组对象
// input: 输入流对象
// 读取状态命令和原型数组状态
void StateGroup::read(Input& input)
{
    Group::read(input);

    input.readObjects("stateCommands", stateCommands);
    input.readObject("prototypeArrayState", prototypeArrayState);
}

// 将状态组对象写入输出流
// output: 输出流对象
// 写入状态命令和原型数组状态
void StateGroup::write(Output& output) const
{
    Group::write(output);

    output.writeObjects("stateCommands", stateCommands);
    output.write("prototypeArrayState", prototypeArrayState);
}
