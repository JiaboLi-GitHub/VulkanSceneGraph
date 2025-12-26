/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/Camera.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Camera - 相机类，用于定义视图和投影矩阵
//

// Camera类的默认构造函数
// 创建默认的透视投影和LookAt视图矩阵
Camera::Camera() :
    projectionMatrix(Perspective::create()),  // 默认透视投影矩阵
    viewMatrix(LookAt::create())  // 默认LookAt视图矩阵
{
}

// Camera类的构造函数
// 使用指定的投影矩阵、视图矩阵和视口状态创建相机
// in_projectionMatrix: 投影矩阵对象
// in_viewMatrix: 视图矩阵对象
// in_viewportState: 视口状态对象
Camera::Camera(ref_ptr<ProjectionMatrix> in_projectionMatrix, ref_ptr<ViewMatrix> in_viewMatrix, ref_ptr<ViewportState> in_viewportState) :
    projectionMatrix(in_projectionMatrix),  // 投影矩阵
    viewMatrix(in_viewMatrix),  // 视图矩阵
    viewportState(in_viewportState)  // 视口状态
{
}

// 从输入流读取相机数据
// 读取相机的名称、投影矩阵、视图矩阵和视口状态
// input: 输入流对象
void Camera::read(Input& input)
{
    Node::read(input);

    input.read("name", name);
    input.readObject("projectionMatrix", projectionMatrix);
    input.readObject("viewMatrix", viewMatrix);
    input.readObject("viewportState", viewportState);
}

// 将相机数据写入输出流
// 写入相机的名称、投影矩阵、视图矩阵和视口状态
// output: 输出流对象
void Camera::write(Output& output) const
{
    Node::write(output);

    output.write("name", name);
    output.writeObject("projectionMatrix", projectionMatrix);
    output.writeObject("viewMatrix", viewMatrix);
    output.writeObject("viewportState", viewportState);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FindCameras - 查找相机访问者，用于在场景图中查找所有相机
//

// 应用访问者到Object对象
// 遍历对象树，查找所有相机对象
// object: 要遍历的对象
void FindCameras::apply(Object& object)
{
    _objectPath.push_back(&object);  // 将对象添加到路径

    object.traverse(*this);  // 继续遍历

    _objectPath.pop_back();  // 从路径中移除对象
}

// 应用访问者到Camera对象
// 找到相机对象，将其添加到相机映射中
// camera: 相机对象
void FindCameras::apply(Camera& camera)
{
    _objectPath.push_back(&camera);  // 将相机添加到路径

    // 将对象路径转换为引用对象路径
    RefObjectPath convertedPath(_objectPath.begin(), _objectPath.end());

    // 将相机添加到映射中，使用路径作为键
    cameras[convertedPath] = &camera;

    _objectPath.pop_back();  // 从路径中移除相机
}
