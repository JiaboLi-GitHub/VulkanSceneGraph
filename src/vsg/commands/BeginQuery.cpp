/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BeginQuery.h>

using namespace vsg;

// 构造函数：创建开始查询命令（默认）
// 开始查询命令用于开始记录查询统计信息（如遮挡查询、时间戳等）
BeginQuery::BeginQuery()
{
}

// 构造函数：使用查询池、查询索引和标志创建开始查询命令
// pool: 查询池对象
// in_query: 查询索引
// in_flags: 查询控制标志（如VK_QUERY_CONTROL_PRECISE_BIT用于遮挡查询）
BeginQuery::BeginQuery(ref_ptr<QueryPool> pool, uint32_t in_query, VkQueryControlFlags in_flags) :
    queryPool(pool),
    query(in_query),
    flags(in_flags)
{
}

// 从输入流读取开始查询命令对象
// input: 输入流对象
// 读取查询池、查询索引和标志
void BeginQuery::read(Input& input)
{
    Command::read(input);

    input.readObject("queryPool", queryPool);
    input.read("query", query);
    input.readValue<uint32_t>("flags", flags);
}

// 将开始查询命令对象写入输出流
// output: 输出流对象
// 写入查询池、查询索引和标志
void BeginQuery::write(Output& output) const
{
    Command::write(output);

    output.writeObject("queryPool", queryPool);
    output.write("query", query);
    output.writeValue<uint32_t>("flags", flags);
}

// 编译开始查询命令
// context: 编译上下文对象
// 编译查询池对象
void BeginQuery::compile(Context& context)
{
    if (queryPool) queryPool->compile(context);
}

// 记录开始查询命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdBeginQuery命令，开始记录查询
void BeginQuery::record(CommandBuffer& commandBuffer) const
{
    if (!queryPool) return;
    vkCmdBeginQuery(commandBuffer, *queryPool, query, flags);
}
