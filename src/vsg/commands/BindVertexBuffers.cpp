/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BindVertexBuffers.h>
#include <vsg/core/compare.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：使用首绑定点和数据列表创建绑定顶点缓冲区命令
// in_firstBinding: 首绑定点索引（从哪个绑定点开始绑定）
// in_arrays: 顶点数据列表（将被转换为BufferInfo）
BindVertexBuffers::BindVertexBuffers(uint32_t in_firstBinding, const DataList& in_arrays) :
    firstBinding(in_firstBinding)
{
    assignArrays(in_arrays);
}

// 析构函数：销毁绑定顶点缓冲区命令
BindVertexBuffers::~BindVertexBuffers()
{
}

// 比较两个绑定顶点缓冲区命令对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较首绑定点和顶点数组容器
int BindVertexBuffers::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(firstBinding, rhs.firstBinding))) return result;
    return compare_pointer_container(arrays, rhs.arrays);
}

// 分配顶点数组
// arrayData: 数据列表
// 将数据列表转换为BufferInfo列表
void BindVertexBuffers::assignArrays(const DataList& arrayData)
{
    arrays.clear();
    arrays.reserve(arrayData.size());
    for (auto& data : arrayData)
    {
        arrays.push_back(BufferInfo::create(data));
    }
}

// 从输入流读取绑定顶点缓冲区命令对象
// input: 输入流对象
// 清除Vulkan对象，然后读取首绑定点和顶点数组
void BindVertexBuffers::read(Input& input)
{
    Command::read(input);

    // 清除Vulkan对象
    _vulkanData.clear();

    input.read("firstBinding", firstBinding);

    DataList dataList;
    dataList.resize(input.readValue<uint32_t>("arrays"));
    for (auto& array : dataList)
    {
        input.readObject("array", array);
    }
    assignArrays(dataList);
}

// 将绑定顶点缓冲区命令对象写入输出流
// output: 输出流对象
// 写入首绑定点和顶点数组
void BindVertexBuffers::write(Output& output) const
{
    Command::write(output);

    output.write("firstBinding", firstBinding);

    output.writeValue<uint32_t>("arrays", arrays.size());
    for (const auto& array : arrays)
    {
        if (array)
            output.writeObject("array", array->data.get());
        else
            output.writeObject("array", nullptr);
    }
}

// 编译绑定顶点缓冲区命令
// context: 编译上下文对象
// 检查顶点数组是否需要复制到GPU，如果需要则创建缓冲区并传输数据，然后分配Vulkan数组数据
void BindVertexBuffers::compile(Context& context)
{
    // 如果没有数组，无需编译
    if (arrays.empty()) return;

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
        createBufferAndTransferData(context, arrays, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    }

    // 分配Vulkan数组数据
    assignVulkanArrayData(deviceID, arrays, _vulkanData[deviceID]);
}

// 记录绑定顶点缓冲区命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdBindVertexBuffers命令，绑定顶点缓冲区
void BindVertexBuffers::record(CommandBuffer& commandBuffer) const
{
    auto& vkd = _vulkanData[commandBuffer.deviceID];
    vkCmdBindVertexBuffers(commandBuffer, firstBinding, static_cast<uint32_t>(vkd.vkBuffers.size()), vkd.vkBuffers.data(), vkd.offsets.data());
}
