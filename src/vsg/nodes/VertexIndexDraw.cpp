/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/io/Logger.h>
#include <vsg/io/ReaderWriter.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建顶点索引绘制命令节点
// 顶点索引绘制命令节点用于执行索引绘制（vkCmdDrawIndexed），支持实例化渲染
VertexIndexDraw::VertexIndexDraw()
{
}

// 拷贝构造函数：从另一个顶点索引绘制命令节点创建新的顶点索引绘制命令节点
// rhs: 要拷贝的顶点索引绘制命令节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝绘制参数（索引数量、实例数量、首索引、顶点偏移、首实例）和顶点数组、索引
VertexIndexDraw::VertexIndexDraw(const VertexIndexDraw& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    indexCount(rhs.indexCount),
    instanceCount(rhs.instanceCount),
    firstIndex(rhs.firstIndex),
    vertexOffset(rhs.vertexOffset),
    firstInstance(rhs.firstInstance),
    firstBinding(rhs.firstBinding),
    arrays(copyop(rhs.arrays)),
    indices(copyop(rhs.indices))
{
}

// 析构函数：销毁顶点索引绘制命令节点
VertexIndexDraw::~VertexIndexDraw()
{
}

// 比较两个顶点索引绘制命令节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、索引数量、实例数量、首索引、顶点偏移、首实例、首绑定点、顶点数组和索引
int VertexIndexDraw::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(indexCount, rhs.indexCount)) != 0) return result;
    if ((result = compare_value(instanceCount, rhs.instanceCount)) != 0) return result;
    if ((result = compare_value(firstIndex, rhs.firstIndex)) != 0) return result;
    if ((result = compare_value(firstInstance, rhs.firstInstance)) != 0) return result;
    if ((result = compare_value(vertexOffset, rhs.vertexOffset)) != 0) return result;
    if ((result = compare_value(firstBinding, rhs.firstBinding)) != 0) return result;
    if ((result = compare_pointer_container(arrays, rhs.arrays)) != 0) return result;
    return compare_pointer(indices, rhs.indices);
}

// 分配顶点数组
// arrayData: 数据列表
// 将数据列表转换为BufferInfo列表
void VertexIndexDraw::assignArrays(const DataList& arrayData)
{
    arrays.clear();
    arrays.reserve(arrayData.size());
    for (auto& data : arrayData)
    {
        arrays.push_back(BufferInfo::create(data));
    }
}

// 分配索引数据
// indexData: 索引数据对象
// 将索引数据转换为BufferInfo，并计算索引类型
void VertexIndexDraw::assignIndices(ref_ptr<vsg::Data> indexData)
{
    if (indexData)
    {
        indices = BufferInfo::create(indexData);
        indexType = computeIndexType(indices->data);
    }
    else
    {
        indices = {};
    }
}

// 从输入流读取顶点索引绘制命令节点对象
// input: 输入流对象
// 读取首绑定点、顶点数组、索引和绘制参数
void VertexIndexDraw::read(Input& input)
{
    _vulkanData.clear();

    Command::read(input);

    input.read("firstBinding", firstBinding);

    DataList dataList;
    dataList.resize(input.readValue<uint32_t>("NumArrays"));
    for (auto& array : dataList)
    {
        input.readObject("Array", array);
    }
    assignArrays(dataList);

    ref_ptr<vsg::Data> indices_data;
    input.readObject("Indices", indices_data);

    assignIndices(indices_data);

    // vkCmdDrawIndexed设置
    input.read("indexCount", indexCount);
    input.read("instanceCount", instanceCount);
    input.read("firstIndex", firstIndex);
    input.read("vertexOffset", vertexOffset);
    input.read("firstInstance", firstInstance);
}

// 将顶点索引绘制命令节点对象写入输出流
// output: 输出流对象
// 写入首绑定点、顶点数组、索引和绘制参数
void VertexIndexDraw::write(Output& output) const
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

    if (indices)
        output.writeObject("Indices", indices->data.get());
    else
        output.writeObject("Indices", nullptr);

    // vkCmdDrawIndexed设置
    output.write("indexCount", indexCount);
    output.write("instanceCount", instanceCount);
    output.write("firstIndex", firstIndex);
    output.write("vertexOffset", vertexOffset);
    output.write("firstInstance", firstInstance);
}

// 编译顶点索引绘制命令节点
// context: 编译上下文对象
// 检查顶点数组和索引是否需要复制到GPU，如果需要则创建缓冲区并传输数据
void VertexIndexDraw::compile(Context& context)
{
    if (arrays.empty() || !indices)
    {
        // VertexIndexDraw不包含必需的数组和索引
        return;
    }

    auto deviceID = context.deviceID;

    // 检查哪些数据需要复制到GPU
    bool requiresCreateAndCopy = false;
    if (indices->requiresCopy(deviceID))
        requiresCreateAndCopy = true;
    else
    {
        for (auto& array : arrays)
        {
            if (array->requiresCopy(deviceID))
            {
                requiresCreateAndCopy = true;
                break;
            }
        }
    }

    // 如果需要，创建缓冲区并传输数据
    if (requiresCreateAndCopy)
    {
        BufferInfoList combinedBufferInfos(arrays);
        combinedBufferInfos.push_back(indices);
        createBufferAndTransferData(context, combinedBufferInfos, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);

        // info("VertexIndexDraw::compile() create and copy ", this);
    }
    else
    {
        // info("VertexIndexDraw::compile() no need to create and copy ", this);
    }

    // 分配Vulkan数组数据
    assignVulkanArrayData(deviceID, arrays, _vulkanData[deviceID]);
}

// 记录顶点索引绘制命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 绑定顶点缓冲区和索引缓冲区，然后执行vkCmdDrawIndexed命令
void VertexIndexDraw::record(CommandBuffer& commandBuffer) const
{
    auto& vkd = _vulkanData[commandBuffer.deviceID];

    VkCommandBuffer cmdBuffer{commandBuffer};

    // 绑定顶点缓冲区
    vkCmdBindVertexBuffers(cmdBuffer, firstBinding, static_cast<uint32_t>(vkd.vkBuffers.size()), vkd.vkBuffers.data(), vkd.offsets.data());

    // 绑定索引缓冲区
    vkCmdBindIndexBuffer(cmdBuffer, indices->buffer->vk(commandBuffer.deviceID), indices->offset, indexType);

    // 执行索引绘制命令
    vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}
