/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/FindAnimations.h>
#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>

using namespace vsg;

// 应用访问者到Object对象
// 遍历对象树，查找所有动画和动画组
// object: 要遍历的对象
void FindAnimations::apply(Object& object)
{
    object.traverse(*this);
}

// 应用访问者到Animation对象
// 将找到的动画添加到动画列表
// animation: 找到的动画对象
void FindAnimations::apply(Animation& animation)
{
    animations.emplace_back(&animation);
}

// 应用访问者到AnimationGroup对象
// 将找到的动画组添加到动画组列表，并继续遍历
// node: 找到的动画组节点
void FindAnimations::apply(AnimationGroup& node)
{
    animationGroups.emplace_back(&node);
    node.traverse(*this);
}
