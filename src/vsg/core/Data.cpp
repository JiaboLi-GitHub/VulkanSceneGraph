/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Allocator.h>
#include <vsg/core/Auxiliary.h>
#include <vsg/core/Data.h>
#include <vsg/core/MipmapLayout.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>

using namespace vsg;

// 比较两个Properties对象
// 使用内存比较来比较所有属性
int Data::Properties::compare(const Properties& rhs) const
{
    return compare_memory(*this, rhs);
}

// Properties的赋值运算符
// 复制所有属性，如果源对象的stride不为0则复制stride
Data::Properties& Data::Properties::operator=(const Properties& rhs)
{
    if (&rhs == this) return *this;

    format = rhs.format;
    // 只有当源对象的stride不为0时才复制stride
    if (rhs.stride != 0) stride = rhs.stride;
    mipLevels = rhs.mipLevels;
    blockWidth = rhs.blockWidth;
    blockHeight = rhs.blockHeight;
    blockDepth = rhs.blockDepth;
    origin = rhs.origin;
    imageViewType = rhs.imageViewType;
    dataVariance = rhs.dataVariance;
    allocatorType = rhs.allocatorType;

    return *this;
}

// 重载new运算符
// 使用VSG的自定义内存分配器，指定数据类型的亲和性
void* Data::operator new(std::size_t count)
{
    return vsg::allocate(count, vsg::ALLOCATOR_AFFINITY_DATA);
}

// 重载delete运算符
// 使用VSG的自定义内存释放器
void Data::operator delete(void* ptr)
{
    vsg::deallocate(ptr);
}

// 比较两个Data对象
// 首先比较基类，然后比较属性，最后比较数据内容
int Data::compare(const Object& rhs_object) const
{
    // 先比较基类Object
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    auto& rhs = static_cast<decltype(*this)>(rhs_object);

    // 比较属性
    if ((result = properties.compare(rhs.properties))) return result;

    // 较短的数据被认为更小
    if (dataSize() < rhs.dataSize()) return -1;
    if (dataSize() > rhs.dataSize()) return 1;

    // 如果两者都为空则相等
    if (dataSize() == 0) return 0;

    // 使用memcmp比较数据内容
    return std::memcmp(dataPointer(), rhs.dataPointer(), dataSize());
}

// 从输入流读取Data对象
// 根据版本号读取不同格式的属性数据
void Data::read(Input& input)
{
    Object::read(input);

    uint32_t format = 0;

    // 根据文件版本读取不同格式的属性
    if (input.version_greater_equal(0, 6, 1))
    {
        // 版本0.6.1及以上：使用"properties"作为标签
        input.read("properties", format, properties.stride, properties.mipLevels, properties.blockWidth, properties.blockHeight, properties.blockDepth, properties.origin, properties.imageViewType, properties.dataVariance);
    }
    else if (input.version_greater_equal(0, 5, 7))
    {
        // 版本0.5.7及以上：使用"Layout"作为标签，包含dataVariance
        input.read("Layout", format, properties.stride, properties.mipLevels, properties.blockWidth, properties.blockHeight, properties.blockDepth, properties.origin, properties.imageViewType, properties.dataVariance);
    }
    else
    {
        // 旧版本：使用"Layout"作为标签，不包含dataVariance，默认为STATIC_DATA
        input.read("Layout", format, properties.stride, properties.mipLevels, properties.blockWidth, properties.blockHeight, properties.blockDepth, properties.origin, properties.imageViewType);
        properties.dataVariance = STATIC_DATA;
    }

    // 将读取的格式值转换为VkFormat类型
    properties.format = VkFormat(format);
}

// 将Data对象写入输出流
// 根据版本号写入不同格式的属性数据
void Data::write(Output& output) const
{
    Object::write(output);

    uint32_t format = properties.format;
    // 根据文件版本写入不同格式的属性
    if (output.version_greater_equal(0, 6, 1))
    {
        // 版本0.6.1及以上：使用"properties"作为标签
        output.write("properties", format, properties.stride, properties.mipLevels, properties.blockWidth, properties.blockHeight, properties.blockDepth, properties.origin, properties.imageViewType, properties.dataVariance);
    }
    else if (output.version_greater_equal(0, 5, 7))
    {
        // 版本0.5.7及以上：使用"Layout"作为标签，包含dataVariance
        output.write("Layout", format, properties.stride, properties.mipLevels, properties.blockWidth, properties.blockHeight, properties.blockDepth, properties.origin, properties.imageViewType, properties.dataVariance);
    }
    else
    {
        // 旧版本：使用"Layout"作为标签，不包含dataVariance
        output.write("Layout", format, properties.stride, properties.mipLevels, properties.blockWidth, properties.blockHeight, properties.blockDepth, properties.origin, properties.imageViewType);
    }
}

