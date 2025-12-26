/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Logger.h>
#include <vsg/vk/DescriptorPools.h>

#include <iostream>

using namespace vsg;

// 构造函数：创建描述符池集合对象
// in_device: 设备对象
// 描述符池集合管理多个描述符池，用于分配描述符集
DescriptorPools::DescriptorPools(ref_ptr<Device> in_device) :
    device(in_device)
{
}

// 析构函数：销毁描述符池集合对象
DescriptorPools::~DescriptorPools()
{
}

// 获取要使用的描述符池大小
// maxSets: 输入输出参数，最大描述符集数量
// descriptorPoolSizes: 输入输出参数，描述符池大小列表
// 根据预留需求和缩放因子计算要使用的描述符池大小
void DescriptorPools::getDescriptorPoolSizesToUse(uint32_t& maxSets, DescriptorPoolSizes& descriptorPoolSizes)
{
    if (reserve_count > 0)
    {
        // 如果最小最大集数大于预留最大集数，按比例缩放描述符数量
        if (minimum_maxSets > reserve_maxSets)
        {
            for (auto& dps : reserve_descriptorPoolSizes)
            {
                dps.descriptorCount = static_cast<uint32_t>(std::ceil(static_cast<double>(dps.descriptorCount) * static_cast<double>(minimum_maxSets) / static_cast<double>(reserve_maxSets)));
            }
            reserve_maxSets = minimum_maxSets;
        }

        if (minimum_maxSets > maxSets)
        {
            maxSets = minimum_maxSets;
        }

        // 合并预留的描述符池大小
        for (auto& [type, descriptorCount] : reserve_descriptorPoolSizes)
        {
            auto itr = descriptorPoolSizes.begin();
            for (; itr != descriptorPoolSizes.end(); ++itr)
            {
                if (itr->type == type)
                {
                    if (descriptorCount > itr->descriptorCount)
                        itr->descriptorCount = descriptorCount;
                    break;
                }
            }
            if (itr == descriptorPoolSizes.end())
            {
                descriptorPoolSizes.push_back(VkDescriptorPoolSize{type, descriptorCount});
            }
        }
    }

    // 计算最小最大集数（考虑缩放因子和最大限制）
    minimum_maxSets = std::min(maximum_maxSets, static_cast<uint32_t>(static_cast<double>(maxSets) * scale_maxSets));

    // 清除预留数据
    reserve_count = 0;
    reserve_maxSets = 0;
    reserve_descriptorPoolSizes.clear();
}

// 预留资源
// requirements: 资源需求
// 根据资源需求预留描述符池资源，如果现有池不足则创建新的描述符池
void DescriptorPools::reserve(const ResourceRequirements& requirements)
{
    auto maxSets = requirements.computeNumDescriptorSets();
    auto descriptorPoolSizes = requirements.computeDescriptorPoolSizes();

    // 更新跟踪所有预留调用的变量
    ++reserve_count;
    reserve_maxSets += maxSets;
    for (auto& dps : descriptorPoolSizes)
    {
        auto itr = std::find_if(reserve_descriptorPoolSizes.begin(), reserve_descriptorPoolSizes.end(), [&dps](const VkDescriptorPoolSize& value) { return value.type == dps.type; });
        if (itr != reserve_descriptorPoolSizes.end())
            itr->descriptorCount += dps.descriptorCount;
        else
            reserve_descriptorPoolSizes.push_back(dps);
    }

    // 计算总可用资源
    uint32_t available_maxSets = 0;
    DescriptorPoolSizes available_descriptorPoolSizes;
    for (auto& descriptorPool : descriptorPools)
    {
        descriptorPool->available(available_maxSets, available_descriptorPoolSizes);
    }

    // 计算额外需要的资源
    auto required_maxSets = maxSets;
    if (available_maxSets < required_maxSets)
        required_maxSets -= available_maxSets;
    else
        required_maxSets = 0;

    DescriptorPoolSizes required_descriptorPoolSizes;
    for (const auto& [type, descriptorCount] : descriptorPoolSizes)
    {
        uint32_t adjustedDescriptorCount = descriptorCount;
        // 从可用资源中减去已分配的部分
        for (auto itr = available_descriptorPoolSizes.begin(); itr != available_descriptorPoolSizes.end(); ++itr)
        {
            if (itr->type == type)
            {
                if (itr->descriptorCount < adjustedDescriptorCount)
                    adjustedDescriptorCount -= itr->descriptorCount;
                else
                {
                    adjustedDescriptorCount = 0;
                    break;
                }
            }
        }
        if (adjustedDescriptorCount > 0)
            required_descriptorPoolSizes.push_back(VkDescriptorPoolSize{type, adjustedDescriptorCount});
    }

    // 检查现有可用资源是否满足所有需求
    if (required_maxSets == 0 && required_descriptorPoolSizes.empty())
    {
        vsg::debug("DescriptorPools::reserve(const ResourceRequirements& requirements) enough resource in existing DescriptorPools");
        return;
    }

    // 描述符资源不足，分配新的描述符池
    getDescriptorPoolSizesToUse(required_maxSets, required_descriptorPoolSizes);
    descriptorPools.push_back(vsg::DescriptorPool::create(device, required_maxSets, required_descriptorPoolSizes));
}

