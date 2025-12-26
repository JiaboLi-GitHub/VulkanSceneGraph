/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/commands/BindVertexBuffers.h>
#include <vsg/maths/quat.h>
#include <vsg/maths/sample.h>
#include <vsg/maths/transform.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/InstanceDraw.h>
#include <vsg/nodes/InstanceDrawIndexed.h>
#include <vsg/nodes/InstanceNode.h>
#include <vsg/nodes/VertexDraw.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/state/ArrayState.h>
#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/state/GraphicsPipeline.h>
#include <vsg/state/InputAssemblyState.h>
#include <vsg/state/VertexInputState.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ArrayState - 数组状态基类，用于管理顶点数组和属性信息
//
// 拷贝构造函数：从另一个数组状态对象创建新的数组状态对象
// rhs: 要拷贝的数组状态对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝局部到世界变换栈、世界到局部变换栈、拓扑、顶点属性位置、顶点属性详情、顶点数组、代理顶点数组和数组列表
ArrayState::ArrayState(const ArrayState& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    localToWorldStack(rhs.localToWorldStack),
    worldToLocalStack(rhs.worldToLocalStack),
    topology(rhs.topology),
    vertex_attribute_location(rhs.vertex_attribute_location),
    vertexAttribute(rhs.vertexAttribute),
    vertices(rhs.vertices),
    proxy_vertices(rhs.proxy_vertices),
    arrays(rhs.arrays)
{
}

// 获取顶点数组（默认实现，直接返回顶点数组）
// instanceIndex: 实例索引（未使用）
// 返回: 顶点数组的常量引用
ref_ptr<const vec3Array> ArrayState::vertexArray(uint32_t /*instanceIndex*/)
{
    return vertices;
}

// 应用绑定图形管线
// bpg: 绑定图形管线对象
// 遍历管线状态并应用它们（用于提取顶点输入状态等信息）
void ArrayState::apply(const vsg::BindGraphicsPipeline& bpg)
{
    for (auto& pipelineState : bpg.pipeline->pipelineStates)
    {
        pipelineState->accept(*this);
    }
}

// 从顶点输入状态获取属性详情
// vas: 顶点输入状态对象
// location: 属性位置索引
// attributeDetails: 输出参数，用于存储属性详情
// 返回: 如果找到属性则返回true，否则返回false
// 在顶点输入状态中查找指定位置的属性，并提取其绑定、格式、偏移、步长和输入速率
bool ArrayState::getAttributeDetails(const VertexInputState& vas, uint32_t location, AttributeDetails& attributeDetails)
{
    for (const auto& attribute : vas.vertexAttributeDescriptions)
    {
        if (attribute.location == location)
        {
            for (const auto& binding : vas.vertexBindingDescriptions)
            {
                if (attribute.binding == binding.binding)
                {
                    attributeDetails.binding = attribute.binding;
                    attributeDetails.format = attribute.format;
                    attributeDetails.offset = attribute.offset;
                    attributeDetails.stride = binding.stride;
                    attributeDetails.inputRate = binding.inputRate;
                    return true;
                }
            }
        }
    }
    return false;
}

void ArrayState::apply(const VertexInputState& vas)
{
    getAttributeDetails(vas, vertex_attribute_location, vertexAttribute);
}

void ArrayState::apply(const InputAssemblyState& ias)
{
    topology = ias.topology;
}

void ArrayState::apply(const vsg::Geometry& geometry)
{
    applyArrays(geometry.firstBinding, geometry.arrays);
}

void ArrayState::apply(const vsg::VertexDraw& vid)
{
    applyArrays(vid.firstBinding, vid.arrays);
}

void ArrayState::apply(const vsg::VertexIndexDraw& vid)
{
    applyArrays(vid.firstBinding, vid.arrays);
}

void ArrayState::apply(const vsg::InstanceNode& id)
{
    // bindings set by Phong/PBR ShaderSets.
    if (id.colors) applyArray(6, id.colors);
    if (id.translations) applyArray(7, id.translations);
    if (id.rotations) applyArray(8, id.rotations);
    if (id.scales) applyArray(9, id.scales);
}

void ArrayState::apply(const vsg::InstanceDraw& id)
{
    applyArrays(id.firstBinding, id.arrays);
}

void ArrayState::apply(const vsg::InstanceDrawIndexed& id)
{
    applyArrays(id.firstBinding, id.arrays);
}

void ArrayState::apply(const vsg::BindVertexBuffers& bvb)
{
    applyArrays(bvb.firstBinding, bvb.arrays);
}

void ArrayState::applyArray(uint32_t binding, const ref_ptr<BufferInfo>& in_array)
{
    if (in_array && in_array->data) applyArray(binding, in_array->data);
}

void ArrayState::applyArray(uint32_t binding, const ref_ptr<Data>& in_array)
{
    if (arrays.size() <= binding) arrays.resize(binding + 1);
    arrays[binding] = in_array;

    // if the required vertexAttribute is within the new arrays apply the appropriate array to set up the vertices array
    if ((vertexAttribute.binding == binding) && arrays[vertexAttribute.binding])
    {
        arrays[vertexAttribute.binding]->accept(*this);
    }
}

void ArrayState::applyArrays(uint32_t firstBinding, const BufferInfoList& in_arrays)
{
    if (arrays.size() < (in_arrays.size() + firstBinding)) arrays.resize(in_arrays.size() + firstBinding);
    for (size_t i = 0; i < in_arrays.size(); ++i)
    {
        arrays[firstBinding + i] = in_arrays[i]->data;
    }

    // if the required vertexAttribute is within the new arrays apply the appropriate array to set up the vertices array
    if ((vertexAttribute.binding >= firstBinding) && ((vertexAttribute.binding - firstBinding) < arrays.size()) && arrays[vertexAttribute.binding])
    {
        arrays[vertexAttribute.binding]->accept(*this);
    }
}

void ArrayState::applyArrays(uint32_t firstBinding, const DataList& in_arrays)
{
    if (arrays.size() < (in_arrays.size() + firstBinding)) arrays.resize(in_arrays.size() + firstBinding);
    std::copy(in_arrays.begin(), in_arrays.end(), arrays.begin() + firstBinding);

    // if the required vertexAttribute is within the new arrays apply the appropriate array to set up the vertices array
    if ((vertexAttribute.binding >= firstBinding) && ((vertexAttribute.binding - firstBinding) < arrays.size()) && arrays[vertexAttribute.binding])
    {
        arrays[vertexAttribute.binding]->accept(*this);
    }
}

void ArrayState::apply(const vsg::BufferInfo& bufferInfo)
{
    if (bufferInfo.data) bufferInfo.data->accept(*this);
}

void ArrayState::apply(const vsg::vec3Array& array)
{
    vertices = &array;
}

// 应用数据数组（通用数据）
// array: 数据数组对象
// 如果数组未匹配到vec3Array，则使用代理数组来适配它（仅当格式为R32G32B32_SFLOAT且步长大于0时）
void ArrayState::apply(const vsg::Data& array)
{
    // 数组未匹配到vec3Array，使用代理数组来适配它
    if (vertexAttribute.stride > 0 && (vertexAttribute.format == VK_FORMAT_R32G32B32_SFLOAT))
    {
        if (!proxy_vertices) proxy_vertices = vsg::vec3Array::create();

        // 计算顶点数量并分配代理顶点数组
        uint32_t numVertices = static_cast<uint32_t>(arrays[vertexAttribute.binding]->dataSize()) / vertexAttribute.stride;
        proxy_vertices->assign(arrays[vertexAttribute.binding], vertexAttribute.offset, vertexAttribute.stride, numVertices, array.properties);

        vertices = proxy_vertices;
    }
    else
    {
        vertices = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// NullArrayState - 空数组状态，不存储顶点数组
//
// 构造函数：创建空数组状态对象（默认）
// 空数组状态用于不需要顶点数组的场景（如计算着色器）
NullArrayState::NullArrayState() :
    Inherit()
{
}

// 构造函数：从数组状态对象创建空数组状态对象
// as: 源数组状态对象
// 清空顶点数组
NullArrayState::NullArrayState(const ArrayState& as) :
    Inherit(as)
{
    vertices = {};
}

// 克隆数组状态
// 返回: 新的空数组状态对象
ref_ptr<ArrayState> NullArrayState::cloneArrayState()
{
    return NullArrayState::create(*this);
}

// 克隆指定的数组状态
// arrayState: 要克隆的数组状态对象
// 返回: 新的空数组状态对象
ref_ptr<ArrayState> NullArrayState::cloneArrayState(ref_ptr<ArrayState> arrayState)
{
    return NullArrayState::create(*arrayState);
}

// 应用vec3数组（清空顶点数组）
// 不存储顶点数组
void NullArrayState::apply(const vsg::vec3Array&)
{
    vertices = {};
}

// 应用数据数组（清空顶点数组）
// 不存储顶点数组
void NullArrayState::apply(const vsg::Data&)
{
    vertices = {};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TranslationArrayState - 平移数组状态，支持实例平移变换
//
// 构造函数：创建平移数组状态对象（默认）
// 平移数组状态用于支持实例化绘制中的平移变换
TranslationArrayState::TranslationArrayState()
{
}

// 拷贝构造函数：从另一个平移数组状态对象创建新的平移数组状态对象
// rhs: 要拷贝的平移数组状态对象
// 拷贝平移属性位置和平移属性详情
TranslationArrayState::TranslationArrayState(const TranslationArrayState& rhs) :
    Inherit(rhs),
    translation_attribute_location(rhs.translation_attribute_location),
    translationAttribute(rhs.translationAttribute)
{
}

// 构造函数：从数组状态对象创建平移数组状态对象
// rhs: 源数组状态对象
TranslationArrayState::TranslationArrayState(const ArrayState& rhs) :
    Inherit(rhs)
{
}

// 克隆数组状态
// 返回: 新的平移数组状态对象
ref_ptr<ArrayState> TranslationArrayState::cloneArrayState()
{
    return TranslationArrayState::create(*this);
}

// 克隆指定的数组状态
// arrayState: 要克隆的数组状态对象
// 返回: 新的平移数组状态对象
ref_ptr<ArrayState> TranslationArrayState::cloneArrayState(ref_ptr<ArrayState> arrayState)
{
    return TranslationArrayState::create(*arrayState);
}

// 应用顶点输入状态
// vas: 顶点输入状态对象
// 提取顶点属性和平移属性的详情
void TranslationArrayState::apply(const VertexInputState& vas)
{
    getAttributeDetails(vas, vertex_attribute_location, vertexAttribute);
    getAttributeDetails(vas, translation_attribute_location, translationAttribute);
}

// 获取顶点数组（应用实例平移变换）
// instanceIndex: 实例索引
// 返回: 应用平移变换后的顶点数组
// 根据实例索引获取平移值，并将平移应用到所有顶点
ref_ptr<const vec3Array> TranslationArrayState::vertexArray(uint32_t instanceIndex)
{
    auto translations = arrays[translationAttribute.binding].cast<vec3Array>();

    if (translations && (instanceIndex < translations->size()))
    {
        auto translation = translations->at(instanceIndex);
        auto new_vertices = vsg::vec3Array::create(static_cast<uint32_t>(vertices->size()));
        auto src_vertex_itr = vertices->begin();
        // 将平移应用到每个顶点
        for (auto& v : *new_vertices)
        {
            v = *(src_vertex_itr++) + translation;
        }
        return new_vertices;
    }

    return vertices;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TranslationRotationScaleArrayState
//
TranslationRotationScaleArrayState::TranslationRotationScaleArrayState()
{
}

TranslationRotationScaleArrayState::TranslationRotationScaleArrayState(const TranslationRotationScaleArrayState& rhs) :
    Inherit(rhs),
    translation_attribute_location(rhs.translation_attribute_location),
    translationAttribute(rhs.translationAttribute)
{
}

TranslationRotationScaleArrayState::TranslationRotationScaleArrayState(const ArrayState& rhs) :
    Inherit(rhs)
{
}

ref_ptr<ArrayState> TranslationRotationScaleArrayState::cloneArrayState()
{
    return TranslationRotationScaleArrayState::create(*this);
}

ref_ptr<ArrayState> TranslationRotationScaleArrayState::cloneArrayState(ref_ptr<ArrayState> arrayState)
{
    return TranslationRotationScaleArrayState::create(*arrayState);
}

void TranslationRotationScaleArrayState::apply(const VertexInputState& vas)
{
    getAttributeDetails(vas, vertex_attribute_location, vertexAttribute);
    getAttributeDetails(vas, translation_attribute_location, translationAttribute);
    getAttributeDetails(vas, rotation_attribute_location, rotationAttribute);
    getAttributeDetails(vas, scale_attribute_location, scaleAttribute);
}

// 获取顶点数组（应用实例平移、旋转和缩放变换）
// instanceIndex: 实例索引
// 返回: 应用变换后的顶点数组
// 根据实例索引获取平移、旋转和缩放值，并按顺序应用：缩放 -> 旋转 -> 平移
ref_ptr<const vec3Array> TranslationRotationScaleArrayState::vertexArray(uint32_t instanceIndex)
{
    auto translations = arrays[translationAttribute.binding].cast<vec3Array>();
    auto rotations = arrays[rotationAttribute.binding].cast<quatArray>();
    auto scales = arrays[scaleAttribute.binding].cast<vec3Array>();

    // vsg::info("TranslationRotationScaleArrayState::vertexArray(", instanceIndex, ") translations = ", translations, ", rotations = ", rotations, ", scales = ", scales);

    if (translations && (instanceIndex < translations->size()) &&
        rotations && (instanceIndex < rotations->size()) &&
        scales && (instanceIndex < scales->size()))
    {
        auto translation = translations->at(instanceIndex);
        auto rotation = rotations->at(instanceIndex);
        auto scale = scales->at(instanceIndex);
        auto new_vertices = vsg::vec3Array::create(static_cast<uint32_t>(vertices->size()));
        auto src_vertex_itr = vertices->begin();
        // 应用变换：v = translation + rotation * (scale * vertex)
        for (auto& v : *new_vertices)
        {
            v = translation + rotation * (scale * (*(src_vertex_itr++)));
        }
        return new_vertices;
    }

    return vertices;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DisplacementMapArrayState
//
DisplacementMapArrayState::DisplacementMapArrayState()
{
}

DisplacementMapArrayState::DisplacementMapArrayState(const DisplacementMapArrayState& rhs) :
    Inherit(rhs)
{
}

DisplacementMapArrayState::DisplacementMapArrayState(const ArrayState& rhs) :
    Inherit(rhs)
{
}

ref_ptr<ArrayState> DisplacementMapArrayState::cloneArrayState()
{
    return DisplacementMapArrayState::create(*this);
}

ref_ptr<ArrayState> DisplacementMapArrayState::cloneArrayState(ref_ptr<ArrayState> arrayState)
{
    return DisplacementMapArrayState::create(*arrayState);
}

void DisplacementMapArrayState::apply(const DescriptorImage& di)
{
    if (!di.imageInfoList.empty())
    {
        const auto& imageInfo = *di.imageInfoList[0];
        if (imageInfo.imageView && imageInfo.imageView->image)
        {
            displacementMap = imageInfo.imageView->image->data.cast<floatArray2D>();
            sampler = imageInfo.sampler;
        }
    }
}

void DisplacementMapArrayState::apply(const DescriptorSet& ds)
{
    for (auto& descriptor : ds.descriptors)
    {
        if (descriptor->dstBinding == dm_binding)
        {
            descriptor->accept(*this);
            break;
        }
    }
}

void DisplacementMapArrayState::apply(const BindDescriptorSet& bds)
{
    if (bds.firstSet == dm_set)
    {
        apply(*bds.descriptorSet);
    }
}

void DisplacementMapArrayState::apply(const BindDescriptorSets& bds)
{
    if (bds.firstSet <= dm_set && dm_set < (bds.firstSet + +static_cast<uint32_t>(bds.descriptorSets.size())))
    {
        apply(*bds.descriptorSets[dm_set - bds.firstSet]);
    }
}

void DisplacementMapArrayState::apply(const VertexInputState& vas)
{
    getAttributeDetails(vas, vertex_attribute_location, vertexAttribute);
    getAttributeDetails(vas, normal_attribute_location, normalAttribute);
    getAttributeDetails(vas, texcoord_attribute_location, texcoordAttribute);
}

// 获取顶点数组（应用位移贴图）
// instanceIndex: 实例索引（未使用）
// 返回: 应用位移贴图后的顶点数组
// 使用位移贴图沿法线方向偏移顶点位置，实现几何细节增强
ref_ptr<const vec3Array> DisplacementMapArrayState::vertexArray(uint32_t /*instanceIndex*/)
{
    if (displacementMap)
    {
        auto normals = arrays[normalAttribute.binding].cast<vec3Array>();
        auto texcoords = arrays[texcoordAttribute.binding].cast<vec2Array>();
        // 检查数组大小是否匹配
        if (texcoords->size() != vertices->size()) return {};
        if (normals->size() != vertices->size()) return {};

        auto new_vertices = vsg::vec3Array::create(static_cast<uint32_t>(vertices->size()));
        auto src_vertex_itr = vertices->begin();
        auto src_texcoord_itr = texcoords->begin();
        auto src_normal_itr = normals->begin();
        // vec2 tc_scale(static_cast<float>(displacementMap->width()) - 1.0f, static_cast<float>(displacementMap->height()) - 1.0f);

        // 如果没有分配采样器，使用默认构造的采样器
        if (!sampler) sampler = Sampler::create();

        // 对每个顶点应用位移：v = vertex + normal * displacement
        for (auto& v : *new_vertices)
        {
            const auto& tc = *(src_texcoord_itr++);
            const auto& n = *(src_normal_itr++);
            // 从位移贴图中采样位移值
            float d = sample(*sampler, *displacementMap, tc);
            v = *(src_vertex_itr++) + n * d;
        }
        return new_vertices;
    }

    return vertices;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TranslationAndDisplacementMapArrayState
//
TranslationAndDisplacementMapArrayState::TranslationAndDisplacementMapArrayState()
{
}

TranslationAndDisplacementMapArrayState::TranslationAndDisplacementMapArrayState(const TranslationAndDisplacementMapArrayState& rhs) :
    Inherit(rhs)
{
}

TranslationAndDisplacementMapArrayState::TranslationAndDisplacementMapArrayState(const ArrayState& rhs) :
    Inherit(rhs)
{
}

ref_ptr<ArrayState> TranslationAndDisplacementMapArrayState::cloneArrayState()
{
    return TranslationAndDisplacementMapArrayState::create(*this);
}

ref_ptr<ArrayState> TranslationAndDisplacementMapArrayState::cloneArrayState(ref_ptr<ArrayState> arrayState)
{
    return TranslationAndDisplacementMapArrayState::create(*arrayState);
}

void TranslationAndDisplacementMapArrayState::apply(const VertexInputState& vas)
{
    getAttributeDetails(vas, vertex_attribute_location, vertexAttribute);
    getAttributeDetails(vas, normal_attribute_location, normalAttribute);
    getAttributeDetails(vas, texcoord_attribute_location, texcoordAttribute);
    getAttributeDetails(vas, translation_attribute_location, translationAttribute);
}

// 获取顶点数组（应用实例平移和位移贴图）
// instanceIndex: 实例索引
// 返回: 应用平移和位移贴图后的顶点数组
// 结合实例平移和位移贴图来变换顶点
ref_ptr<const vec3Array> TranslationAndDisplacementMapArrayState::vertexArray(uint32_t instanceIndex)
{
    auto translations = arrays[translationAttribute.binding].cast<vec3Array>();

    vec3 translation;
    if (translations && (instanceIndex < translations->size()))
    {
        translation = translations->at(instanceIndex);
    }

    if (displacementMap)
    {
        auto normals = arrays[normalAttribute.binding].cast<vec3Array>();
        auto texcoords = arrays[texcoordAttribute.binding].cast<vec2Array>();
        // 检查数组大小是否匹配
        if (texcoords->size() != vertices->size()) return {};
        if (normals->size() != vertices->size()) return {};

        auto new_vertices = vsg::vec3Array::create(static_cast<uint32_t>(vertices->size()));
        auto src_vertex_itr = vertices->begin();
        auto src_teccoord_itr = texcoords->begin();
        auto src_normal_itr = normals->begin();
        //vec2 tc_scale(static_cast<float>(displacementMap->width()) - 1.0f, static_cast<float>(displacementMap->height()) - 1.0f);

        // 如果没有分配采样器，使用默认构造的采样器
        if (!sampler) sampler = Sampler::create();

        // 对每个顶点应用位移和平移：v = vertex + normal * displacement + translation
        for (auto& v : *new_vertices)
        {
            const auto& tc = *(src_teccoord_itr++);
            const auto& n = *(src_normal_itr++);
            // 从位移贴图中采样位移值
            float d = sample(*sampler, *displacementMap, tc);
            v = *(src_vertex_itr++) + n * d + translation;
        }
        return new_vertices;
    }

    return vertices;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BillboardArrayState
//
BillboardArrayState::BillboardArrayState()
{
}

BillboardArrayState::BillboardArrayState(const BillboardArrayState& rhs) :
    Inherit(rhs),
    translation_attribute_location(rhs.translation_attribute_location),
    translationAttribute(rhs.translationAttribute)
{
}

BillboardArrayState::BillboardArrayState(const ArrayState& rhs) :
    Inherit(rhs)
{
}

ref_ptr<ArrayState> BillboardArrayState::cloneArrayState()
{
    return BillboardArrayState::create(*this);
}

ref_ptr<ArrayState> BillboardArrayState::cloneArrayState(ref_ptr<ArrayState> arrayState)
{
    return BillboardArrayState::create(*arrayState);
}

void BillboardArrayState::apply(const VertexInputState& vas)
{
    getAttributeDetails(vas, vertex_attribute_location, vertexAttribute);
    getAttributeDetails(vas, translation_attribute_location, translationAttribute);
}

// 获取顶点数组（应用广告牌变换）
// instanceIndex: 实例索引
// 返回: 应用广告牌变换后的顶点数组
// 广告牌变换使几何体始终面向相机，常用于粒子系统、精灵等
ref_ptr<const vec3Array> BillboardArrayState::vertexArray(uint32_t instanceIndex)
{
    // 访问者模式：从数组中获取值（支持vec4Value和vec4Array）
    struct GetValue : public ConstVisitor
    {
        explicit GetValue(uint32_t i) :
            index(i) {}
        uint32_t index;
        vec4 value;

        void apply(const vec4Value& data) override { value = data.value(); }
        void apply(const vec4Array& data) override { value = data[index]; }
    } gv(instanceIndex);

    // 获取平移和距离缩放值（vec4的xyz为平移，w为自动距离缩放）
    arrays[translationAttribute.binding]->accept(gv);
    dvec3 translation(gv.value.xyz);
    double autoDistanceScale = gv.value.w;

    // 计算广告牌变换矩阵
    dmat4 billboard_to_local;
    if (!localToWorldStack.empty() && !worldToLocalStack.empty())
    {
        const auto& mv = localToWorldStack.back();
        const auto& inverse_mv = worldToLocalStack.back();
        // 将平移转换到眼空间
        auto center_eye = mv * translation;
        // 计算广告牌矩阵（使几何体面向相机）
        auto billboard_mv = computeBillboardMatrix(center_eye, autoDistanceScale);
        // 转换回局部空间
        billboard_to_local = inverse_mv * billboard_mv;
    }
    else
    {
        // 如果没有变换栈，只应用平移
        billboard_to_local = vsg::translate(translation);
    }

    // 应用广告牌变换到所有顶点
    auto new_vertices = vsg::vec3Array::create(static_cast<uint32_t>(vertices->size()));
    auto src_vertex_itr = vertices->begin();
    for (auto& v : *new_vertices)
    {
        const auto& sv = *(src_vertex_itr++);
        v = vec3(billboard_to_local * dvec3(sv));
    }
    return new_vertices;
}
