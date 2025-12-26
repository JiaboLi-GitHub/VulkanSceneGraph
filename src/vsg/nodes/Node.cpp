/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Allocator.h>
#include <vsg/nodes/Node.h>

using namespace vsg;

// Node类的默认构造函数
// Node是场景图中所有节点的基类
Node::Node()
{
}

// Node类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
Node::Node(const Node& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop)
{
}

// Node类的析构函数
Node::~Node()
{
}

// 重载new运算符
// 使用VSG的自定义内存分配器，指定节点类型的亲和性
void* Node::operator new(std::size_t count)
{
    return vsg::allocate(count, vsg::ALLOCATOR_AFFINITY_NODES);
}

// 重载delete运算符
// 使用VSG的自定义内存释放器
void Node::operator delete(void* ptr)
{
    vsg::deallocate(ptr);
}
