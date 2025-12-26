/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/VSG.h>
#include <vsg/io/glsl.h>
#include <vsg/io/spirv.h>
#include <vsg/io/write.h>
#include <vsg/utils/SharedObjects.h>

using namespace vsg;

// 将对象写入文件
// object: 要写入的对象
// filename: 文件名路径
// options: 选项对象
// 返回: 如果成功写入则返回true
// 根据文件扩展名选择合适的写入器，支持共享对象缓存
bool vsg::write(ref_ptr<Object> object, const Path& filename, ref_ptr<const Options> options)
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "write", COLOR_WRITE);

    bool fileWritten = false;
    if (options)
    {
        // 如果文件已包含在共享对象中，不写入文件
        if (options->sharedObjects && options->sharedObjects->contains(filename, options)) return true;

        // 如果选项中有读取器/写入器列表，按顺序尝试
        if (!options->readerWriters.empty())
        {
            for (auto& readerWriter : options->readerWriters)
            {
                fileWritten = readerWriter->write(object, filename, options);
                if (fileWritten) break;
            }
        }
    }

    // 如果写入器列表中没有成功写入，回退到使用原生VSG格式
    if (!fileWritten)
    {
        // 如果扩展名兼容，回退到使用原生VSG
        auto ext = vsg::lowerCaseFileExtension(filename);
        if (ext == ".vsga" || ext == ".vsgt" || ext == ".vsgb")
        {
            VSG rw;
            fileWritten = rw.write(object, filename, options);
        }
        else if (ext == ".spv")
        {
            spirv rw;
            fileWritten = rw.write(object, filename, options);
        }
        else if (glsl::extensionSupported(ext))
        {
            glsl rw;
            fileWritten = rw.write(object, filename, options);
        }
    }
#if 0
    if (fileWritten && options && options->sharedObjects)
    {
        options->sharedObjects->add(object, filename, options);
    }
#endif

    return fileWritten;
}
