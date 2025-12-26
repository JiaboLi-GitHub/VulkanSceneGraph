/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/commands/BindVertexBuffers.h>
#include <vsg/commands/Commands.h>
#include <vsg/commands/Draw.h>
#include <vsg/commands/DrawIndexed.h>
#include <vsg/io/Logger.h>
#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/CullNode.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/InstanceDraw.h>
#include <vsg/nodes/InstanceDrawIndexed.h>
#include <vsg/nodes/InstanceNode.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/VertexDraw.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/text/Text.h>
#include <vsg/text/TextGroup.h>
#include <vsg/utils/ComputeBounds.h>

using namespace vsg;

// ComputeBounds类的构造函数
// 创建边界计算器，用于计算场景图的包围盒
// intialArrayState: 初始数组状态（可选）
ComputeBounds::ComputeBounds(ref_ptr<ArrayState> intialArrayState)
{
    // 预留数组状态栈空间
    arrayStateStack.reserve(4);
    // 初始化数组状态栈
    arrayStateStack.emplace_back(intialArrayState ? intialArrayState : ArrayState::create());
}

// 应用访问者到Object对象
// 遍历对象树，计算包围盒
// object: 要遍历的对象
void ComputeBounds::apply(const vsg::Object& object)
{
    object.traverse(*this);
}

// 应用访问者到StateGroup对象
// 更新数组状态栈，然后继续遍历
// stategroup: 状态组对象
void ComputeBounds::apply(const StateGroup& stategroup)
{
    // 克隆或创建新的数组状态
    auto arrayState = stategroup.prototypeArrayState ? stategroup.prototypeArrayState->cloneArrayState(arrayStateStack.back()) : arrayStateStack.back()->cloneArrayState();

    // 应用所有状态命令到数组状态
    for (auto& statecommand : stategroup.stateCommands)
    {
        statecommand->accept(*arrayState);
    }

    // 将新数组状态压入栈
    arrayStateStack.emplace_back(arrayState);

    // 继续遍历子节点
    stategroup.traverse(*this);

    // 恢复数组状态栈
    arrayStateStack.pop_back();
}

// 应用访问者到Transform对象
// 将变换应用到矩阵栈，然后继续遍历
// transform: 变换对象
void ComputeBounds::apply(const Transform& transform)
{
    // 将变换应用到当前矩阵栈顶
    if (matrixStack.empty())
        matrixStack.push_back(transform.transform(dmat4{}));
    else
        matrixStack.push_back(transform.transform(matrixStack.back()));

    // 继续遍历子节点
    transform.traverse(*this);

    // 恢复矩阵栈
    matrixStack.pop_back();
}

// 应用访问者到MatrixTransform对象
// 将矩阵变换应用到矩阵栈，然后继续遍历
// transform: 矩阵变换对象
void ComputeBounds::apply(const MatrixTransform& transform)
{
    // 将矩阵变换应用到当前矩阵栈顶
    if (matrixStack.empty())
        matrixStack.push_back(transform.matrix);
    else
        matrixStack.push_back(matrixStack.back() * transform.matrix);

    // 继续遍历子节点
    transform.traverse(*this);

    // 恢复矩阵栈
    matrixStack.pop_back();
}

// 应用访问者到CullNode对象
// 如果使用节点边界且边界有效，直接添加边界；否则继续遍历
// cullNode: 剔除节点对象
void ComputeBounds::apply(const CullNode& cullNode)
{
    if (useNodeBounds && cullNode.bound.valid())
        add(cullNode.bound);
    else
        cullNode.traverse(*this);
}

// 应用访问者到CullGroup对象
// 如果使用节点边界且边界有效，直接添加边界；否则继续遍历
// cullGroup: 剔除组对象
void ComputeBounds::apply(const CullGroup& cullGroup)
{
    if (useNodeBounds && cullGroup.bound.valid())
        add(cullGroup.bound);
    else
        cullGroup.traverse(*this);
}

// 应用访问者到LOD对象
// 如果使用节点边界且边界有效，直接添加边界；否则继续遍历
// lod: LOD节点对象
void ComputeBounds::apply(const LOD& lod)
{
    if (useNodeBounds && lod.bound.valid())
        add(lod.bound);
    else
        lod.traverse(*this);
}