// 复制Data对象的内部方法
// 复制属性和辅助对象中的用户对象映射
void Data::_copy(const Data& rhs)
{
    // 复制属性
    properties = rhs.properties;
    // 复制用户对象映射
    if (rhs.getAuxiliary())
    {
        getOrCreateAuxiliary()->userObjects = rhs.getAuxiliary()->userObjects;
    }
    else if (getAuxiliary())
    {
        // 如果源对象没有辅助对象，清空当前对象的用户对象映射
        getAuxiliary()->userObjects.clear();
    }
}

// 清除Data对象的内部方法
// 清除辅助对象中的所有数据
void Data::_clear()
{
    if (getAuxiliary()) getAuxiliary()->clear();
}

// 设置Mipmap布局
// 将MipmapLayout对象存储到用户对象映射中
void Data::setMipmapLayout(MipmapLayout* mipmapLayout)
{
    if (mipmapLayout)
        setObject("mipmapLayout", ref_ptr<MipmapLayout>(mipmapLayout));
    else if (getAuxiliary())
        removeObject("mipmapLayout");
}

// 获取Mipmap布局
// 从用户对象映射中获取MipmapLayout对象
const MipmapLayout* Data::getMipmapLayout() const
{
    return getObject<MipmapLayout>("mipmapLayout");
}

// 计算包括Mipmap在内的值数量
// 返回所有Mipmap级别的数据块总数
std::size_t Data::computeValueCountIncludingMipmaps() const
{
    std::size_t count = 0;

    // 如果存在Mipmap布局，使用布局信息计算
    if (auto mipmapLayout = getMipmapLayout())
    {
        for (const auto& mipmap : *mipmapLayout)
        {
            // 向上取整到块大小
            std::size_t w = (mipmap.x + properties.blockWidth - 1) / properties.blockWidth;
            std::size_t h = (mipmap.y + properties.blockHeight - 1) / properties.blockHeight;
            std::size_t d = (mipmap.z + properties.blockDepth - 1) / properties.blockDepth;

            count += w * h * d;
        }
    }
    else
    {
        // 如果没有Mipmap布局，根据Mipmap级别计算
        std::size_t x = width() * properties.blockWidth;
        std::size_t y = height() * properties.blockHeight;
        std::size_t z = depth() * properties.blockDepth;

        auto mipLevels = std::max(properties.mipLevels, uint8_t(1));
        for (uint8_t level = 0; level < mipLevels; ++level)
        {
            // 向上取整到块大小
            std::size_t w = (x + properties.blockWidth - 1) / properties.blockWidth;
            std::size_t h = (y + properties.blockHeight - 1) / properties.blockHeight;
            std::size_t d = (z + properties.blockDepth - 1) / properties.blockDepth;

            count += w * h * d;

            // 为下一级Mipmap减半尺寸
            if (x > 1) x = x / 2;
            if (y > 1) y = y / 2;
            if (z > 1) z = z / 2;
        }
    }

    return count;
}

// 获取像素范围
// 返回以像素为单位的宽度、高度和深度
std::tuple<uint32_t, uint32_t, uint32_t> Data::pixelExtents() const
{
    // 默认使用块大小计算
    uint32_t w = width() * properties.blockWidth;
    uint32_t h = height() * properties.blockHeight;
    uint32_t d = depth() * properties.blockDepth;

    // 如果存在Mipmap布局，使用第一个Mipmap级别的尺寸
    if (auto mipmapLayout = getMipmapLayout())
    {
        auto mipmap = mipmapLayout->at(0);
        w = mipmap.x;
        h = mipmap.y;
        d = mipmap.z;
    }

    return {w, h, d};
}
