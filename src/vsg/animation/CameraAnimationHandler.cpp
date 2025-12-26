/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/CameraAnimationHandler.h>
#include <vsg/animation/TransformSampler.h>
#include <vsg/app/Camera.h>
#include <vsg/io/Logger.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/ui/ApplicationEvent.h>
#include <vsg/ui/PrintEvents.h>

using namespace vsg;

// CameraAnimationHandler类的默认构造函数
// 创建相机动画处理器，用于录制和播放相机动画路径
CameraAnimationHandler::CameraAnimationHandler()
{
}

// CameraAnimationHandler类的构造函数
// 从文件加载相机动画或创建新的相机动画
// in_object: 要动画化的对象（通常是Camera）
// in_filename: 动画文件路径
// in_options: 选项对象
CameraAnimationHandler::CameraAnimationHandler(ref_ptr<Object> in_object, const Path& in_filename, ref_ptr<Options> in_options) :
    object(in_object),
    filename(in_filename),
    options(in_options)
{
    if (filename)
    {
        // 尝试从文件读取动画对象
        if (auto read_object = vsg::read(filename, options))
        {
            // 如果读取的是Animation对象，查找其中的CameraSampler
            if ((animation = read_object.cast<Animation>()))
            {
                for (auto sampler : animation->samplers)
                {
                    if (auto cs = sampler.cast<CameraSampler>())
                    {
                        cameraSampler = cs;
                        break;
                    }
                }
            }
            // 如果读取的是CameraSampler对象，创建Animation并添加采样器
            else if ((cameraSampler = read_object.cast<CameraSampler>()))
            {
                animation = Animation::create();
                animation->samplers.push_back(cameraSampler);
            }
            // 如果读取的是TransformSampler，转换为CameraSampler
            else if (auto ts = read_object.cast<TransformSampler>())
            {
                auto tkf = ts->keyframes;

                // 将TransformSampler转换为CameraSampler
                cameraSampler = CameraSampler::create();
                cameraSampler->name = ts->name;
                const auto& ckf = cameraSampler->keyframes = CameraKeyframes::create();

                // 复制位置和旋转关键帧
                ckf->positions = tkf->positions;
                ckf->rotations = tkf->rotations;

                animation = Animation::create();
                animation->samplers.push_back(cameraSampler);
            }
            // 如果读取的是TransformKeyframes，创建CameraSampler
            else if (auto keyframes = read_object.cast<TransformKeyframes>())
            {
                cameraSampler = CameraSampler::create();
                const auto& ckf = cameraSampler->keyframes = CameraKeyframes::create();

                // 复制位置和旋转关键帧
                ckf->positions = keyframes->positions;
                ckf->rotations = keyframes->rotations;

                animation = Animation::create();
                animation->samplers.push_back(cameraSampler);
            }
        }
        // 如果对象和相机采样器都存在，关联它们
        if (object && cameraSampler) cameraSampler->object = object;
    }
    else
    {
        // 如果没有指定文件名，使用默认文件名
        filename = "saved_animation.vsgt";
    }
}

// CameraAnimationHandler类的构造函数
// 使用现有的动画对象创建相机动画处理器
// in_object: 要动画化的对象（通常是Camera）
// in_animation: 现有的动画对象
// in_filename: 动画文件路径
// in_options: 选项对象
CameraAnimationHandler::CameraAnimationHandler(ref_ptr<Object> in_object, ref_ptr<Animation> in_animation, const Path& in_filename, ref_ptr<Options> in_options) :
    object(in_object),
    filename(in_filename),
    options(in_options),
    animation(in_animation)
{
    if (animation)
    {
        // 从动画中查找CameraSampler
        for (auto& sampler : animation->samplers)
        {
            if (auto ts = sampler.cast<CameraSampler>())
            {
                cameraSampler = ts;
                cameraSampler->object = object;
                break;
            }
        }
    }
}

