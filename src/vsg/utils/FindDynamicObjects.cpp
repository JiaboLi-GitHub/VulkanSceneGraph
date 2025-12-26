/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/AnimationGroup.h>
#include <vsg/animation/JointSampler.h>
#include <vsg/animation/MorphSampler.h>
#include <vsg/animation/TransformSampler.h>
#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/commands/BindVertexBuffers.h>
#include <vsg/commands/Draw.h>
#include <vsg/commands/DrawIndexed.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/Transform.h>
#include <vsg/nodes/VertexDraw.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/state/DescriptorBuffer.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/state/DescriptorSet.h>
#include <vsg/utils/FindDynamicObjects.h>
#include <vsg/vk/DescriptorPool.h>

using namespace vsg;

// 应用访问者到Object对象
// 遍历对象树，查找所有动态对象
// object: 要遍历的对象
void FindDynamicObjects::apply(const Object& object)
{
    object.traverse(*this);
}

// 应用访问者到Data对象
// 如果数据是动态的，标记它
// data: 要检查的数据对象
void FindDynamicObjects::apply(const Data& data)
{
    if (data.dynamic()) tag(&data);
}

// 应用访问者到AnimationGroup对象
// 标记动画组并继续遍历
// ag: 动画组对象
void FindDynamicObjects::apply(const AnimationGroup& ag)
{
    tag(&ag);
    ag.traverse(*this);
}

// 应用访问者到Animation对象
// 标记动画并继续遍历
// animation: 动画对象
void FindDynamicObjects::apply(const Animation& animation)
{
    tag(&animation);
    animation.traverse(*this);
}

// 应用访问者到AnimationSampler对象
// 标记动画采样器
// sampler: 动画采样器对象
void FindDynamicObjects::apply(const AnimationSampler& sampler)
{
    tag(&sampler);
}

// 应用访问者到TransformSampler对象
// 标记变换采样器及其关联对象
// sampler: 变换采样器对象
void FindDynamicObjects::apply(const TransformSampler& sampler)
{
    tag(&sampler);
    if (sampler.object)
    {
        tag(sampler.object);
        sampler.object->traverse(*this);
    }
}

// 应用访问者到MorphSampler对象
// 标记形变采样器及其关联对象
// sampler: 形变采样器对象
void FindDynamicObjects::apply(const MorphSampler& sampler)
{
    tag(&sampler);
    tag(sampler.object);
}

// 应用访问者到JointSampler对象
// 标记关节采样器及其关联对象
// sampler: 关节采样器对象
void FindDynamicObjects::apply(const JointSampler& sampler)
{
    tag(&sampler);
    tag(sampler.jointMatrices);
    tag(sampler.subgraph);
    if (sampler.subgraph) sampler.subgraph->traverse(*this);
}

// 应用访问者到BufferInfo对象
// 检查缓冲区信息中的数据
// info: 缓冲区信息对象
void FindDynamicObjects::apply(const BufferInfo& info)
{
    if (info.data) info.data->accept(*this);
}

// 应用访问者到Image对象
// 检查图像中的数据
// image: 图像对象
void FindDynamicObjects::apply(const Image& image)
{
    if (image.data) image.data->accept(*this);
}

// 应用访问者到ImageView对象
// 检查图像视图中的图像
// imageView: 图像视图对象
void FindDynamicObjects::apply(const ImageView& imageView)
{
    if (imageView.image) imageView.image->accept(*this);
}

// 应用访问者到ImageInfo对象
// 检查图像信息中的采样器和图像视图
// info: 图像信息对象
void FindDynamicObjects::apply(const ImageInfo& info)
{
    if (info.sampler) info.sampler->accept(*this);
    if (info.imageView) info.imageView->accept(*this);
}

// 应用访问者到DescriptorBuffer对象
// 检查描述符缓冲区中的所有缓冲区信息
// db: 描述符缓冲区对象
void FindDynamicObjects::apply(const DescriptorBuffer& db)
{
    for (auto info : db.bufferInfoList)
    {
        info->accept(*this);
    }
}

// 应用访问者到DescriptorImage对象
// 检查描述符图像中的所有图像信息
// di: 描述符图像对象
void FindDynamicObjects::apply(const DescriptorImage& di)
{
    for (auto info : di.imageInfoList)
    {
        info->accept(*this);
    }
}

// 应用访问者到BindIndexBuffer对象
// 检查索引缓冲区
// bib: 绑定索引缓冲区对象
void FindDynamicObjects::apply(const BindIndexBuffer& bib)
{
    if (bib.indices) bib.indices->accept(*this);
}

// 应用访问者到BindVertexBuffers对象
// 检查所有顶点缓冲区
// bvb: 绑定顶点缓冲区对象
void FindDynamicObjects::apply(const BindVertexBuffers& bvb)
{
    for (auto info : bvb.arrays)
    {
        if (info) info->accept(*this);
    }
}

// 应用访问者到VertexDraw对象
// 检查所有顶点数组
// vd: 顶点绘制对象
void FindDynamicObjects::apply(const VertexDraw& vd)
{
    for (auto info : vd.arrays)
    {
        if (info) info->accept(*this);
    }
}

// 应用访问者到VertexIndexDraw对象
// 检查索引和所有顶点数组
// vid: 顶点索引绘制对象
void FindDynamicObjects::apply(const VertexIndexDraw& vid)
{
    if (vid.indices) vid.indices->accept(*this);
    for (auto info : vid.arrays)
    {
        if (info) info->accept(*this);
    }
}

// 应用访问者到Geometry对象
// 检查索引、顶点数组和绘制命令
// geom: 几何对象
void FindDynamicObjects::apply(const Geometry& geom)
{
    if (geom.indices) geom.indices->accept(*this);
    for (auto info : geom.arrays)
    {
        if (info) info->accept(*this);
    }
    for (auto command : geom.commands)
    {
        if (command) command->accept(*this);
    }
}