// 应用访问者到PagedLOD对象
// 如果使用节点边界且边界有效，直接添加边界；否则继续遍历
// plod: 分页LOD节点对象
void ComputeBounds::apply(const PagedLOD& plod)
{
    if (useNodeBounds && plod.bound.valid())
        add(plod.bound);
    else
        plod.traverse(*this);
}

// 应用访问者到Geometry对象
// 应用数组状态，然后处理索引和绘制命令
// geometry: 几何对象
void ComputeBounds::apply(const vsg::Geometry& geometry)
{
    auto& arrayState = *arrayStateStack.back();
    // 应用几何到数组状态
    arrayState.apply(geometry);

    // 处理索引
    if (geometry.indices) geometry.indices->accept(*this);

    // 处理绘制命令
    for (auto& command : geometry.commands)
    {
        command->accept(*this);
    }
}

// 应用访问者到VertexDraw对象
// 应用数组状态，然后计算顶点绘制的边界
// vid: 顶点绘制对象
void ComputeBounds::apply(const vsg::VertexDraw& vid)
{
    auto& arrayState = *arrayStateStack.back();
    // 应用顶点绘制到数组状态
    arrayState.apply(vid);

    // 计算绘制的边界
    applyDraw(vid.firstVertex, vid.vertexCount, vid.firstInstance, vid.instanceCount);
}

// 应用访问者到VertexIndexDraw对象
// 应用数组状态，处理索引，然后计算索引绘制的边界
// vid: 顶点索引绘制对象
void ComputeBounds::apply(const vsg::VertexIndexDraw& vid)
{
    auto& arrayState = *arrayStateStack.back();
    // 应用顶点索引绘制到数组状态
    arrayState.apply(vid);

    // 处理索引
    if (vid.indices) vid.indices->accept(*this);

    // 计算索引绘制的边界
    applyDrawIndexed(vid.firstIndex, vid.indexCount, vid.firstInstance, vid.vertexOffset, vid.instanceCount);
}

// 应用访问者到InstanceNode对象
// 应用数组状态，设置实例节点，然后继续遍历
// in: 实例节点对象
void ComputeBounds::apply(const vsg::InstanceNode& in)
{
    auto& arrayState = *arrayStateStack.back();

    // 应用实例节点到数组状态
    arrayState.apply(in);

    // 设置当前实例节点
    instanceNode = &in;

    // 继续遍历子节点
    in.traverse(*this);
}

// 应用访问者到InstanceDraw对象
// 应用数组状态，然后计算实例绘制的边界
// id: 实例绘制对象
void ComputeBounds::apply(const vsg::InstanceDraw& id)
{
    if (!instanceNode) return;

    auto& arrayState = *arrayStateStack.back();
    // 应用实例绘制到数组状态
    arrayState.apply(id);

    // 计算实例绘制的边界
    applyDraw(id.firstVertex, id.vertexCount, instanceNode->firstInstance, instanceNode->instanceCount);
}

// 应用访问者到InstanceDrawIndexed对象
// 应用数组状态，处理索引，然后计算实例索引绘制的边界
// idi: 实例索引绘制对象
void ComputeBounds::apply(const vsg::InstanceDrawIndexed& idi)
{
    if (!instanceNode) return;

    auto& arrayState = *arrayStateStack.back();
    // 应用实例索引绘制到数组状态
    arrayState.apply(idi);

    // 处理索引
    idi.indices->accept(*this);
    // 计算实例索引绘制的边界
    applyDrawIndexed(idi.firstIndex, idi.indexCount, instanceNode->firstInstance, idi.vertexOffset, instanceNode->instanceCount);
}

// 应用访问者到BindVertexBuffers对象
// 将顶点缓冲区绑定应用到数组状态
// bvb: 绑定顶点缓冲区对象
void ComputeBounds::apply(const vsg::BindVertexBuffers& bvb)
{
    arrayStateStack.back()->apply(bvb);
}

// 应用访问者到BindIndexBuffer对象
// 处理索引缓冲区
// bib: 绑定索引缓冲区对象
void ComputeBounds::apply(const BindIndexBuffer& bib)
{
    bib.indices->accept(*this);
}