// 分配描述符集
// descriptorSetLayout: 描述符集布局
// 返回: 描述符集实现对象
// 从描述符池集合中分配描述符集，如果所有池都满则创建新的描述符池
ref_ptr<DescriptorSet::Implementation> DescriptorPools::allocateDescriptorSet(DescriptorSetLayout* descriptorSetLayout)
{
    // 从后向前遍历描述符池（优先使用最新的池）
    for (auto itr = descriptorPools.rbegin(); itr != descriptorPools.rend(); ++itr)
    {
        auto dsi = (*itr)->allocateDescriptorSet(descriptorSetLayout);
        if (dsi) return dsi;
    }

    // 如果所有池都满，创建新的描述符池
    DescriptorPoolSizes descriptorPoolSizes;
    descriptorSetLayout->getDescriptorPoolSizes(descriptorPoolSizes);

    uint32_t maxSets = 1;
    getDescriptorPoolSizesToUse(maxSets, descriptorPoolSizes);

    auto descriptorPool = vsg::DescriptorPool::create(device, maxSets, descriptorPoolSizes);
    auto dsi = descriptorPool->allocateDescriptorSet(descriptorSetLayout);

    descriptorPools.push_back(descriptorPool);
    return dsi;
}

void DescriptorPools::report(std::ostream& out, indentation indent) const
{
    auto print = [&out, &indent](const std::string_view& name, uint32_t numSets, const DescriptorPoolSizes& descriptorPoolSizes) {
        out << indent << name << " {" << std::endl;
        indent += 4;
        out << indent << "numSets " << numSets << std::endl;
        out << indent << "descriptorPoolSizes " << descriptorPoolSizes.size() << " {" << std::endl;
        indent += 4;
        for (const auto& dps : descriptorPoolSizes)
        {
            out << indent << "VkDescriptorPoolSize{ " << dps.type << ", " << dps.descriptorCount << " }" << std::endl;
        }
        indent -= 4;
        out << indent << "}" << std::endl;

        indent -= 4;
        out << indent << "}" << std::endl;
    };

    out << "DescriptorPools::report(..) " << this << " {" << std::endl;
    indent += 4;

#if 1
    out << indent << "descriptorPools " << descriptorPools.size() << std::endl;
#else
    out << indent << "descriptorPools " << descriptorPools.size() << " {" << std::endl;
    indent += 4;
    for (auto& dp : descriptorPools)
    {
        dp->report(out, indent);
    }
    indent -= 4;
    out << indent << "}" << std::endl;
#endif

    uint32_t numSets = 0;
    DescriptorPoolSizes descriptorPoolSizes;

    allocated(numSets, descriptorPoolSizes);
    print("DescriptorPools::allocated()", numSets, descriptorPoolSizes);

    numSets = 0;
    descriptorPoolSizes.clear();
    used(numSets, descriptorPoolSizes);
    print("DescriptorPools::used()", numSets, descriptorPoolSizes);

    numSets = 0;
    descriptorPoolSizes.clear();
    available(numSets, descriptorPoolSizes);
    print("DescriptorPools::available()", numSets, descriptorPoolSizes);

    indent -= 4;
    out << indent << "}" << std::endl;
}

bool DescriptorPools::available(uint32_t& numSets, DescriptorPoolSizes& availableDescriptorPoolSizes) const
{
    bool result = false;
    for (auto& dp : descriptorPools)
    {
        result = dp->available(numSets, availableDescriptorPoolSizes) | result;
    }
    return result;
}

bool DescriptorPools::used(uint32_t& numSets, DescriptorPoolSizes& descriptorPoolSizes) const
{
    bool result = false;
    for (auto& dp : descriptorPools)
    {
        result = dp->used(numSets, descriptorPoolSizes) | result;
    }
    return result;
}

bool DescriptorPools::allocated(uint32_t& numSets, DescriptorPoolSizes& descriptorPoolSizes) const
{
    if (descriptorPools.empty()) return false;

    for (const auto& dp : descriptorPools)
    {
        numSets += dp->maxSets;
        for (auto& dps : dp->descriptorPoolSizes)
        {
            auto itr = std::find_if(descriptorPoolSizes.begin(), descriptorPoolSizes.end(), [&dps](const VkDescriptorPoolSize& value) { return value.type == dps.type; });
            if (itr != descriptorPoolSizes.end())
                itr->descriptorCount += dps.descriptorCount;
            else
                descriptorPoolSizes.push_back(dps);
        }
    }
    return true;
}
