/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/stream.h>
#include <vsg/nodes/InstrumentationNode.h>
#include <vsg/utils/Instrumentation.h>

using namespace vsg;

// 构造函数：创建性能分析节点
// 性能分析节点用于包装子节点，收集CPU和GPU性能数据，用于性能分析和调试
InstrumentationNode::InstrumentationNode() :
    _level(1),
    _color(255, 255, 255, 255),
    _name("InstrumentationNode"),
    _sl_Visitor{_name.c_str(), "InstrumentationNode::traverse(Visitor& rt)", __FILE__, 42, _color, _level},
    _sl_ConstVisitor{_name.c_str(), "InstrumentationNode::traverse(Visitor& rt)", __FILE__, 48, _color, _level},
    _sl_RecordTraversal{_name.c_str(), "InstrumentationNode::traverse(Visitor& rt)", __FILE__, 54, _color, _level}
{
}

// 拷贝构造函数：从另一个性能分析节点创建新的性能分析节点
// rhs: 要拷贝的性能分析节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝级别、颜色、名称和性能分析源位置信息
InstrumentationNode::InstrumentationNode(const InstrumentationNode& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    _level(rhs._level),
    _color(rhs._color),
    _name(rhs._name),
    _sl_Visitor{rhs._sl_Visitor},
    _sl_ConstVisitor{rhs._sl_ConstVisitor},
    _sl_RecordTraversal{rhs._sl_RecordTraversal}
{
    _sl_Visitor.name = _name.c_str();
    _sl_ConstVisitor.name = _name.c_str();
    _sl_RecordTraversal.name = _name.c_str();
}

// 构造函数：使用子节点创建性能分析节点
// in_child: 要包装的子节点
InstrumentationNode::InstrumentationNode(ref_ptr<Node> in_child) :
    InstrumentationNode()
{
    child = in_child;
}

// 析构函数：销毁性能分析节点
InstrumentationNode::~InstrumentationNode()
{
}

// 比较两个性能分析节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、级别、颜色和名称
int InstrumentationNode::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(_level, rhs._level)) != 0) return result;
    if ((result = compare_memory(_color, rhs._color)) != 0) return result;
    return compare_value(_name, rhs._name);
}

// 遍历访问者（非const版本）
// visitor: 访问者对象
// 使用CPU性能分析包装子节点的访问
void InstrumentationNode::traverse(Visitor& visitor)
{
    CpuInstrumentation cpuInst(visitor.getInstrumentation(), &_sl_Visitor, child.get());
    child->accept(visitor);
}

// 遍历常量访问者
// visitor: 常量访问者对象
// 使用CPU性能分析包装子节点的访问
void InstrumentationNode::traverse(ConstVisitor& visitor) const
{
    CpuInstrumentation cpuInst(visitor.getInstrumentation(), &_sl_ConstVisitor, child.get());
    child->accept(visitor);
}

// 遍历记录遍历器
// rt: 记录遍历器对象
// 使用GPU性能分析包装子节点的记录
void InstrumentationNode::traverse(RecordTraversal& rt) const
{
    GpuInstrumentation cpuInst(rt.instrumentation, &_sl_RecordTraversal, *rt.getCommandBuffer(), child.get());
    child->accept(rt);
}

// 设置性能分析颜色
// color: 颜色值（用于在性能分析工具中显示）
// 更新所有性能分析源位置的颜色
void InstrumentationNode::setColor(uint_color color)
{
    _color = color;
    _sl_Visitor.color = _color;
    _sl_ConstVisitor.color = _color;
    _sl_RecordTraversal.color = _color;
}

// 设置性能分析名称
// name: 名称字符串（用于在性能分析工具中标识）
// 更新所有性能分析源位置的名称
void InstrumentationNode::setName(const std::string& name)
{
    _name = name;
    if (_name.empty())
    {
        _sl_Visitor.name = nullptr;
        _sl_ConstVisitor.name = nullptr;
        _sl_RecordTraversal.name = nullptr;
    }
    else
    {
        _sl_Visitor.name = _name.c_str();
        _sl_ConstVisitor.name = _name.c_str();
        _sl_RecordTraversal.name = _name.c_str();
    }
}

// 设置性能分析级别
// level: 级别值（用于控制性能分析的详细程度）
// 更新所有性能分析源位置的级别
void InstrumentationNode::setLevel(uint32_t level)
{
    _level = level;
    _sl_Visitor.level = _level;
    _sl_ConstVisitor.level = _level;
    _sl_RecordTraversal.level = _level;
}

// 从输入流读取性能分析节点对象
// input: 输入流对象
// 读取级别、颜色、名称和子节点
void InstrumentationNode::read(Input& input)
{
    Node::read(input);

    uint32_t level = 0;
    uint_color color;
    std::string name;

    input.read("Level", level);
    input.read("Color", color);
    input.read("Name", name);

    setLevel(level);
    setColor(color);
    setName(name);

    input.read("child", child);
}

// 将性能分析节点对象写入输出流
// output: 输出流对象
// 写入级别、颜色、名称和子节点
void InstrumentationNode::write(Output& output) const
{
    Node::write(output);

    output.write("Level", _level);
    output.write("Color", _color);
    output.write("Name", _name);

    output.write("child", child);
}
