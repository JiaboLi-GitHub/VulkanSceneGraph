/* <editor-fold desc="MIT License">

Copyright(c) 2022 Josef Stumpfegger & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/io/Logger.h>
#include <vsg/state/QueryPool.h>

using namespace vsg;

//////////////////////////////////////
//
// Query Pool
//

// 构造函数：创建查询池对象（默认）
// 查询池用于执行GPU查询操作（时间戳、遮挡查询、管线统计等）
QueryPool::QueryPool()
{
}

// 构造函数：使用设备、标志、查询类型、查询数量和管线统计标志创建查询池对象
// device: Vulkan设备对象
// in_flags: 创建标志
// in_queryType: 查询类型（时间戳、遮挡、管线统计等）
// in_queryCount: 查询数量
// in_pipelineStatistics: 管线统计标志（用于管线统计查询）
// 创建后立即编译
QueryPool::QueryPool(Device* device, VkQueryPoolCreateFlags in_flags, VkQueryType in_queryType, uint32_t in_queryCount, VkQueryPipelineStatisticFlags in_pipelineStatistics) :
    flags(in_flags),
    queryType(in_queryType),
    queryCount(in_queryCount),
    pipelineStatistics(in_pipelineStatistics)
{
    compile(device);
}

// 析构函数：销毁查询池对象
// 释放Vulkan查询池对象
QueryPool::~QueryPool()
{
    if (_queryPool)
        vkDestroyQueryPool(*_device, _queryPool, nullptr);
}

// 从输入流读取查询池对象
// input: 输入流对象
// 读取标志、查询类型、查询数量和管线统计标志
void QueryPool::read(Input& input)
{
    Object::read(input);

    input.readValue<uint32_t>("flags", flags);
    input.readValue<uint32_t>("queryType", queryType);
    input.readValue<uint32_t>("queryCount", queryCount);
    input.readValue<uint32_t>("pipelineStatisticFlags", pipelineStatistics);
}

// 将查询池对象写入输出流
// output: 输出流对象
// 写入标志、查询类型、查询数量和管线统计标志
void QueryPool::write(Output& output) const
{
    Object::write(output);

    output.writeValue<uint32_t>("flags", flags);
    output.writeValue<uint32_t>("queryType", queryType);
    output.writeValue<uint32_t>("queryCount", queryCount);
    output.writeValue<uint32_t>("pipelineStatisticFlags", pipelineStatistics);
}

// 重置查询池
// 重置查询池中的所有查询（需要VK_EXT_host_query_reset扩展支持）
void QueryPool::reset()
{
    if (!_queryPool) return;

    auto extensions = _device->getExtensions();
    if (extensions->vkResetQueryPool)
    {
        extensions->vkResetQueryPool(*_device, _queryPool, 0, queryCount);
    }
    else
    {
        warn("QueryPool::reset() vkResetQueryPool not supported by device/driver.");
    }
}

// 获取查询结果（32位）
// results: 输出参数，用于存储查询结果
// firstQuery: 第一个查询的索引
// resultsFlags: 结果标志（等待、部分结果等）
// 返回: Vulkan结果代码
// 从查询池获取32位查询结果
VkResult QueryPool::getResults(std::vector<uint32_t>& results, uint32_t firstQuery, VkQueryResultFlags resultsFlags) const
{
    if (!_queryPool) return VK_NOT_READY;
    if (firstQuery > queryCount) return VK_ERROR_UNKNOWN; // 超出范围

    uint32_t count = std::min(queryCount - firstQuery, static_cast<uint32_t>(results.size()));
    if (count == 0) return VK_ERROR_UNKNOWN; // 超出范围

    return vkGetQueryPoolResults(*_device, _queryPool, firstQuery, count, count * sizeof(uint32_t), results.data(), sizeof(uint32_t), resultsFlags);
}

// 获取查询结果（64位）
// results: 输出参数，用于存储查询结果
// firstQuery: 第一个查询的索引
// resultsFlags: 结果标志（等待、部分结果等）
// 返回: Vulkan结果代码
// 从查询池获取64位查询结果
VkResult QueryPool::getResults(std::vector<uint64_t>& results, uint32_t firstQuery, VkQueryResultFlags resultsFlags) const
{
    if (!_queryPool) return VK_NOT_READY;
    if (firstQuery > queryCount) return VK_ERROR_UNKNOWN; // 超出范围

    uint32_t count = std::min(queryCount - firstQuery, static_cast<uint32_t>(results.size()));
    if (count == 0) return VK_ERROR_UNKNOWN; // 超出范围

    return vkGetQueryPoolResults(*_device, _queryPool, firstQuery, count, count * sizeof(uint64_t), results.data(), sizeof(uint64_t), resultsFlags);
}

// 编译查询池（使用设备）
// device: Vulkan设备对象
// 创建Vulkan查询池对象
void QueryPool::compile(Device* device)
{
    if (_queryPool) return;
    _device = device;
    // 设置查询池创建信息
    VkQueryPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                     {},         //pNext
                                     flags,      //flags
                                     queryType,  //queryType
                                     queryCount, //queryCount
                                     pipelineStatistics};
    // 创建Vulkan查询池
    if (VkResult res = vkCreateQueryPool(*_device, &createInfo, nullptr, &_queryPool); res != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create QueryPool.", res};
    }
}

// 编译查询池（使用上下文）
// context: 编译上下文对象
// 委托给compile(Device*)方法
void QueryPool::compile(Context& context)
{
    compile(context.device);
}
