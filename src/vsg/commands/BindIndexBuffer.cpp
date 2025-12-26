/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/core/compare.h>
#include <vsg/vk/CommandBuffer.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 提供定义，因为VK_INDEX_TYPE_UINT8_EXT在某些头文件中不可用
#define VK_INDEX_TYPE_UINT8 static_cast<VkIndexType>(1000265000)

// 计算索引类型
// indices: 索引数据对象
// 返回: Vulkan索引类型（根据数据元素大小：1字节=UINT8，2字节=UINT16，4字节=UINT32）
// 根据索引数据的元素大小自动确定Vulkan索引类型
VkIndexType vsg::computeIndexType(const Data* indices)
{
    if (indices)
    {
        switch (indices->valueSize())
        {
        case (1): return VK_INDEX_TYPE_UINT8;
        case (2): return VK_INDEX_TYPE_UINT16;
        case (4): return VK_INDEX_TYPE_UINT32;
        default: break;
        }
    }
    // 没有分配有效值
    return VK_INDEX_TYPE_MAX_ENUM;
}

// 构造函数：使用索引数据创建绑定索引缓冲区命令
// in_indices: 索引数据对象（将被转换为BufferInfo并计算索引类型）
BindIndexBuffer::BindIndexBuffer(ref_ptr<Data> in_indices)
{
    assignIndices(in_indices);
}

// 构造函数：使用索引类型和BufferInfo创建绑定索引缓冲区命令
// in_indexType: Vulkan索引类型
// in_indices: 索引缓冲区信息对象
BindIndexBuffer::BindIndexBuffer(VkIndexType in_indexType, ref_ptr<BufferInfo> in_indices) :
    indexType(in_indexType),
    indices(in_indices)
{
}

// 析构函数：销毁绑定索引缓冲区命令
BindIndexBuffer::~BindIndexBuffer()
{
}

// 比较两个绑定索引缓冲区命令对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较索引缓冲区信息
int BindIndexBuffer::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_pointer(indices, rhs.indices);
}

// 分配索引数据
// indexData: 索引数据对象
// 将索引数据转换为BufferInfo，并自动计算索引类型
void BindIndexBuffer::assignIndices(ref_ptr<vsg::Data> indexData)
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

// 从输入流读取绑定索引缓冲区命令对象
// input: 输入流对象
// 读取索引数据并分配
void BindIndexBuffer::read(Input& input)
{
    Command::read(input);

    // 读取关键索引数据
    ref_ptr<vsg::Data> indices_data;
    input.readObject("indices", indices_data);

    assignIndices(indices_data);
}

// 将绑定索引缓冲区命令对象写入输出流
// output: 输出流对象
// 写入索引数据
void BindIndexBuffer::write(Output& output) const
{
    Command::write(output);

    // 写入索引数据
    if (indices)
        output.writeObject("indices", indices->data);
    else
        output.writeObject("indices", nullptr);
}

// 编译绑定索引缓冲区命令
// context: 编译上下文对象
// 检查索引缓冲区是否需要复制到GPU，如果需要则创建缓冲区并传输数据
void BindIndexBuffer::compile(Context& context)
{
    // 如果没有索引，无需编译
    if (!indices) return;

    // 检查是否已编译
    if (indices->requiresCopy(context.deviceID))
    {
        createBufferAndTransferData(context, {indices}, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    }
}

// 记录绑定索引缓冲区命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdBindIndexBuffer命令，绑定索引缓冲区
void BindIndexBuffer::record(CommandBuffer& commandBuffer) const
{
    vkCmdBindIndexBuffer(commandBuffer, indices->buffer->vk(commandBuffer.deviceID), indices->offset, indexType);
}
