/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/state/ResourceHints.h>

using namespace vsg;

// 构造函数：创建资源提示对象（默认）
// 资源提示用于向系统提供资源分配的建议（缓冲区大小、描述符池大小、光源数量范围等）
ResourceHints::ResourceHints()
{
}

// 析构函数：销毁资源提示对象
ResourceHints::~ResourceHints()
{
}

// 从输入流读取资源提示对象
// input: 输入流对象
// 读取最大槽位（版本1.1.11及以上支持状态和视图槽位）、描述符集数量、描述符池大小、最小缓冲区大小、
// 最小设备内存大小、最小临时缓冲区大小（版本1.1.8及以上）、光源数量范围、阴影贴图数量范围、阴影贴图大小（版本1.0.10及以上）、
// 数据库分页器读取线程数、数据传输提示（版本1.1.8及以上）、视口状态提示（版本1.1.11及以上）
void ResourceHints::read(Input& input)
{
    Object::read(input);

    if (input.version_greater_equal(1, 1, 11))
    {
        input.read("maxSlots", maxSlots.state, maxSlots.view);
    }
    else
    {
        input.read("maxSlot", maxSlots.state);
        maxSlots.view = maxSlots.state;
    }

    input.read("numDescriptorSets", numDescriptorSets);

    if (input.version_greater_equal(0, 7, 3))
        descriptorPoolSizes.resize(input.readValue<uint32_t>("descriptorPoolSizes"));
    else
        descriptorPoolSizes.resize(input.readValue<uint32_t>("NumDescriptorPoolSize"));

    for (auto& [type, count] : descriptorPoolSizes)
    {
        input.readValue<uint32_t>("type", type);
        input.read("count", count);
    }

    input.read("minimumBufferSize", minimumBufferSize);
    input.read("minimumDeviceMemorySize", minimumDeviceMemorySize);

    if (input.version_greater_equal(1, 1, 8))
    {
        input.read("minimumStagingBufferSize", minimumStagingBufferSize);
    }

    if (input.version_greater_equal(1, 0, 10))
    {
        input.read("numLightsRange", numLightsRange);
        input.read("numShadowMapsRange", numShadowMapsRange);
        input.read("shadowMapSize", shadowMapSize);
    }

    if (input.version_greater_equal(1, 1, 8))
    {
        input.read("numDatabasePagerReadThreads", numDatabasePagerReadThreads);
        input.readValue<uint32_t>("dataTransferHint", dataTransferHint);
    }

    if (input.version_greater_equal(1, 1, 11))
    {
        input.read("viewportStateHint", viewportStateHint);
    }
}

// 将资源提示对象写入输出流
// output: 输出流对象
// 写入所有资源提示参数
void ResourceHints::write(Output& output) const
{
    Object::write(output);

    if (output.version_greater_equal(1, 1, 11))
    {
        output.write("maxSlots", maxSlots.state, maxSlots.view);
    }
    else
    {
        output.write("maxSlot", maxSlots.state);
    }

    output.write("numDescriptorSets", numDescriptorSets);

    if (output.version_greater_equal(0, 7, 3))
        output.writeValue<uint32_t>("descriptorPoolSizes", descriptorPoolSizes.size());
    else
        output.writeValue<uint32_t>("NumDescriptorPoolSize", descriptorPoolSizes.size());

    for (auto& [type, count] : descriptorPoolSizes)
    {
        output.writeValue<uint32_t>("type", type);
        output.write("count", count);
    }

    output.write("minimumBufferSize", minimumBufferSize);
    output.write("minimumDeviceMemorySize", minimumDeviceMemorySize);

    if (output.version_greater_equal(1, 1, 8))
    {
        output.write("minimumStagingBufferSize", minimumStagingBufferSize);
    }

    if (output.version_greater_equal(1, 0, 10))
    {
        output.write("numLightsRange", numLightsRange);
        output.write("numShadowMapsRange", numShadowMapsRange);
        output.write("shadowMapSize", shadowMapSize);
    }

    if (output.version_greater_equal(1, 1, 8))
    {
        output.write("numDatabasePagerReadThreads", numDatabasePagerReadThreads);
        output.writeValue<uint32_t>("dataTransferHint", dataTransferHint);
    }

    if (output.version_greater_equal(1, 1, 11))
    {
        output.write("viewportStateHint", viewportStateHint);
    }
}