// 应用访问者到StateCommand对象
// 将状态命令应用到数组状态
// statecommand: 状态命令对象
void ComputeBounds::apply(const vsg::StateCommand& statecommand)
{
    statecommand.accept(*arrayStateStack.back());
}

// 应用访问者到BufferInfo对象
// 处理缓冲区信息中的数据
// bufferInfo: 缓冲区信息对象
void ComputeBounds::apply(const BufferInfo& bufferInfo)
{
    if (bufferInfo.data) bufferInfo.data->accept(*this);
}

// 应用访问者到ushortArray对象
// 设置无符号短整型索引数组
// array: 无符号短整型数组对象
void ComputeBounds::apply(const ushortArray& array)
{
    ushort_indices = &array;
    uint_indices = nullptr;
}

// 应用访问者到uintArray对象
// 设置无符号整型索引数组
// array: 无符号整型数组对象
void ComputeBounds::apply(const uintArray& array)
{
    ushort_indices = nullptr;
    uint_indices = &array;
}

// 应用访问者到Draw对象
// 计算绘制的边界
// draw: 绘制命令对象
void ComputeBounds::apply(const Draw& draw)
{
    applyDraw(draw.firstVertex, draw.vertexCount, draw.firstInstance, draw.instanceCount);
}

// 应用访问者到DrawIndexed对象
// 计算索引绘制的边界
// drawIndexed: 索引绘制命令对象
void ComputeBounds::apply(const DrawIndexed& drawIndexed)
{
    applyDrawIndexed(drawIndexed.firstIndex, drawIndexed.indexCount, drawIndexed.firstInstance, drawIndexed.vertexOffset, drawIndexed.instanceCount);
};

// 计算顶点绘制的边界
// 遍历所有实例和顶点，应用变换矩阵，添加到边界
// firstVertex: 起始顶点索引
// vertexCount: 顶点数量
// firstInstance: 起始实例索引
// instanceCount: 实例数量
void ComputeBounds::applyDraw(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount)
{
    auto& arrayState = *arrayStateStack.back();
    // 计算最后一个实例索引
    uint32_t lastIndex = instanceCount > 1 ? (firstInstance + instanceCount) : firstInstance + 1;
    uint32_t endVertex = firstVertex + vertexCount;
    // 获取当前变换矩阵
    dmat4 matrix;
    if (!matrixStack.empty()) matrix = matrixStack.back();

    // 遍历所有实例
    for (uint32_t instanceIndex = firstInstance; instanceIndex < lastIndex; ++instanceIndex)
    {
        // 获取当前实例的顶点数组
        if (auto vertices = arrayState.vertexArray(instanceIndex))
        {
            // 遍历所有顶点，应用变换并添加到边界
            for (uint32_t i = firstVertex; i < endVertex; ++i)
            {
                bounds.add(matrix * dvec3(vertices->at(i)));
            }
        }
    }
}

// 计算索引绘制的边界
// 遍历所有实例和索引，应用变换矩阵，添加到边界
// firstIndex: 起始索引
// indexCount: 索引数量
// firstInstance: 起始实例索引
// vertexOffset: 顶点偏移量
// instanceCount: 实例数量
void ComputeBounds::applyDrawIndexed(uint32_t firstIndex, uint32_t indexCount, uint32_t firstInstance, uint32_t vertexOffset, uint32_t instanceCount)
{
    auto& arrayState = *arrayStateStack.back();
    // 计算最后一个实例索引
    uint32_t lastIndex = instanceCount > 1 ? (firstInstance + instanceCount) : firstInstance + 1;
    uint32_t endIndex = firstIndex + indexCount;
    // 获取当前变换矩阵
    dmat4 matrix;
    if (!matrixStack.empty()) matrix = matrixStack.back();

    // 处理无符号短整型索引
    if (ushort_indices)
    {
        // 遍历所有实例
        for (uint32_t instanceIndex = firstInstance; instanceIndex < lastIndex; ++instanceIndex)
        {
            // 获取当前实例的顶点数组
            if (auto vertices = arrayState.vertexArray(instanceIndex))
            {
                // 遍历所有索引，应用变换并添加到边界
                for (uint32_t i = firstIndex; i < endIndex; ++i)
                {
                    bounds.add(matrix * dvec3(vertices->at(ushort_indices->at(i) + vertexOffset)));
                }
            }
        }
    }
    // 处理无符号整型索引
    else if (uint_indices)
    {
        // 遍历所有实例
        for (uint32_t instanceIndex = firstInstance; instanceIndex < lastIndex; ++instanceIndex)
        {
            // 获取当前实例的顶点数组
            if (auto vertices = arrayState.vertexArray(instanceIndex))
            {
                // 遍历所有索引，应用变换并添加到边界
                for (uint32_t i = firstIndex; i < endIndex; ++i)
                {
                    bounds.add(matrix * dvec3(vertices->at(uint_indices->at(i) + vertexOffset)));
                }
            }
        }
    }
}

