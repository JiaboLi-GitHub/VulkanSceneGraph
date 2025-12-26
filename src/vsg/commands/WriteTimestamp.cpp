/* <editor-fold desc="MIT License">

Copyright(c) 2022 Josef Stumpfegger & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/WriteTimestamp.h>

using namespace vsg;

// 构造函数：创建写入时间戳命令（默认）
// 写入时间戳命令用于在指定管道阶段记录时间戳，用于性能分析
WriteTimestamp::WriteTimestamp()
{
}

// 构造函数：使用管道阶段、查询池和查询索引创建写入时间戳命令
// stage: 管道阶段标志位（指定在哪个阶段记录时间戳）
// pool: 查询池对象（必须是时间戳查询池）
// in_query: 查询索引
WriteTimestamp::WriteTimestamp(VkPipelineStageFlagBits stage, ref_ptr<QueryPool> pool, uint32_t in_query) :
    pipelineStage(stage),
    queryPool(pool),
    query(in_query)
{
}

// 从输入流读取写入时间戳命令对象
// input: 输入流对象
// 读取管道阶段、查询池和查询索引
void WriteTimestamp::read(Input& input)
{
    Command::read(input);

    input.readValue<uint32_t>("pipelineStage", pipelineStage);
    input.readObject("queryPool", queryPool);
    input.read("query", query);
}

// 将写入时间戳命令对象写入输出流
// output: 输出流对象
// 写入管道阶段、查询池和查询索引
void WriteTimestamp::write(Output& output) const
{
    Command::write(output);

    output.writeValue<uint32_t>("pipelineStage", pipelineStage);
    output.writeObject("queryPool", queryPool);
    output.write("query", query);
}

// 编译写入时间戳命令
// context: 编译上下文对象
// 编译查询池对象
void WriteTimestamp::compile(Context& context)
{
    if (queryPool) queryPool->compile(context);
}

// 记录写入时间戳命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdWriteTimestamp命令，在指定管道阶段记录时间戳
void WriteTimestamp::record(CommandBuffer& commandBuffer) const
{
    if (!queryPool) return;
    vkCmdWriteTimestamp(commandBuffer, pipelineStage, *queryPool, query);
}
