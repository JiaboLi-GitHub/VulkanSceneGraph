/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/EndQuery.h>

using namespace vsg;

// 构造函数：创建结束查询命令（默认）
// 结束查询命令用于结束查询统计信息的记录
EndQuery::EndQuery()
{
}

// 构造函数：使用查询池和查询索引创建结束查询命令
// pool: 查询池对象
// in_query: 查询索引
EndQuery::EndQuery(ref_ptr<QueryPool> pool, uint32_t in_query) :
    queryPool(pool),
    query(in_query)
{
}

// 从输入流读取结束查询命令对象
// input: 输入流对象
// 读取查询池和查询索引
void EndQuery::read(Input& input)
{
    Command::read(input);

    input.readObject("queryPool", queryPool);
    input.read("query", query);
}

// 将结束查询命令对象写入输出流
// output: 输出流对象
// 写入查询池和查询索引
void EndQuery::write(Output& output) const
{
    Command::write(output);

    output.writeObject("queryPool", queryPool);
    output.write("query", query);
}

// 编译结束查询命令
// context: 编译上下文对象
// 编译查询池对象
void EndQuery::compile(Context& context)
{
    if (queryPool) queryPool->compile(context);
}

// 记录结束查询命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdEndQuery命令，结束查询记录
void EndQuery::record(CommandBuffer& commandBuffer) const
{
    if (!queryPool) return;
    vkCmdEndQuery(commandBuffer, *queryPool, query);
}