// 应用相机动画处理器到Camera对象
// 在录制模式下，记录相机的当前位置和方向
// camera: 要处理的相机对象
void CameraAnimationHandler::apply(Camera& camera)
{
    info("CameraAnimationHandler::apply(Camera& camera) ", cameraSampler);

    if (cameraSampler)
    {
        // 获取或创建关键帧对象
        auto& keyframes = cameraSampler->keyframes;
        if (!keyframes) keyframes = CameraKeyframes::create();

        // 从相机的视图矩阵中提取位置和方向
        dvec3 position, scale;
        dquat orientation;
        auto matrix = camera.viewMatrix->inverse();
        if (decompose(matrix, position, orientation, scale))
        {
            // 添加关键帧（相对于录制开始时间）
            keyframes->add(simulationTime - startTime, position, orientation);
        }
    }
}

// 应用相机动画处理器到MatrixTransform对象
// 在录制模式下，记录变换矩阵的位置和方向
// transform: 要处理的矩阵变换对象
void CameraAnimationHandler::apply(MatrixTransform& transform)
{
    if (cameraSampler)
    {
        // 获取或创建关键帧对象
        auto& keyframes = cameraSampler->keyframes;
        if (!keyframes) keyframes = CameraKeyframes::create();

        // 从变换矩阵中提取位置和方向
        dvec3 position, scale;
        dquat orientation;
        if (decompose(transform.matrix, position, orientation, scale))
        {
            // 添加关键帧（相对于录制开始时间）
            keyframes->add(simulationTime - startTime, position, orientation);
        }
    }
}

// 播放动画
// 启动相机动画的播放
void CameraAnimationHandler::play()
{
    // 如果已经在播放，直接返回
    if (playing) return;

    // 启动动画
    playing = animation->start(simulationTime);
    if (playing) info("Starting playback.");
}

// 开始录制
// 开始录制相机动画路径
void CameraAnimationHandler::record()
{
    // 如果已经在录制，直接返回
    if (recording) return;

    info("Starting recording.");
    // 记录录制开始时间
    startTime = simulationTime;
    recording = true;

    // 如果没有动画对象，创建一个
    if (!animation)
    {
        animation = Animation::create();
    }
    // 如果没有相机采样器，创建一个
    if (!cameraSampler)
    {
        cameraSampler = CameraSampler::create();
        cameraSampler->object = object;

        animation->samplers.push_back(cameraSampler);
    }

    // 清空或创建关键帧对象
    if (cameraSampler->keyframes)
    {
        cameraSampler->keyframes->clear();
    }
    else
    {
        cameraSampler->keyframes = CameraKeyframes::create();
    }
}

// 停止播放或录制
// 停止动画播放，或在录制模式下保存动画到文件
void CameraAnimationHandler::stop()
{
    if (playing)
    {
        // 停止播放
        info("Stopping playback.");
        playing = animation->stop(simulationTime);
    }
    else if (recording)
    {
        // 停止录制并保存动画
        info("Stop recording.");

        if (filename)
        {
            // 将录制的动画保存到文件
            if (vsg::write(animation, filename, options))
            {
                info("Written recoded path to : ", filename);
            }
        }

        recording = false;
        playing = false;
    }
}

// 处理按键事件
// 响应切换播放和录制的按键
// keyPress: 按键事件
void CameraAnimationHandler::apply(KeyPressEvent& keyPress)
{
    // 切换播放键：开始/停止播放
    if (keyPress.keyModified == togglePlaybackKey)
    {
        recording = false;
        if (animation)
        {
            if (!playing)
            {
                play();
            }
            else
            {
                stop();
            }
        }
    }
    // 切换录制键：开始/停止录制
    else if (keyPress.keyModified == toggleRecordingKey)
    {
        if (!recording)
        {
            record();
        }
        else
        {
            stop();
        }
    }
}

// 处理帧事件
// 更新动画状态或录制相机位置
// frame: 帧事件，包含帧戳信息
void CameraAnimationHandler::apply(FrameEvent& frame)
{
    // 更新模拟时间
    simulationTime = frame.frameStamp->simulationTime;

    if (!object) return;

    if (playing)
    {
        // 播放模式：更新动画
        animation->update(simulationTime);
    }
    else if (recording)
    {
        // 录制模式：记录对象状态
        object->accept(*this);
    }
}
