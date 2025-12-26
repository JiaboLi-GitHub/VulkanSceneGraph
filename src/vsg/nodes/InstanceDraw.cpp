/* <editor-fold desc="MIT License">

Copyright(c) 2025 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/io/Logger.h>
#include <vsg/io/ReaderWriter.h>
#include <vsg/nodes/InstanceDraw.h>
#include <vsg/nodes/InstanceNode.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建实例绘制命令节点
// 实例绘制命令节点用于执行实例化无索引绘制（vkCmdDraw），从InstanceNode获取实例数据
InstanceDraw::InstanceDraw()
{
}

// 拷贝构造函数：从另一个实例绘制命令节点创建新的实例绘制命令节点
// rhs: 要拷贝的实例绘制命令节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝绘制参数（顶点数量、首顶点）和顶点数组
InstanceDraw::InstanceDraw(const InstanceDraw& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    vertexCount(rhs.vertexCount),
    firstVertex(rhs.firstVertex),
    firstBinding(rhs.firstBinding),
    arrays(copyop(rhs.arrays))
{
}

// 析构函数：销毁实例绘制命令节点
InstanceDraw::~InstanceDraw()
{
}

// 比较两个实例绘制命令节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、顶点数量、首顶点、首绑定点和顶点数组
int InstanceDraw::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(vertexCount, rhs.vertexCount)) != 0) return result;
    if ((result = compare_value(firstVertex, rhs.firstVertex)) != 0) return result;
    if ((result = compare_value(firstBinding, rhs.firstBinding)) != 0) return result;
    return compare_pointer_container(arrays, rhs.arrays);
}

// 分配顶点数组
// arrayData: 数据列表
// 将数据列表转换为BufferInfo列表
void InstanceDraw::assignArrays(const DataList& arrayData)
{
    arrays.clear();
    arrays.reserve(arrayData.size());
    for (auto& data : arrayData)
    {
        arrays.push_back(BufferInfo::create(data));
    }
}

// 从输入流读取实例绘制命令节点对象
// input: 输入流对象
// 读取首绑定点、顶点数组和绘制参数（顶点数量、首顶点）
void InstanceDraw::read(Input& input)
{
    Command::read(input);

    input.read("firstBinding", firstBinding);

    DataList dataList;
    dataList.resize(input.readValue<uint32_t>("NumArrays"));
    for (auto& array : dataList)
    {
        input.readObject("Array", array);
    }
    assignArrays(dataList);

    // vkCmdDraw设置
    input.read("vertexCount", vertexCount);
    input.read("firstVertex", firstVertex);
}

// 将实例绘制命令节点对象写入输出流
// output: 输出流对象
// 写入首绑定点、顶点数组和绘制参数
void InstanceDraw::write(Output& output) const
{
    Command::write(output);

    output.write("firstBinding", firstBinding);
    output.writeValue<uint32_t>("NumArrays", arrays.size());
    for (const auto& array : arrays)
    {
        if (array)
            output.writeObject("Array", array->data.get());
        else
            output.writeObject("Array", nullptr);
    }

    // vkCmdDrawIndexed设置
    output.write("vertexCount", vertexCount);
    output.write("firstVertex", firstVertex);
}

// 编译实例绘制命令节点
// context: 编译上下文对象
// 检查顶点数组是否需要复制到GPU，如果需要则创建缓冲区并传输数据
void InstanceDraw::compile(Context& context)
{
    if (arrays.empty())
    {
        // InstanceDraw不包含必需的数组和索引
        return;
    }

    auto deviceID = context.deviceID;

    // 检查哪些顶点数组需要复制到GPU
    bool requiresCreateAndCopy = false;
    for (auto& array : arrays)
    {
        if (array->requiresCopy(deviceID))
        {
            requiresCreateAndCopy = true;
            break;
        }
    }

    // 如果需要，创建缓冲区并传输数据
    if (requiresCreateAndCopy)
    {
        createBufferAndTransferData(context, arrays, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);

        // info("InstanceDraw::compile() create and copy ", this);
    }
    else
    {
        // info("InstanceDraw::compile() no need to create and copy ", this);
    }
}

// 记录实例绘制命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 从命令缓冲区获取InstanceNode，合并顶点数组和实例数据，绑定顶点缓冲区并执行vkCmdDraw命令
void InstanceDraw::record(CommandBuffer& commandBuffer) const
{
    // 从命令缓冲区获取InstanceNode（必须存在）
    auto instanceNode = commandBuffer.instanceNode;
    if (!instanceNode)
    {
        vsg::info("InstanceDraw::record() required vsg::InstanceNode not provided.");
        return;
    }

    auto deviceID = commandBuffer.deviceID;
    VkCommandBuffer cmdBuffer{commandBuffer};

    // 准备顶点缓冲区列表
    std::vector<VkBuffer> vkBuffers;
    std::vector<VkDeviceSize> offsets;

    vkBuffers.reserve(8);
    offsets.reserve(8);

    // Lambda函数：将BufferInfo添加到缓冲区列表
    auto assignBufferInfo = [&](const ref_ptr<BufferInfo>& bufferInfo) -> void {
        vkBuffers.push_back(bufferInfo->buffer->vk(deviceID));
        offsets.push_back(bufferInfo->offset);
    };

    // 添加本地顶点数组
    for (auto& bi : arrays)
    {
        assignBufferInfo(bi);
    }

    // 添加InstanceNode的实例数据（颜色、平移、旋转、缩放）
    if (instanceNode->colors) assignBufferInfo(instanceNode->colors);
    if (instanceNode->translations) assignBufferInfo(instanceNode->translations);
    if (instanceNode->rotations) assignBufferInfo(instanceNode->rotations);
    if (instanceNode->scales) assignBufferInfo(instanceNode->scales);

    // TODO: 需要合并继承的InstanceNode值与本地数组的值
    // 绑定顶点缓冲区
    vkCmdBindVertexBuffers(cmdBuffer, firstBinding, static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data(), offsets.data());

    // vsg::info("InstanceDraw::record(CommandBuffer& commandBuffer) vkCmdDraw vkBuffers.size() = ", vkBuffers.size(), ", vertexCount = ", vertexCount, ", instanceNode->instanceCount = ", instanceNode->instanceCount);
    // 执行实例化无索引绘制命令（使用InstanceNode的实例数量和首实例）
    vkCmdDraw(cmdBuffer, vertexCount, instanceNode->instanceCount, firstVertex, instanceNode->firstInstance);
}
