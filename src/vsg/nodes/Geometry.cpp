/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/io/ReaderWriter.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/vk/Context.h>

using namespace vsg;

/////////////////////////////////////////////////////////////////////////////////////////
//
//  Geometry节点
//      顶点数组
//      索引数组
//      绘制命令 + 索引绘制命令
//

// Geometry类的默认构造函数
// Geometry节点包含几何数据（顶点、索引）和绘制命令
Geometry::Geometry()
{
}

// Geometry类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作，包括数组、索引和命令的拷贝
Geometry::Geometry(const Geometry& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    firstBinding(rhs.firstBinding),
    arrays(copyop(rhs.arrays)),
    indices(copyop(rhs.indices)),
    commands(copyop(rhs.commands))
{
}

// Geometry类的析构函数
Geometry::~Geometry()
{
}

// 比较两个Geometry对象
// 依次比较基类、第一个绑定点、数组、索引和命令
int Geometry::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较第一个绑定点
    if ((result = compare_value(firstBinding, rhs.firstBinding)) != 0) return result;
    // 比较顶点数组容器
    if ((result = compare_pointer_container(arrays, rhs.arrays)) != 0) return result;
    // 比较索引
    if ((result = compare_pointer(indices, rhs.indices)) != 0) return result;
    // 比较命令容器
    return compare_pointer_container(commands, rhs.commands);
}

// 分配顶点数组
// 将数据列表转换为BufferInfo列表
void Geometry::assignArrays(const DataList& arrayData)
{
    arrays.clear();
    arrays.reserve(arrayData.size());
    // 为每个数据创建BufferInfo
    for (auto& data : arrayData)
    {
        arrays.push_back(BufferInfo::create(data));
    }
}

// 分配索引数据
// 将索引数据转换为BufferInfo，并计算索引类型
void Geometry::assignIndices(ref_ptr<vsg::Data> indexData)
{
    if (indexData)
    {
        // 创建索引BufferInfo并计算索引类型
        indices = BufferInfo::create(indexData);
        indexType = computeIndexType(indices->data);
    }
    else
    {
        indices = {};
    }
}

// 从输入流读取Geometry对象
// 读取第一个绑定点、顶点数组、索引和绘制命令
void Geometry::read(Input& input)
{
    Node::read(input);

    // 读取第一个绑定点
    input.read("firstBinding", firstBinding);

    // 读取顶点数组
    DataList dataList;
    dataList.resize(input.readValue<uint32_t>("NumArrays"));
    for (auto& array : dataList)
    {
        input.readObject("Array", array);
    }
    assignArrays(dataList);

    // 读取索引数据
    ref_ptr<vsg::Data> indices_data;
    input.readObject("Indices", indices_data);

    assignIndices(indices_data);

    // 读取绘制命令
    commands.resize(input.readValue<uint32_t>("NumCommands"));
    for (auto& command : commands)
    {
        input.readObject("Command", command);
    }
}

// 将Geometry对象写入输出流
// 写入第一个绑定点、顶点数组、索引和绘制命令
void Geometry::write(Output& output) const
{
    Node::write(output);

    // 写入第一个绑定点
    output.write("firstBinding", firstBinding);
    // 写入顶点数组数量和数据
    output.writeValue<uint32_t>("NumArrays", arrays.size());
    for (const auto& array : arrays)
    {
        if (array)
            output.writeObject("Array", array->data.get());
        else
            output.writeObject("Array", nullptr);
    }

    // 写入索引数据
    if (indices)
        output.writeObject("Indices", indices->data.get());
    else
        output.writeObject("Indices", nullptr);

    // 写入绘制命令数量和数据
    output.writeValue<uint32_t>("NumCommands", commands.size());
    for (const auto& command : commands)
    {
        output.writeObject("Command", command.get());
    }
}

// 编译Geometry对象
// 将几何数据编译为GPU可用的格式
void Geometry::compile(Context& context)
{
    // 如果数组或命令为空，则无法编译
    if (arrays.empty() || commands.empty())
    {
        // Geometry不包含必需的数组或命令
        return;
    }

    for (auto& command : commands)
    {
        command->compile(context);
    }

    auto deviceID = context.deviceID;

    bool requiresCreateAndCopy = false;
    if (indices && indices->requiresCopy(deviceID))
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

    if (requiresCreateAndCopy)
    {
        BufferInfoList combinedBufferInfos(arrays);
        if (indices)
        {
            combinedBufferInfos.push_back(indices);
        }

        createBufferAndTransferData(context, combinedBufferInfos, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    }

    assignVulkanArrayData(deviceID, arrays, _vulkanData[deviceID]);
}

void Geometry::record(CommandBuffer& commandBuffer) const
{
    auto& vkd = _vulkanData[commandBuffer.deviceID];

    VkCommandBuffer cmdBuffer{commandBuffer};

    vkCmdBindVertexBuffers(cmdBuffer, firstBinding, static_cast<uint32_t>(vkd.vkBuffers.size()), vkd.vkBuffers.data(), vkd.offsets.data());

    if (indices)
    {
        vkCmdBindIndexBuffer(cmdBuffer, indices->buffer->vk(commandBuffer.deviceID), indices->offset, indexType);
    }

    for (auto& command : commands)
    {
        command->record(commandBuffer);
    }
}
