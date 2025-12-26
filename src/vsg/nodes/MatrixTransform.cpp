/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/io/stream.h>
#include <vsg/nodes/MatrixTransform.h>

using namespace vsg;

// 构造函数：创建矩阵变换节点
// 矩阵变换节点使用4x4矩阵对子节点进行空间变换（平移、旋转、缩放等）
MatrixTransform::MatrixTransform()
{
}

// 拷贝构造函数：从另一个矩阵变换节点创建新的矩阵变换节点
// rhs: 要拷贝的矩阵变换节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝变换矩阵
MatrixTransform::MatrixTransform(const MatrixTransform& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    matrix(rhs.matrix)
{
}

// 构造函数：使用变换矩阵创建矩阵变换节点
// in_matrix: 4x4双精度变换矩阵（用于对子节点进行空间变换）
MatrixTransform::MatrixTransform(const dmat4& in_matrix) :
    matrix(in_matrix)
{
}

// 比较两个矩阵变换节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类Transform，然后比较变换矩阵
int MatrixTransform::compare(const Object& rhs_object) const
{
    int result = Transform::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_value(matrix, rhs.matrix);
}

// 从输入流读取矩阵变换节点对象
// input: 输入流对象
// 根据版本号选择不同的读取方式（新版本直接读取，旧版本通过基类读取）
void MatrixTransform::read(Input& input)
{
    if (input.version_greater_equal(1, 1, 2))
    {
        // 新版本格式：直接读取节点、矩阵、标志和子节点
        Node::read(input);
        input.read("matrix", matrix);
        input.read("subgraphRequiresLocalFrustum", subgraphRequiresLocalFrustum);
        input.readObjects("children", children);
    }
    else
    {
        // 旧版本格式：通过基类读取
        Transform::read(input);
        input.read("matrix", matrix);
        input.read("subgraphRequiresLocalFrustum", subgraphRequiresLocalFrustum);
    }
}

// 将矩阵变换节点对象写入输出流
// output: 输出流对象
// 根据版本号选择不同的写入方式（新版本直接写入，旧版本通过基类写入）
void MatrixTransform::write(Output& output) const
{
    if (output.version_greater_equal(1, 1, 2))
    {
        // 新版本格式：直接写入节点、矩阵、标志和子节点
        Node::write(output);
        output.write("matrix", matrix);
        output.write("subgraphRequiresLocalFrustum", subgraphRequiresLocalFrustum);
        output.writeObjects("children", children);
    }
    else
    {
        // 旧版本格式：通过基类写入
        Transform::write(output);
        output.write("matrix", matrix);
        output.write("subgraphRequiresLocalFrustum", subgraphRequiresLocalFrustum);
    }
}
