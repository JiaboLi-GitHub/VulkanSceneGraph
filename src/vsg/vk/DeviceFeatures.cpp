/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/vk/DeviceFeatures.h>

using namespace vsg;

// 构造函数：创建设备特性对象
// 设备特性对象管理Vulkan设备特性的启用和链式结构
DeviceFeatures::DeviceFeatures()
{
}

// 析构函数：销毁设备特性对象
// 清理所有特性并释放内存
DeviceFeatures::~DeviceFeatures()
{
    clear();
}

// 获取物理设备特性
// 返回: 物理设备特性引用
// 获取基础物理设备特性（VkPhysicalDeviceFeatures）
VkPhysicalDeviceFeatures& DeviceFeatures::get()
{
    return get<VkPhysicalDeviceFeatures2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2>().features;
}

// 清除所有特性
// 释放所有特性的内存并清空特性列表
void DeviceFeatures::clear()
{
    for (auto& feature : _features)
    {
        feature.second.second(feature.second.first);
    }

    _features.clear();
}

// 获取特性数据指针（用于pNext链）
// 返回: 特性链的头部指针，如果没有特性则返回nullptr
// 将特性链接成pNext链，返回链的头部指针（用于Vulkan结构体的pNext字段）
void* DeviceFeatures::data() const
{
    if (_features.empty()) return nullptr;

    // 将特性pNext指针链接在一起
    FeatureHeader* previous = nullptr;
    for (auto itr = _features.rbegin(); itr != _features.rend(); ++itr)
    {
        itr->second.first->pNext = previous;
        previous = itr->second.first;
    }

    // 返回链的头部
    return const_cast<void*>(reinterpret_cast<const void*>(_features.begin()->second.first));
}
