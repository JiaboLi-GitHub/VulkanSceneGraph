/* <editor-fold desc="MIT License">

Copyright(c) 2022 Josef Stumpfegger & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/ResetQueryPool.h>

using namespace vsg;

// 构造函数：创建重置查询池命令（默认）
// 重置查询池命令用于重置查询池中的查询状态
ResetQueryPool::ResetQueryPool()
{
}

// 构造函数：使用查询池创建重置查询池命令
// pool: 查询池对象
// 自动设置首查询为0，查询数量为查询池的总查询数
ResetQueryPool::ResetQueryPool(ref_ptr<QueryPool> pool) :
    queryPool(pool),
    firstQuery(0),
    queryCount(pool->queryCount)
{
}

// 从输入流读取重置查询池命令对象
// input: 输入流对象
// 读取查询池、首查询索引和查询数量
void ResetQueryPool::read(Input& input)
{
    Command::read(input);

    input.readObject("queryPool", queryPool);
    input.read("firstQuery", firstQuery);
    input.read("queryCount", queryCount);
}

// 将重置查询池命令对象写入输出流
// output: 输出流对象
// 写入查询池、首查询索引和查询数量
void ResetQueryPool::write(Output& output) const
{
    Command::write(output);

    output.writeObject("queryPool", queryPool);
    output.write("firstQuery", firstQuery);
    output.write("queryCount", queryCount);
}

// 编译重置查询池命令
// context: 编译上下文对象
// 编译查询池对象
void ResetQueryPool::compile(Context& context)
{
    if (queryPool) queryPool->compile(context);
}

// 记录重置查询池命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdResetQueryPool命令，重置指定范围的查询
void ResetQueryPool::record(CommandBuffer& commandBuffer) const
{
    if (!queryPool) return;
    vkCmdResetQueryPool(commandBuffer, *queryPool, firstQuery, queryCount);
}
