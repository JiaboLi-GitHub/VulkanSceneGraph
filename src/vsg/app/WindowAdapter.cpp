/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/WindowAdapter.h>

using namespace vsg;

// WindowAdapter类的构造函数
// 创建窗口适配器，用于管理窗口表面和特性
// surface: Vulkan表面对象
// traits: 窗口特性对象
WindowAdapter::WindowAdapter(vsg::ref_ptr<vsg::Surface> surface, vsg::ref_ptr<vsg::WindowTraits> traits) :
    Inherit(traits)  // 继承窗口特性
{
    if (surface)
    {
        _surface = surface;  // 保存表面
        _instance = surface->getInstance();  // 获取Vulkan实例
    }
    if (traits)
    {
        // 从特性中获取窗口尺寸
        _extent2D.width = traits->width;
        _extent2D.height = traits->height;
    }
}

// 更新窗口尺寸
// 更新窗口的宽度和高度
// width: 新的窗口宽度
// height: 新的窗口高度
void WindowAdapter::updateExtents(uint32_t width, uint32_t height)
{
    _extent2D.width = width;
    _extent2D.height = height;
}

// 调整窗口大小
// 重新构建交换链以匹配新的窗口尺寸
void WindowAdapter::resize()
{
    buildSwapchain();  // 重建交换链
}
