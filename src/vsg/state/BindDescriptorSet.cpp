/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/View.h>
#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/state/BindDescriptorSet.h>
#include <vsg/vk/Context.h>

using namespace vsg;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BindDescriptorSets
//
// 构造函数：创建绑定描述符集命令（默认）
// 绑定描述符集命令用于将多个描述符集绑定到渲染管线
// 槽位1：在绑定管线之后执行
BindDescriptorSets::BindDescriptorSets() :
    Inherit(1), // 槽位1
    pipelineBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS),
    firstSet(0)
{
}

// 拷贝构造函数：从另一个绑定描述符集命令对象创建新的绑定描述符集命令对象
// rhs: 要拷贝的绑定描述符集命令对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝管道绑定点、布局、首集索引、描述符集列表和动态偏移
BindDescriptorSets::BindDescriptorSets(const BindDescriptorSets& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    pipelineBindPoint(rhs.pipelineBindPoint),
    layout(copyop(rhs.layout)),
    firstSet(rhs.firstSet),
    descriptorSets(copyop(rhs.descriptorSets)),
    dynamicOffsets(rhs.dynamicOffsets)
{
}

// 比较两个绑定描述符集命令对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、管道绑定点、布局、首集索引和描述符集容器
int BindDescriptorSets::compare(const Object& rhs_object) const
{
    int result = StateCommand::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(pipelineBindPoint, rhs.pipelineBindPoint))) return result;
    if ((result = compare_pointer(layout, rhs.layout))) return result;
    if ((result = compare_value(firstSet, rhs.firstSet))) return result;
    return compare_pointer_container(descriptorSets, rhs.descriptorSets);
}

// 从输入流读取绑定描述符集命令对象
// input: 输入流对象
// 读取管道绑定点（版本0.5.4及以上）、布局、首集索引、描述符集列表和动态偏移（版本0.5.4及以上）
void BindDescriptorSets::read(Input& input)
{
    _vulkanData.clear();

    StateCommand::read(input);

    if (input.version_greater_equal(0, 5, 4))
    {
        input.readValue<uint32_t>("pipelineBindPoint", pipelineBindPoint);
    }

    input.readObject("layout", layout);
    input.read("firstSet", firstSet);
    input.readObjects("descriptorSets", descriptorSets);

    if (input.version_greater_equal(0, 5, 4))
    {
        input.readValues("dynamicOffsets", dynamicOffsets);
    }
}

// 将绑定描述符集命令对象写入输出流
// output: 输出流对象
// 写入管道绑定点、布局、首集索引、描述符集列表和动态偏移
void BindDescriptorSets::write(Output& output) const
{
    StateCommand::write(output);

    if (output.version_greater_equal(0, 5, 4))
    {
        output.writeValue<uint32_t>("pipelineBindPoint", pipelineBindPoint);
    }

    output.writeObject("layout", layout);
    output.write("firstSet", firstSet);
    output.writeObjects("descriptorSets", descriptorSets);

    if (output.version_greater_equal(0, 5, 4))
    {
        output.writeValues("dynamicOffsets", dynamicOffsets);
    }
}

// 编译绑定描述符集命令
// context: 编译上下文对象
// 编译布局和所有描述符集，然后获取Vulkan句柄
void BindDescriptorSets::compile(Context& context)
{
    auto& vkd = _vulkanData[context.deviceID];

    // 如果已编译，无需重新编译
    if (vkd._vkPipelineLayout != 0 && vkd._vkDescriptorSets.size() == descriptorSets.size()) return;

    // 编译布局并获取Vulkan句柄
    layout->compile(context);
    vkd._vkPipelineLayout = layout->vk(context.deviceID);

    // 编译所有描述符集并获取Vulkan句柄
    vkd._vkDescriptorSets.resize(descriptorSets.size());
    for (size_t i = 0; i < descriptorSets.size(); ++i)
    {
        descriptorSets[i]->compile(context);
        vkd._vkDescriptorSets[i] = descriptorSets[i]->vk(context.deviceID);
    }
}

