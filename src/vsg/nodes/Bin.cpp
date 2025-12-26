/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Logger.h>
#include <vsg/nodes/Bin.h>
#include <vsg/vk/State.h>

#include <algorithm>

using namespace vsg;

// 构造函数：创建bin节点
// bin节点用于收集和排序渲染元素，支持按值排序（升序、降序或不排序）
Bin::Bin()
{
}

// 拷贝构造函数：从另一个bin节点创建新的bin节点
// rhs: 要拷贝的bin节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝bin编号和排序顺序
Bin::Bin(const Bin& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    binNumber(rhs.binNumber),
    sortOrder(rhs.sortOrder)
{
}

// 构造函数：使用bin编号和排序顺序创建bin节点
// in_binNumber: bin编号（用于指定渲染顺序）
// in_sortOrder: 排序顺序（升序、降序或不排序）
Bin::Bin(int32_t in_binNumber, SortOrder in_sortOrder) :
    binNumber(in_binNumber),
    sortOrder(in_sortOrder)
{
}

// 析构函数：销毁bin节点
Bin::~Bin()
{
}

// 比较两个bin节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较bin编号和排序顺序
int Bin::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(binNumber, rhs.binNumber)) != 0) return result;
    return compare_value(sortOrder, rhs.sortOrder);
}

// 清除bin中的所有元素
// 清空矩阵、状态命令、元素和bin元素列表，为下一帧做准备
void Bin::clear()
{
    _matrices.clear();
    _stateCommands.clear();
    _elements.clear();
    _binElements.clear();
}

// 添加元素到bin
// state: 当前渲染状态
// value: 排序值（用于排序，如深度值）
// node: 要添加的节点
// 记录当前模型视图矩阵、状态命令和节点，创建元素并添加到bin中
void Bin::add(State* state, double value, const Node* node)
{
    //debug("Bin::add(state= ", state, ", value = ", value, ", ", node, ") ", this, ", binNumber = ", binNumber, ",  binElements.size()=", _binElements.size());

    Element element;

    // 获取当前模型视图矩阵
    const auto& mv = state->modelviewMatrixStack.top();
#if 1
    // 优化：如果矩阵与最后一个相同，重用索引
    if (_matrices.empty())
    {
        element.matrixIndex = static_cast<uint32_t>(_matrices.size());
        _matrices.push_back(mv);
    }
    else
    {
        if (_matrices.back() == mv)
        {
            //debug("reoccurring ");
            // 重用最后一个矩阵索引
            element.matrixIndex = static_cast<uint32_t>(_matrices.size()) - 1;
        }
        else
        {
            //debug("new ");
            // 添加新矩阵
            element.matrixIndex = static_cast<uint32_t>(_matrices.size());
            _matrices.push_back(mv);
        }
    }
#else
    // 简单版本：总是添加新矩阵
    element.matrixIndex = _matrices.size();
    _matrices.push_back(mv);
#endif

    // 记录状态命令
    element.stateCommandIndex = static_cast<uint32_t>(_stateCommands.size());
    for (const auto& stateStack : state->stateStacks)
    {
        if (stateStack.size() > 0)
        {
            _stateCommands.push_back(stateStack.top());
            ++element.stateCommandCount;
        }
    }

    element.child = node;

    // 添加bin元素（排序值和元素索引）
    _binElements.emplace_back(static_cast<float>(value), static_cast<uint32_t>(_elements.size()));

    _elements.push_back(element);
}

// 遍历bin并记录命令
// rt: 记录遍历器对象
// 根据排序顺序对元素排序，然后按顺序遍历每个元素，应用相应的矩阵和状态命令
void Bin::traverse(RecordTraversal& rt) const
{
    //debug("Bin::traverse(RecordTraversal& visitor) ", sortOrder, " ", _binElements.size());

    auto state = rt.getState();

    // 根据排序顺序对bin元素排序
    switch (sortOrder)
    {
    case (ASCENDING):
        // 升序排序
        std::sort(_binElements.begin(), _binElements.end(), [](const KeyIndex& lhs, const KeyIndex& rhs) { return lhs.first < rhs.first; });
        break;
    case (DESCENDING):
        // 降序排序
        std::sort(_binElements.begin(), _binElements.end(), [](const KeyIndex& lhs, const KeyIndex& rhs) { return rhs.first < lhs.first; });
        break;
    case (NO_SORT):
        // 不排序
        break;
    }

    uint32_t previousMatrixIndex = static_cast<uint32_t>(_matrices.size());
    //uint32_t previousStateCommandIndex = _stateCommands.size();

    // 推送视锥体
    state->pushFrustum();
    state->dirty = true;

    // 遍历排序后的元素
    for (const auto& keyElement : _binElements)
    {
        const auto& element = _elements[keyElement.second];

        // 如果矩阵索引改变，更新模型视图矩阵
        if (element.matrixIndex != previousMatrixIndex)
        {
            state->modelviewMatrixStack.push(_matrices[element.matrixIndex]);
            state->applyFrustum();
            state->dirty = true;
            previousMatrixIndex = element.matrixIndex;
            //debug("    updating");
        }
        else
        {
            //debug("    No need to update");
        }

        // 如果有状态命令，推送它们
        if (element.stateCommandCount > 0)
        {
            auto begin = _stateCommands.begin() + element.stateCommandIndex;
            auto end = begin + element.stateCommandCount;
            state->push(begin, end);

            // 访问子节点
            element.child->accept(rt);

            // 弹出状态命令
            state->pop(begin, end);
        }
        else
        {
            // 直接访问子节点
            element.child->accept(rt);
        }
    }

    // 弹出视锥体
    state->popFrustum();
    state->dirty = true;
}

// 从输入流读取bin节点对象
// input: 输入流对象
// 读取bin编号和排序顺序
void Bin::read(Input& input)
{
    Node::read(input);

    input.read("binNumber", binNumber);
    input.read("sortOrder", sortOrder);
}

// 将bin节点对象写入输出流
// output: 输出流对象
// 写入bin编号和排序顺序
void Bin::write(Output& output) const
{
    Node::write(output);

    output.write("binNumber", binNumber);
    output.write("sortOrder", sortOrder);
}
