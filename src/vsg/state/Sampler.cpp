/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/state/Sampler.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建采样器对象（默认）
// 采样器用于定义纹理采样方式（过滤、寻址模式、各向异性等）
Sampler::Sampler()
{
}

// 析构函数：销毁采样器对象
Sampler::~Sampler()
{
}

// 比较两个采样器对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较采样器参数区域（从flags到unnormalizedCoordinates）
int Sampler::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_region(flags, unnormalizedCoordinates, rhs.flags);
}

// 从输入流读取采样器对象
// input: 输入流对象
// 读取所有采样器参数（标志、过滤模式、寻址模式、各向异性、LOD范围等）
void Sampler::read(Input& input)
{
    input.readValue<uint32_t>("flags", flags);
    input.readValue<uint32_t>("minFilter", minFilter);
    input.readValue<uint32_t>("magFilter", magFilter);
    input.readValue<uint32_t>("mipmapMode", mipmapMode);
    input.readValue<uint32_t>("addressModeU", addressModeU);
    input.readValue<uint32_t>("addressModeV", addressModeV);
    input.readValue<uint32_t>("addressModeW", addressModeW);
    input.read("mipLodBias", mipLodBias);
    input.readValue<uint32_t>("anisotropyEnable", anisotropyEnable);
    input.read("maxAnisotropy", maxAnisotropy);
    input.readValue<uint32_t>("compareEnable", compareEnable);
    input.readValue<uint32_t>("compareOp", compareOp);
    input.read("minLod", minLod);
    input.read("maxLod", maxLod);
    input.readValue<uint32_t>("borderColor", borderColor);
    input.readValue<uint32_t>("unnormalizedCoordinates", unnormalizedCoordinates);
}

// 将采样器对象写入输出流
// output: 输出流对象
// 写入所有采样器参数
void Sampler::write(Output& output) const
{
    output.writeValue<uint32_t>("flags", flags);
    output.writeValue<uint32_t>("minFilter", minFilter);
    output.writeValue<uint32_t>("magFilter", magFilter);
    output.writeValue<uint32_t>("mipmapMode", mipmapMode);
    output.writeValue<uint32_t>("addressModeU", addressModeU);
    output.writeValue<uint32_t>("addressModeV", addressModeV);
    output.writeValue<uint32_t>("addressModeW", addressModeW);
    output.write("mipLodBias", mipLodBias);
    output.writeValue<uint32_t>("anisotropyEnable", anisotropyEnable);
    output.write("maxAnisotropy", maxAnisotropy);
    output.writeValue<uint32_t>("compareEnable", compareEnable);
    output.writeValue<uint32_t>("compareOp", compareOp);
    output.write("minLod", minLod);
    output.write("maxLod", maxLod);
    output.writeValue<uint32_t>("borderColor", borderColor);
    output.writeValue<uint32_t>("unnormalizedCoordinates", unnormalizedCoordinates);
}

// 编译采样器
// context: 编译上下文对象
// 从临时内存分配采样器创建信息，填充所有参数，然后创建Vulkan采样器对象
void Sampler::compile(Context& context)
{
    if (_implementation[context.deviceID]) return;

    // 从临时内存分配采样器创建信息
    auto samplerInfo = context.scratchMemory->allocate<VkSamplerCreateInfo>();

    // 填充采样器创建信息
    samplerInfo->sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo->pNext = nullptr;
    samplerInfo->flags = flags;
    samplerInfo->magFilter = magFilter;
    samplerInfo->minFilter = minFilter;
    samplerInfo->mipmapMode = mipmapMode;
    samplerInfo->addressModeU = addressModeU;
    samplerInfo->addressModeV = addressModeV;
    samplerInfo->addressModeW = addressModeW;
    samplerInfo->mipLodBias = mipLodBias;
    samplerInfo->anisotropyEnable = anisotropyEnable;
    samplerInfo->maxAnisotropy = maxAnisotropy;
    samplerInfo->compareEnable = compareEnable;
    samplerInfo->compareOp = compareOp;
    samplerInfo->minLod = minLod;
    samplerInfo->maxLod = maxLod;
    samplerInfo->borderColor = borderColor;
    samplerInfo->unnormalizedCoordinates = unnormalizedCoordinates;

    // 创建Vulkan采样器实现
    _implementation[context.deviceID] = Implementation::create(context.device, *samplerInfo);
}

// 实现类构造函数：创建采样器实现对象
// device: Vulkan设备对象
// createSamplerInfo: 采样器创建信息
// 创建Vulkan采样器对象
Sampler::Implementation::Implementation(Device* device, const VkSamplerCreateInfo& createSamplerInfo) :
    _device(device)
{
    if (VkResult result = vkCreateSampler(*device, &createSamplerInfo, _device->getAllocationCallbacks(), &_sampler); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create VkSampler.", result};
    }
}

// 实现类析构函数：销毁采样器实现对象
// 释放Vulkan采样器对象
Sampler::Implementation::~Implementation()
{
    if (_sampler)
    {
        vkDestroySampler(*_device, _sampler, _device->getAllocationCallbacks());
    }
}