// 记录绑定描述符集命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdBindDescriptorSets命令，绑定多个描述符集到渲染管线
void BindDescriptorSets::record(CommandBuffer& commandBuffer) const
{
    //info("BindDescriptorSets::record() ", dynamicOffsets.size(), ", ", dynamicOffsets.data());
    auto& vkd = _vulkanData[commandBuffer.deviceID];
    vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, vkd._vkPipelineLayout, firstSet,
                            static_cast<uint32_t>(vkd._vkDescriptorSets.size()), vkd._vkDescriptorSets.data(),
                            static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.data());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BindDescriptorSet
//
// 构造函数：创建绑定描述符集命令（默认，单个描述符集）
// 绑定描述符集命令用于将单个描述符集绑定到渲染管线
BindDescriptorSet::BindDescriptorSet() :
    pipelineBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS),
    firstSet(0)
{
}

// 拷贝构造函数：从另一个绑定描述符集命令对象创建新的绑定描述符集命令对象
// rhs: 要拷贝的绑定描述符集命令对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝管道绑定点、布局、首集索引、描述符集和动态偏移
BindDescriptorSet::BindDescriptorSet(const BindDescriptorSet& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    pipelineBindPoint(rhs.pipelineBindPoint),
    layout(copyop(rhs.layout)),
    firstSet(rhs.firstSet),
    descriptorSet(copyop(rhs.descriptorSet)),
    dynamicOffsets(rhs.dynamicOffsets)
{
}

// 比较两个绑定描述符集命令对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、管道绑定点、布局、首集索引和描述符集
int BindDescriptorSet::compare(const Object& rhs_object) const
{
    int result = StateCommand::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(pipelineBindPoint, rhs.pipelineBindPoint))) return result;
    if ((result = compare_pointer(layout, rhs.layout))) return result;
    if ((result = compare_value(firstSet, rhs.firstSet))) return result;
    return compare_pointer(descriptorSet, rhs.descriptorSet);
}

// 从输入流读取绑定描述符集命令对象
// input: 输入流对象
// 读取管道绑定点（版本0.5.4及以上）、布局、首集索引、描述符集和动态偏移（版本0.5.4及以上）
void BindDescriptorSet::read(Input& input)
{
    _vulkanData.clear();

    StateCommand::read(input);

    if (input.version_greater_equal(0, 5, 4))
    {
        input.readValue<uint32_t>("pipelineBindPoint", pipelineBindPoint);
    }

    input.readObject("layout", layout);
    input.read("firstSet", firstSet);
    input.readObject("descriptorSet", descriptorSet);

    if (input.version_greater_equal(0, 5, 4))
    {
        input.readValues("dynamicOffsets", dynamicOffsets);
    }
}

// 将绑定描述符集命令对象写入输出流
// output: 输出流对象
// 写入管道绑定点、布局、首集索引、描述符集和动态偏移
void BindDescriptorSet::write(Output& output) const
{
    StateCommand::write(output);

    if (output.version_greater_equal(0, 5, 4))
    {
        output.writeValue<uint32_t>("pipelineBindPoint", pipelineBindPoint);
    }

    output.writeObject("layout", layout);
    output.write("firstSet", firstSet);
    output.writeObject("descriptorSet", descriptorSet);

    if (output.version_greater_equal(0, 5, 4))
    {
        output.writeValues("dynamicOffsets", dynamicOffsets);
    }
}

// 编译绑定描述符集命令
// context: 编译上下文对象
// 编译布局和描述符集，然后获取Vulkan句柄
void BindDescriptorSet::compile(Context& context)
{
    auto& vkd = _vulkanData[context.deviceID];

    // 如果已编译，无需重新编译
    if (vkd._vkPipelineLayout != 0 && vkd._vkDescriptorSet != 0) return;

    // 编译布局和描述符集
    layout->compile(context);
    descriptorSet->compile(context);

    // 获取Vulkan句柄
    vkd._vkPipelineLayout = layout->vk(context.deviceID);
    vkd._vkDescriptorSet = descriptorSet->vk(context.deviceID);
}

// 记录绑定描述符集命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdBindDescriptorSets命令，绑定单个描述符集到渲染管线
void BindDescriptorSet::record(CommandBuffer& commandBuffer) const
{
    //info("BindDescriptorSet::record() ", dynamicOffsets.size(), ", ", dynamicOffsets.data());
    auto& vkd = _vulkanData[commandBuffer.deviceID];
    vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, vkd._vkPipelineLayout, firstSet,
                            1, &(vkd._vkDescriptorSet),
                            static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.data());
}
