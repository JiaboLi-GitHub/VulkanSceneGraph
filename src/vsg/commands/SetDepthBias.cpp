/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/SetDepthBias.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// 构造函数：创建设置深度偏移命令（默认）
// 设置深度偏移命令用于调整深度值，常用于阴影贴图和避免Z-fighting
SetDepthBias::SetDepthBias()
{
}

// 构造函数：使用深度偏移参数创建设置深度偏移命令
// in_depthBiasConstantFactor: 深度偏移常数因子
// in_depthBiasClamp: 深度偏移最大值（0表示不限制）
// in_depthBiasSlopeFactor: 深度偏移斜率因子
SetDepthBias::SetDepthBias(float in_depthBiasConstantFactor, float in_depthBiasClamp, float in_depthBiasSlopeFactor) :
    depthBiasConstantFactor(in_depthBiasConstantFactor),
    depthBiasClamp(in_depthBiasClamp),
    depthBiasSlopeFactor(in_depthBiasSlopeFactor)
{
}

// 记录设置深度偏移命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdSetDepthBias命令，设置深度偏移状态
void SetDepthBias::record(CommandBuffer& commandBuffer) const
{
    vkCmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}
