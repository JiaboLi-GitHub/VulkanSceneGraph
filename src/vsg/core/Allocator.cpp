/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/IntrusiveAllocator.h>
#include <vsg/io/FileSystem.h>
#include <vsg/io/Logger.h>

#include <algorithm>
#include <cstddef>
#include <iostream>

using namespace vsg;

// 获取Allocator单例实例
// 使用IntrusiveAllocator作为默认分配器
std::unique_ptr<Allocator>& Allocator::instance()
{
    static std::unique_ptr<Allocator> s_allocator(new IntrusiveAllocator());
    return s_allocator;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// vsg::allocate和vsg::deallocate便利函数，映射到使用Allocator单例
//

// 分配内存
// 使用Allocator单例分配指定大小和亲和性的内存
void* vsg::allocate(std::size_t size, AllocatorAffinity allocatorAffinity)
{
    return Allocator::instance()->allocate(size, allocatorAffinity);
}

// 释放内存
// 使用Allocator单例释放指定大小的内存
void vsg::deallocate(void* ptr, std::size_t size)
{
    Allocator::instance()->deallocate(ptr, size);
}
