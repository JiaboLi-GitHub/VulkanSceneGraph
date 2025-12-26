/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/VSG.h>
#include <vsg/io/glsl.h>
#include <vsg/io/json.h>
#include <vsg/io/read.h>
#include <vsg/io/spirv.h>
#include <vsg/io/tile.h>
#include <vsg/io/txt.h>
#include <vsg/threading/OperationThreads.h>
#include <vsg/utils/FindDynamicObjects.h>
#include <vsg/utils/PropagateDynamicObjects.h>
#include <vsg/utils/SharedObjects.h>

using namespace vsg;

// 从文件读取对象
// filename: 文件名路径
// options: 选项对象
// 返回: 读取的对象，如果失败则返回空指针
// 根据文件扩展名选择合适的读取器，支持共享对象缓存和动态对象处理
ref_ptr<Object> vsg::read(const Path& filename, ref_ptr<const Options> options)
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "read", COLOR_READ);

    // Lambda函数：实际读取文件的函数
    auto read_file = [&]() -> ref_ptr<Object> {
        // 如果选项中有读取器/写入器列表，按顺序尝试
        if (options && !options->readerWriters.empty())
        {
            for (auto& readerWriter : options->readerWriters)
            {
                auto object = readerWriter->read(filename, options);
                if (object) return object;
            }
            return {};
        }

        // 根据文件扩展名选择读取器
        auto ext = vsg::lowerCaseFileExtension(filename);

        if (ext == ".vsga" || ext == ".vsgt" || ext == ".vsgb")
        {
            VSG rw;
            return rw.read(filename, options);
        }
        else if (ext == ".spv")
        {
            spirv rw;
            return rw.read(filename, options);
        }
        else if (ext == ".json")
        {
            json rw;
            return rw.read(filename, options);
        }
        else if (glsl::extensionSupported(ext))
        {
            glsl rw;
            return rw.read(filename, options);
        }
        else if (txt::extensionSupported(ext))
        {
            txt rw;
            return rw.read(filename, options);
        }
        else
        {
            // 没有加载文件的方法
            return {};
        }
    };

    // 如果使用共享对象缓存，通过缓存读取
    if (options && options->sharedObjects && options->sharedObjects->suitable(filename))
    {
        auto loadedObject = LoadedObject::create(filename, options);

        options->sharedObjects->share(loadedObject, [&](auto load) {
            load->object = read_file();

            // 如果对象需要动态处理，查找并传播动态对象
            if (load->object && options && options->findDynamicObjects && options->propagateDynamicObjects)
            {
                // 调用查找和传播访问者，收集所有需要克隆的动态对象

                std::scoped_lock<std::mutex> fdo_lock(options->findDynamicObjects->mutex);

                options->findDynamicObjects->dynamicObjects.clear();
                load->object->accept(*(options->findDynamicObjects));

                std::scoped_lock<std::mutex> pdo_lock(options->propagateDynamicObjects->mutex);
                options->propagateDynamicObjects->dynamicObjects.swap(options->findDynamicObjects->dynamicObjects);
                load->object->accept(*(options->propagateDynamicObjects));

                load->dynamicObjects.swap(options->propagateDynamicObjects->dynamicObjects);
            }
        });

        // 如果有动态对象，创建副本
        if (!loadedObject->dynamicObjects.empty())
        {
            vsg::CopyOp copyop;
            auto duplicate = copyop.duplicate = new vsg::Duplicate;
            for (auto& object : loadedObject->dynamicObjects)
            {
                duplicate->insert(object);
            }

            vsg::info("loaded filename = ", filename, ", object = ", loadedObject->object, ", dynamicObjects.size() = ", loadedObject->dynamicObjects.size());

            return copyop(loadedObject->object);
        }

        return loadedObject->object;
    }
    else
    {
        return read_file();
    }
}

// 从多个文件读取对象（支持多线程）
// filenames: 文件名路径列表
// options: 选项对象
// 返回: 文件名到对象的映射
// 如果提供了操作线程且文件数量大于1，则使用多线程并行读取
PathObjects vsg::read(const Paths& filenames, ref_ptr<const Options> options)
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "read", COLOR_READ);

    ref_ptr<OperationThreads> operationThreads;
    if (options) operationThreads = options->operationThreads;

    PathObjects entries;

    // 如果有多线程支持且文件数量大于1，使用多线程读取
    if (operationThreads && filenames.size() > 1)
    {
        // 设置条目容器供操作写入
        for (const auto& filename : filenames)
        {
            entries[filename] = nullptr;
        }

        // 读取操作类：在后台线程中读取文件
        struct ReadOperation : public Operation
        {
            ReadOperation(const Path& f, ref_ptr<const Options> opt, ref_ptr<Object>& obj, ref_ptr<Latch> l) :
                filename(f),
                options(opt),
                object(obj),
                latch(l) {}

            void run() override
            {
                object = vsg::read(filename, options);
                latch->count_down();
            }

            Path filename;
            ref_ptr<const Options> options;
            ref_ptr<Object>& object;
            ref_ptr<Latch> latch;
        };

        // 使用latch同步此线程与文件读取线程
        auto latch = Latch::create(static_cast<int>(filenames.size()));

        // 添加操作
        for (auto& [filename, object] : entries)
        {
            operationThreads->add(ref_ptr<Operation>(new ReadOperation(filename, options, object, latch)));
        }

        // 也使用此线程读取文件
        operationThreads->run();

        // 等待所有读取操作完成
        latch->wait();
    }
    else
    {
        // 单线程运行读取
        for (auto& filename : filenames)
        {
            if (filename)
            {
                entries[filename] = vsg::read(filename, options);
            }
            else
            {
                entries[filename] = nullptr;
            }
        }
    }

    return entries;
}

ref_ptr<Object> vsg::read(std::istream& fin, ref_ptr<const Options> options)
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "read", COLOR_READ);

    if (options && !options->readerWriters.empty())
    {
        for (auto& readerWriter : options->readerWriters)
        {
            auto object = readerWriter->read(fin, options);
            if (object) return object;
        }
    }

    return {};
}

ref_ptr<Object> vsg::read(const uint8_t* ptr, size_t size, ref_ptr<const Options> options)
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "read", COLOR_READ);

    if (options && !options->readerWriters.empty())
    {
        for (auto& readerWriter : options->readerWriters)
        {
            auto object = readerWriter->read(ptr, size, options);
            if (object) return object;
        }
    }

    return {};
}