// 应用访问者到Text对象
// 处理文本对象的技术
// text: 文本对象
void ComputeBounds::apply(const Text& text)
{
    if (text.technique) text.technique->accept(*this);
}

// 应用访问者到TextGroup对象
// 处理文本组对象的技术
// textGroup: 文本组对象
void ComputeBounds::apply(const TextGroup& textGroup)
{
    if (textGroup.technique) textGroup.technique->accept(*this);
}

// 应用访问者到TextTechnique对象
// 获取文本技术的范围并添加到边界
// technique: 文本技术对象
void ComputeBounds::apply(const TextTechnique& technique)
{
    auto bb = technique.extents();
    if (bb.valid()) add(bb);
}

// 添加包围盒到边界
// 将包围盒的8个角点添加到边界中（应用当前变换矩阵）
// bb: 包围盒对象
void ComputeBounds::add(const dbox& bb)
{
    if (matrixStack.empty())
    {
        // 没有变换矩阵，直接添加包围盒
        bounds.add(bb);
    }
    else
    {
        // 有变换矩阵，变换包围盒的8个角点并添加
        auto& matrix = matrixStack.back();
        bounds.add(matrix * bb.min);
        bounds.add(matrix * dvec3(bb.max.x, bb.min.y, bb.min.z));
        bounds.add(matrix * dvec3(bb.max.x, bb.max.y, bb.min.z));
        bounds.add(matrix * dvec3(bb.min.x, bb.max.y, bb.min.z));
        bounds.add(matrix * dvec3(bb.min.x, bb.min.y, bb.max.z));
        bounds.add(matrix * dvec3(bb.max.x, bb.min.y, bb.max.z));
        bounds.add(matrix * bb.max);
        bounds.add(matrix * dvec3(bb.min.x, bb.max.y, bb.max.z));
    }
}

// 添加包围球到边界
// 将包围球的8个角点添加到边界中（应用当前变换矩阵）
// bs: 包围球对象
void ComputeBounds::add(const dsphere& bs)
{
    if (matrixStack.empty())
    {
        // 没有变换矩阵，直接添加包围球的边界框
        bounds.add(bs.center.x - bs.radius, bs.center.y - bs.radius, bs.center.z - bs.radius);
        bounds.add(bs.center.x + bs.radius, bs.center.y + bs.radius, bs.center.z + bs.radius);
    }
    else
    {
        // 有变换矩阵，变换包围球的8个角点并添加
        auto& matrix = matrixStack.back();
        bounds.add(matrix * dvec3(bs.center.x - bs.radius, bs.center.y - bs.radius, bs.center.z - bs.radius));
        bounds.add(matrix * dvec3(bs.center.x + bs.radius, bs.center.y - bs.radius, bs.center.z - bs.radius));
        bounds.add(matrix * dvec3(bs.center.x - bs.radius, bs.center.y + bs.radius, bs.center.z - bs.radius));
        bounds.add(matrix * dvec3(bs.center.x + bs.radius, bs.center.y + bs.radius, bs.center.z - bs.radius));
        bounds.add(matrix * dvec3(bs.center.x - bs.radius, bs.center.y - bs.radius, bs.center.z + bs.radius));
        bounds.add(matrix * dvec3(bs.center.x + bs.radius, bs.center.y - bs.radius, bs.center.z + bs.radius));
        bounds.add(matrix * dvec3(bs.center.x - bs.radius, bs.center.y + bs.radius, bs.center.z + bs.radius));
        bounds.add(matrix * dvec3(bs.center.x + bs.radius, bs.center.y + bs.radius, bs.center.z + bs.radius));
    }
}
