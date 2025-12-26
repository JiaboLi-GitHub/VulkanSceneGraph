/* <editor-fold desc="MIT License">

Copyright(c) 2025 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/nodes/InstanceNode.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建实例节点
// 实例节点用于使用实例化渲染技术高效地渲染多个相同几何体的副本
InstanceNode::InstanceNode()
{
}

// 拷贝构造函数：从另一个实例节点创建新的实例节点
// rhs: 要拷贝的实例节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝实例参数（首实例索引、实例数量）和实例数据（平移、旋转、缩放、颜色）以及子节点
InstanceNode::InstanceNode(const InstanceNode& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    firstInstance(rhs.firstInstance),
    instanceCount(rhs.instanceCount),
    translations(copyop(rhs.translations)),
    rotations(copyop(rhs.rotations)),
    scales(copyop(rhs.scales)),
    colors(copyop(rhs.colors)),
    child(copyop(rhs.child))
{
}

// 析构函数：销毁实例节点
InstanceNode::~InstanceNode()
{
}

// 比较两个实例节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、首实例索引、实例数量、平移、旋转、缩放、颜色和子节点
int InstanceNode::compare(const Object& rhs_object) const
{
    int result = Node::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(firstInstance, rhs.firstInstance)) != 0) return result;
    if ((result = compare_value(instanceCount, rhs.instanceCount)) != 0) return result;
    if ((result = compare_pointer(translations, rhs.translations)) != 0) return result;
    if ((result = compare_pointer(rotations, rhs.rotations)) != 0) return result;
    if ((result = compare_pointer(scales, rhs.scales)) != 0) return result;
    if ((result = compare_pointer(colors, rhs.colors)) != 0) return result;
    return compare_pointer(child, rhs.child);
}

// 从输入流读取实例节点对象
// input: 输入流对象
// 读取实例参数和实例数据（作为Data对象，然后转换为BufferInfo）
void InstanceNode::read(Input& input)
{
    Node::read(input);

    input.read("firstInstance", firstInstance);
    input.read("instanceCount", instanceCount);

    // 读取平移数据
    vsg::ref_ptr<Data> data;
    input.readObject("translations", data);
    if (data)
        translations = vsg::BufferInfo::create(data);
    else
        translations = {};

    // 读取旋转数据
    input.readObject("rotations", data);
    if (data)
        rotations = vsg::BufferInfo::create(data);
    else
        rotations = {};

    // 读取缩放数据
    input.readObject("scales", data);
    if (data)
        scales = vsg::BufferInfo::create(data);
    else
        scales = {};

    // 读取颜色数据
    input.readObject("colors", data);
    if (data)
        colors = vsg::BufferInfo::create(data);
    else
        colors = {};

    input.read("child", child);
}

// 将实例节点对象写入输出流
// output: 输出流对象
// 写入实例参数和实例数据（从BufferInfo提取Data对象）
void InstanceNode::write(Output& output) const
{
    Node::write(output);

    output.write("firstInstance", firstInstance);
    output.write("instanceCount", instanceCount);

    if (translations)
        output.writeObject("translations", translations->data);
    else
        output.writeObject("translations", nullptr);

    if (rotations)
        output.writeObject("rotations", rotations->data);
    else
        output.writeObject("rotations", nullptr);

    if (scales)
        output.writeObject("scales", scales->data);
    else
        output.writeObject("scales", nullptr);

    if (colors)
        output.writeObject("colors", colors->data);
    else
        output.writeObject("colors", nullptr);

    output.write("child", child);
}

// 编译实例节点
// context: 编译上下文对象
// 检查实例数据是否需要复制到GPU，如果需要则创建缓冲区并传输数据
void InstanceNode::compile(Context& context)
{
    auto deviceID = context.deviceID;
    bool requiresCreateAndCopy = false;

    // 检查哪些实例数据需要复制到GPU
    if (translations && translations->requiresCopy(deviceID)) requiresCreateAndCopy = true;
    if (rotations && rotations->requiresCopy(deviceID)) requiresCreateAndCopy = true;
    if (scales && scales->requiresCopy(deviceID)) requiresCreateAndCopy = true;
    if (colors && colors->requiresCopy(deviceID)) requiresCreateAndCopy = true;

    // 如果需要，创建缓冲区并传输数据
    if (requiresCreateAndCopy)
    {
        BufferInfoList combinedBufferInfos;
        if (translations) combinedBufferInfos.push_back(translations);
        if (rotations) combinedBufferInfos.push_back(rotations);
        if (scales) combinedBufferInfos.push_back(scales);
        if (colors) combinedBufferInfos.push_back(colors);

        // 创建顶点缓冲区并传输数据
        createBufferAndTransferData(context, combinedBufferInfos, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    }
}
