/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/CopyAndReleaseImage.h>
#include <vsg/core/compare.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/vk/Context.h>

using namespace vsg;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// DescriptorImage
//
// 构造函数：创建描述符图像对象（默认）
// 描述符图像用于将图像和采样器绑定到描述符集（组合图像采样器、采样图像、存储图像等）
// 默认类型为组合图像采样器
DescriptorImage::DescriptorImage() :
    Inherit(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
{
}

// 拷贝构造函数：从另一个描述符图像对象创建新的描述符图像对象
// rhs: 要拷贝的描述符图像对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝图像信息列表
DescriptorImage::DescriptorImage(const DescriptorImage& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    imageInfoList(copyop(rhs.imageInfoList))
{
}

// 构造函数：使用采样器和数据对象创建描述符图像对象
// sampler: 采样器对象
// data: 图像数据对象
// in_dstBinding: 目标绑定点索引
// in_dstArrayElement: 目标数组元素索引
// in_descriptorType: 描述符类型（组合图像采样器、采样图像等）
// 从采样器和数据创建ImageInfo
DescriptorImage::DescriptorImage(ref_ptr<Sampler> sampler, ref_ptr<Data> data, uint32_t in_dstBinding, uint32_t in_dstArrayElement, VkDescriptorType in_descriptorType) :
    Inherit(in_dstBinding, in_dstArrayElement, in_descriptorType)
{
    if (sampler && data)
    {
        imageInfoList.push_back(ImageInfo::create(sampler, data));
    }
}

// 构造函数：使用图像信息对象创建描述符图像对象
// imageInfo: 图像信息对象（包含采样器、图像视图和图像布局）
// in_dstBinding: 目标绑定点索引
// in_dstArrayElement: 目标数组元素索引
// in_descriptorType: 描述符类型
DescriptorImage::DescriptorImage(ref_ptr<ImageInfo> imageInfo, uint32_t in_dstBinding, uint32_t in_dstArrayElement, VkDescriptorType in_descriptorType) :
    Inherit(in_dstBinding, in_dstArrayElement, in_descriptorType)
{
    imageInfoList.push_back(imageInfo);
}

// 构造函数：使用图像信息列表创建描述符图像对象
// in_imageInfoList: 图像信息列表
// in_dstBinding: 目标绑定点索引
// in_dstArrayElement: 目标数组元素索引
// in_descriptorType: 描述符类型
DescriptorImage::DescriptorImage(const ImageInfoList& in_imageInfoList, uint32_t in_dstBinding, uint32_t in_dstArrayElement, VkDescriptorType in_descriptorType) :
    Inherit(in_dstBinding, in_dstArrayElement, in_descriptorType),
    imageInfoList(in_imageInfoList)
{
}

int DescriptorImage::compare(const Object& rhs_object) const
{
    int result = Descriptor::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    return compare_pointer_container(imageInfoList, rhs.imageInfoList);
}

void DescriptorImage::read(Input& input)
{
    imageInfoList.clear();

    Descriptor::read(input);

    imageInfoList.resize(input.readValue<uint32_t>("images"));
    for (auto& imageInfo : imageInfoList)
    {
        imageInfo = ImageInfo::create();

        ref_ptr<Data> data;
        input.readObject("sampler", imageInfo->sampler);
        input.readObject("image", data);

        auto image = Image::create(data);
        if (imageInfo->sampler) image->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        image->usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        imageInfo->imageView = ImageView::create(image);
        imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}

void DescriptorImage::write(Output& output) const
{
    Descriptor::write(output);

    output.writeValue<uint32_t>("images", imageInfoList.size());
    for (const auto& imageInfo : imageInfoList)
    {
        output.writeObject("sampler", imageInfo->sampler.get());

        ref_ptr<Data> data;
        if (imageInfo->imageView && imageInfo->imageView->image) data = imageInfo->imageView->image->data;

        output.writeObject("image", data.get());
    }
}

// 编译描述符图像
// context: 编译上下文对象
// 编译所有采样器和图像视图，如果需要则计算Mipmap级别，然后复制图像数据或分配给传输任务
void DescriptorImage::compile(Context& context)
{
    if (imageInfoList.empty()) return;

    auto transferTask = context.transferTask.get();

    // 编译所有图像信息
    for (auto& imageInfo : imageInfoList)
    {
        // 编译采样器（如果存在）
        if (imageInfo->sampler) imageInfo->sampler->compile(context);
        
        // 编译图像视图（如果存在）
        if (imageInfo->imageView)
        {
            // 如果Mipmap级别为0，计算所需的Mipmap级别
            if (imageInfo->imageView->image->mipLevels == 0)
            {
                imageInfo->computeNumMipMapLevels();
            }

            auto& imageView = *imageInfo->imageView;
            // 编译图像视图（这会编译图像并分配内存）
            imageView.compile(context);

            // 如果没有传输任务且图像数据已修改，直接复制图像数据
            if (!transferTask && imageView.image && imageView.image->syncModifiedCount(context.deviceID))
            {
                const auto& image = *imageView.image;
                context.copy(image.data, imageInfo, image.mipLevels);
            }
        }
    }

    // 如果有传输任务，将图像信息列表分配给传输任务
    if (transferTask) transferTask->assign(imageInfoList);
}

// 将描述符图像信息分配到Vulkan写入描述符集结构
// context: 编译上下文对象
// wds: 输出参数，用于填充Vulkan写入描述符集结构
// 从临时内存分配图像信息数组，填充所有图像信息（采样器句柄、图像视图句柄、图像布局）
void DescriptorImage::assignTo(Context& context, VkWriteDescriptorSet& wds) const
{
    Descriptor::assignTo(context, wds);

    // 从VSG转换为Vulkan格式
    auto pImageInfo = context.scratchMemory->allocate<VkDescriptorImageInfo>(imageInfoList.size());
    wds.descriptorCount = static_cast<uint32_t>(imageInfoList.size());
    wds.pImageInfo = pImageInfo;
    for (size_t i = 0; i < imageInfoList.size(); ++i)
    {
        auto& imageInfo = imageInfoList[i];

        VkDescriptorImageInfo& info = pImageInfo[i];
        // 设置采样器句柄（如果存在）
        if (imageInfo->sampler)
            info.sampler = imageInfo->sampler->vk(context.deviceID);
        else
            info.sampler = 0;

        // 设置图像视图句柄（如果存在）
        if (imageInfo->imageView)
            info.imageView = imageInfo->imageView->vk(context.deviceID);
        else
            info.imageView = 0;

        // 设置图像布局
        info.imageLayout = imageInfo->imageLayout;
    }
}

// 获取描述符数量
// 返回: 描述符数量（图像信息列表的大小）
uint32_t DescriptorImage::getNumDescriptors() const
{
    return static_cast<uint32_t>(imageInfoList.size());
}

VSG_DECLSPEC ref_ptr<DescriptorImage> vsg::createSamplerDescriptor(ref_ptr<Sampler> sampler, uint32_t dstBinding, uint32_t dstArrayElement)
{
    ref_ptr<ImageInfo> imageImageInfo = ImageInfo::create(sampler, nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return DescriptorImage::create(imageImageInfo, dstBinding, dstArrayElement, VK_DESCRIPTOR_TYPE_SAMPLER);
}

VSG_DECLSPEC ref_ptr<DescriptorImage> vsg::createCombinedImageSamplerDescriptor(ref_ptr<Sampler> sampler, ref_ptr<Data> image, uint32_t dstBinding, uint32_t dstArrayElement)
{
    ref_ptr<ImageInfo> imageImageInfo = ImageInfo::create(sampler, image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return DescriptorImage::create(imageImageInfo, dstBinding, dstArrayElement, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

VSG_DECLSPEC ref_ptr<DescriptorImage> vsg::createSampedImageDescriptor(ref_ptr<Data> image, uint32_t dstBinding, uint32_t dstArrayElement)
{
    ref_ptr<ImageInfo> imageImageInfo = ImageInfo::create(nullptr, image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return DescriptorImage::create(imageImageInfo, dstBinding, dstArrayElement, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
}
