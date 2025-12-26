/* <editor-fold desc="MIT License">

Copyright(c) 2019 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/Trackball.h>
#include <vsg/io/Logger.h>

#include <algorithm>
#include <iostream>

using namespace vsg;

// Trackball类的构造函数
// 创建轨迹球控制器，用于交互式相机控制
// camera: 相机对象
// ellipsoidModel: 椭球模型（用于地球坐标系统，可选）
Trackball::Trackball(ref_ptr<Camera> camera, ref_ptr<EllipsoidModel> ellipsoidModel) :
    _camera(camera),  // 相机对象
    _lookAt(camera->viewMatrix.cast<LookAt>()),  // LookAt视图矩阵
    _ellipsoidModel(ellipsoidModel),  // 椭球模型
    _keyboard(Keyboard::create())  // 键盘对象
{
    // 如果没有LookAt视图矩阵，创建一个新的
    if (!_lookAt)
    {
        _lookAt = new LookAt;
    }

    // 将相机约束到地球表面
    clampToGlobe();

    // 添加空格键视角点（保存当前视角）
    addKeyViewpoint(KEY_Space, LookAt::create(*_lookAt), 1.0);
}

// 将相机约束到地球表面
// 确保相机中心点在地球表面上，眼睛位置不低于最小高度
void Trackball::clampToGlobe()
{
    // debug("Trackball::clampToGlobe()");

    if (!_ellipsoidModel) return;  // 没有椭球模型，跳过

    // 获取当前LookAt中心和眼睛位置的经纬度高度
    auto location_center = _ellipsoidModel->convertECEFToLatLongAltitude(_lookAt->center);
    auto location_eye = _ellipsoidModel->convertECEFToLatLongAltitude(_lookAt->eye);

    // 计算中心点在地球表面的位置
    double ratio = location_eye.z / (location_eye.z - location_center.z);
    auto location = _ellipsoidModel->convertECEFToLatLongAltitude(_lookAt->center * ratio + _lookAt->eye * (1.0 - ratio));

    // 将高度约束到地球表面（高度为0）
    location.z = 0.0;

    // 将约束后的位置转换回ECEF坐标
    auto ecef = _ellipsoidModel->convertLatLongAltitudeToECEF(location);

    // 应用新的约束位置到LookAt中心
    _lookAt->center = ecef;

    // 确保眼睛位置不低于最小高度
    double minimum_altitude = 0.1;
    if (location_eye.z < minimum_altitude)
    {
        location_eye.z = minimum_altitude;
        _lookAt->eye = _ellipsoidModel->convertLatLongAltitudeToECEF(location_eye);
        _thrown = false;  // 停止惯性
    }
}

// 获取相机渲染区域坐标
// 将指针事件的窗口坐标转换为相机渲染区域坐标（考虑窗口偏移）
// pointerEvent: 指针事件
// 返回值：相机渲染区域坐标（x, y）
std::pair<int32_t, int32_t> Trackball::cameraRenderAreaCoordinates(const PointerEvent& pointerEvent) const
{
    // 如果有窗口偏移，应用偏移
    if (!windowOffsets.empty())
    {
        auto itr = windowOffsets.find(pointerEvent.window);
        if (itr != windowOffsets.end())
        {
            const auto& offset = itr->second;
            return {pointerEvent.x + offset.x, pointerEvent.y + offset.y};
        }
    }
    // 没有偏移，直接返回原始坐标
    return {pointerEvent.x, pointerEvent.y};
}

// 检查指针事件是否在渲染区域内
// 检查指针事件的坐标是否在相机的渲染区域内
// pointerEvent: 指针事件
// 返回值：true表示在渲染区域内，false表示不在
bool Trackball::withinRenderArea(const PointerEvent& pointerEvent) const
{
    auto renderArea = _camera->getRenderArea();  // 获取渲染区域
    auto [x, y] = cameraRenderAreaCoordinates(pointerEvent);  // 获取相机坐标

    // 检查坐标是否在渲染区域内
    return (x >= renderArea.offset.x && x < static_cast<int32_t>(renderArea.offset.x + renderArea.extent.width)) &&
           (y >= renderArea.offset.y && y < static_cast<int32_t>(renderArea.offset.y + renderArea.extent.height));
}

// 检查事件是否相关
// 检查窗口事件是否来自与轨迹球关联的窗口
// event: 窗口事件
// 返回值：true表示事件相关，false表示不相关
// 如果没有关联窗口，假设所有事件都相关
bool Trackball::eventRelevant(const WindowEvent& event) const
{
    // if no windows have been associated with Trackball with a Trackball::addWindow() then assume event is relevant and should be handled
    if (windowOffsets.empty()) return true;  // 没有关联窗口，所有事件都相关

    return (windowOffsets.count(event.window) > 0);  // 检查窗口是否在关联列表中
}

// 计算归一化设备坐标（NDC）
// 从事件坐标计算归一化设备坐标（范围：-1到1）
// event: 指针事件
// 返回值：归一化设备坐标（x, y）
dvec2 Trackball::ndc(const PointerEvent& event)
{
    auto renderArea = _camera->getRenderArea();  // 获取渲染区域
    auto [x, y] = cameraRenderAreaCoordinates(event);  // 获取相机坐标

    // 计算宽高比
    double aspectRatio = static_cast<double>(renderArea.extent.width) / static_cast<double>(renderArea.extent.height);
    // 计算归一化坐标（考虑宽高比）
    dvec2 v(
        (renderArea.extent.width > 0) ? (static_cast<double>(x - renderArea.offset.x) / static_cast<double>(renderArea.extent.width) * 2.0 - 1.0) * aspectRatio : 0.0,
        (renderArea.extent.height > 0) ? static_cast<double>(y - renderArea.offset.y) / static_cast<double>(renderArea.extent.height) * 2.0 - 1.0 : 0.0);
    return v;
}

// 计算轨迹球坐标（TBC）
// 从事件坐标计算轨迹球坐标（用于旋转计算）
// event: 指针事件
// 返回值：轨迹球坐标（x, y, z）
dvec3 Trackball::tbc(const PointerEvent& event)
{
    dvec2 v = ndc(event);  // 获取归一化设备坐标

    double l = length(v);  // 计算距离原点的距离
    if (l < 1.0f)
    {
        // 在单位圆内：计算z坐标（球面高度）
        double h = 0.5 + cos(l * PI) * 0.5;
        return dvec3(v.x, -v.y, h);  // y坐标取反（屏幕坐标系）
    }
    else
    {
        // 在单位圆外：z坐标为0（平面）
        return dvec3(v.x, -v.y, 0.0);
    }
}

void Trackball::apply(KeyPressEvent& keyPress)
{
    if (_keyboard) keyPress.accept(*_keyboard);

    if (!_hasKeyboardFocus || keyPress.handled || !eventRelevant(keyPress)) return;

    if (auto itr = keyViewpointMap.find(keyPress.keyBase); itr != keyViewpointMap.end())
    {
        _previousTime = keyPress.time;

        setViewpoint(itr->second.lookAt, itr->second.duration);

        keyPress.handled = true;
    }
}

void Trackball::apply(KeyReleaseEvent& keyRelease)
{
    if (_keyboard) keyRelease.accept(*_keyboard);
}

void Trackball::apply(FocusInEvent& focusIn)
{
    if (_keyboard) focusIn.accept(*_keyboard);
}

void Trackball::apply(FocusOutEvent& focusOut)
{
    if (_keyboard) focusOut.accept(*_keyboard);
}

void Trackball::apply(ButtonPressEvent& buttonPress)
{
    if (buttonPress.handled || !eventRelevant(buttonPress))
    {
        _hasKeyboardFocus = false;
        return;
    }

    _hasPointerFocus = _hasKeyboardFocus = withinRenderArea(buttonPress);
    _lastPointerEventWithinRenderArea = _hasPointerFocus;

    if (buttonPress.mask & rotateButtonMask)
        _updateMode = ROTATE;
    else if (buttonPress.mask & panButtonMask)
        _updateMode = PAN;
    else if (buttonPress.mask & zoomButtonMask)
        _updateMode = ZOOM;
    else
        _updateMode = INACTIVE;

    if (_hasPointerFocus) buttonPress.handled = true;

    _zoomPreviousRatio = 0.0;
    _pan.set(0.0, 0.0);
    _rotateAngle = 0.0;

    _previousPointerEvent = &buttonPress;
}

void Trackball::apply(ButtonReleaseEvent& buttonRelease)
{
    if (buttonRelease.handled || !eventRelevant(buttonRelease)) return;

    if (!windowOffsets.empty() && windowOffsets.count(buttonRelease.window) == 0) return;

    if (supportsThrow)
    {
        _thrown = _previousPointerEvent && (std::chrono::duration_cast<std::chrono::milliseconds>(buttonRelease.time - _previousPointerEvent->time).count() == 0);
    }

    _lastPointerEventWithinRenderArea = withinRenderArea(buttonRelease);
    _hasPointerFocus = false;

    _previousPointerEvent = &buttonRelease;
}

void Trackball::apply(MoveEvent& moveEvent)
{
    if (!eventRelevant(moveEvent)) return;

    _lastPointerEventWithinRenderArea = withinRenderArea(moveEvent);

    if (moveEvent.handled || !_hasPointerFocus) return;

    dvec2 new_ndc = ndc(moveEvent);
    dvec3 new_tbc = tbc(moveEvent);

    if (!_previousPointerEvent) _previousPointerEvent = &moveEvent;

    dvec2 prev_ndc = ndc(*_previousPointerEvent);
    dvec3 prev_tbc = tbc(*_previousPointerEvent);

#if 1
    dvec2 control_ndc = new_ndc;
    dvec3 control_tbc = new_tbc;
#else
    dvec2 control_ndc = (new_ndc + prev_ndc) * 0.5;
    dvec3 control_tbc = (new_tbc + prev_tbc) * 0.5;
#endif

    double dt = std::chrono::duration<double, std::chrono::seconds::period>(moveEvent.time - _previousPointerEvent->time).count();
    _previousDelta = dt;

    double scale = 1.0;
    //if (_previousTime > _previousPointerEvent->time) scale = std::chrono::duration<double, std::chrono::seconds::period>(moveEvent.time - _previousTime).count() / dt;
    //    scale *= 2.0;

    _previousTime = moveEvent.time;

    if (moveEvent.mask & rotateButtonMask)
    {
        _updateMode = ROTATE;

        moveEvent.handled = true;

        dvec3 xp = cross(normalize(control_tbc), normalize(prev_tbc));
        double xp_len = length(xp);
        if (xp_len > 0.0)
        {
            _rotateAngle = asin(xp_len);
            _rotateAxis = xp / xp_len;

            rotate(_rotateAngle * scale, _rotateAxis);
        }
        else
        {
            _rotateAngle = 0.0;
        }
    }
    else if (moveEvent.mask & panButtonMask)
    {
        _updateMode = PAN;

        moveEvent.handled = true;

        dvec2 delta = control_ndc - prev_ndc;

        _pan = delta;

        pan(delta * scale);
    }
    else if (moveEvent.mask & zoomButtonMask)
    {
        _updateMode = ZOOM;

        moveEvent.handled = true;

        dvec2 delta = control_ndc - prev_ndc;

        if (delta.y != 0.0)
        {
            _zoomPreviousRatio = zoomScale * 2.0 * delta.y;
            zoom(_zoomPreviousRatio * scale);
        }
    }

    _thrown = false;

    _previousPointerEvent = &moveEvent;
}

void Trackball::apply(ScrollWheelEvent& scrollWheel)
{
    if (scrollWheel.handled || !eventRelevant(scrollWheel) || !_lastPointerEventWithinRenderArea) return;

    scrollWheel.handled = true;

    zoom(scrollWheel.delta.y * 0.1);
}

void Trackball::apply(TouchDownEvent& touchDown)
{
    if (!eventRelevant(touchDown)) return;

    _previousTouches[touchDown.id] = &touchDown;
    switch (touchDown.id)
    {
    case 0: {
        if (_previousTouches.size() == 1)
        {
            vsg::ref_ptr<vsg::Window> w = touchDown.window;
            vsg::ref_ptr<vsg::ButtonPressEvent> evt = vsg::ButtonPressEvent::create(
                w,
                touchDown.time,
                touchDown.x,
                touchDown.y,
                touchMappedToButtonMask,
                touchDown.id);
            apply(*evt.get());
        }
        break;
    }
    case 1: {
        _prevZoomTouchDistance = 0.0;
        if (touchDown.id == 0 && _previousTouches.count(1))
        {
            const auto& prevTouch1 = _previousTouches[1];
            auto a = std::abs(static_cast<double>(prevTouch1->x) - touchDown.x);
            auto b = std::abs(static_cast<double>(prevTouch1->y) - touchDown.y);
            if (a > 0 || b > 0)
                _prevZoomTouchDistance = sqrt(a * a + b * b);
        }
        break;
    }
    }
}

void Trackball::apply(TouchUpEvent& touchUp)
{
    if (!eventRelevant(touchUp)) return;

    if (touchUp.id == 0 && _previousTouches.size() == 1)
    {
        vsg::ref_ptr<vsg::Window> w = touchUp.window;
        vsg::ref_ptr<vsg::ButtonReleaseEvent> evt = vsg::ButtonReleaseEvent::create(
            w,
            touchUp.time,
            touchUp.x,
            touchUp.y,
            touchMappedToButtonMask,
            touchUp.id);
        apply(*evt.get());
    }
    _previousTouches.erase(touchUp.id);
}

void Trackball::apply(TouchMoveEvent& touchMove)
{
    if (!eventRelevant(touchMove)) return;

    vsg::ref_ptr<vsg::Window> w = touchMove.window;
    switch (_previousTouches.size())
    {
    case 1: {
        // Rotate
        vsg::ref_ptr<vsg::MoveEvent> evt = vsg::MoveEvent::create(
            w,
            touchMove.time,
            touchMove.x,
            touchMove.y,
            touchMappedToButtonMask);
        apply(*evt.get());
        break;
    }
    case 2: {
        if (touchMove.id == 0 && _previousTouches.count(0))
        {
            // Zoom
            const auto& prevTouch1 = _previousTouches[1];
            auto a = std::abs(static_cast<double>(prevTouch1->x) - touchMove.x);
            auto b = std::abs(static_cast<double>(prevTouch1->y) - touchMove.y);
            if (a > 0 || b > 0)
            {
                auto touchZoomDistance = sqrt(a * a + b * b);
                if (_prevZoomTouchDistance && touchZoomDistance > 0)
                {
                    auto zoomLevel = touchZoomDistance / _prevZoomTouchDistance;
                    if (zoomLevel < 1)
                        zoomLevel = -(1 / zoomLevel);
                    zoomLevel *= 0.1;
                    zoom(zoomLevel);
                }
                _prevZoomTouchDistance = touchZoomDistance;
            }
        }
        break;
    }
    }
    _previousTouches[touchMove.id] = &touchMove;
}

void Trackball::apply(FrameEvent& frame)
{
    // std::cout<<"Trackball::apply(FrameEvent&) frameCount = "<<frame.frameStamp->frameCount<<std::endl;
    if (_hasKeyboardFocus && _keyboard)
    {
        auto times2speed = [](std::pair<double, double> duration) -> double {
            if (duration.first <= 0.0) return 0.0;
            double speed = duration.first >= 1.0 ? 1.0 : duration.first;

            if (duration.second > 0.0)
            {
                // key has been released so slow down
                speed -= duration.second;
                return speed > 0.0 ? speed : 0.0;
            }
            else
            {
                // key still pressed so return speed based on duration of press
                return speed;
            }
        };

        double speed = 0.0;
        vsg::dvec3 move(0.0, 0.0, 0.0);
        if ((speed = times2speed(_keyboard->times(moveLeftKey))) != 0.0) move.x += -speed;
        if ((speed = times2speed(_keyboard->times(moveRightKey))) != 0.0) move.x += speed;
        if ((speed = times2speed(_keyboard->times(moveUpKey))) != 0.0) move.y += speed;
        if ((speed = times2speed(_keyboard->times(moveDownKey))) != 0.0) move.y += -speed;
        if ((speed = times2speed(_keyboard->times(moveForwardKey))) != 0.0) move.z += speed;
        if ((speed = times2speed(_keyboard->times(moveBackwardKey))) != 0.0) move.z += -speed;

        vsg::dvec3 rot(0.0, 0.0, 0.0);
        if ((speed = times2speed(_keyboard->times(turnLeftKey))) != 0.0) rot.x += speed;
        if ((speed = times2speed(_keyboard->times(turnRightKey))) != 0.0) rot.x -= speed;
        if ((speed = times2speed(_keyboard->times(pitchUpKey))) != 0.0) rot.y += speed;
        if ((speed = times2speed(_keyboard->times(pitchDownKey))) != 0.0) rot.y -= speed;
        if ((speed = times2speed(_keyboard->times(rollLeftKey))) != 0.0) rot.z -= speed;
        if ((speed = times2speed(_keyboard->times(rollRightKey))) != 0.0) rot.z += speed;

        if (rot || move)
        {
            double scale = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - _previousTime).count();
            double scaleTranslation = scale * 0.2 * length(_lookAt->center - _lookAt->eye);
            double scaleRotation = scale * 0.5;

            dvec3 upVector = _lookAt->up;
            dvec3 lookVector = vsg::normalize(_lookAt->center - _lookAt->eye);
            dvec3 sideVector = vsg::normalize(vsg::cross(lookVector, upVector));

            dvec3 delta = sideVector * (scaleTranslation * move.x) + upVector * (scaleTranslation * move.y) + lookVector * (scaleTranslation * move.z);
            dmat4 matrix = vsg::translate(_lookAt->eye + delta) *
                           vsg::rotate(rot.x * scaleRotation, upVector) *
                           vsg::rotate(rot.y * scaleRotation, sideVector) *
                           vsg::rotate(rot.z * scaleRotation, lookVector) *
                           vsg::translate(-_lookAt->eye);

            _lookAt->up = normalize(matrix * (_lookAt->eye + _lookAt->up) - matrix * _lookAt->eye);
            _lookAt->center = matrix * _lookAt->center;
            _lookAt->eye = matrix * _lookAt->eye;

            clampToGlobe();
            _thrown = false;
        }
    }

    if (_endLookAt)
    {
        double timeSinceOfAnimation = std::chrono::duration<double, std::chrono::seconds::period>(frame.time - _startTime).count();
        if (timeSinceOfAnimation < _animationDuration)
        {
            double r = smoothstep(0.0, 1.0, timeSinceOfAnimation / _animationDuration);

            if (_ellipsoidModel)
            {
                auto interpolate = [](const dvec3& start, const dvec3& end, double ratio) -> dvec3 {
                    if (ratio >= 1.0) return end;

                    double length_start = length(start);
                    double length_end = length(end);
                    double acos_ratio = dot(start, end) / (length_start * length_end);
                    double angle = acos_ratio >= 1.0 ? 0.0 : (acos_ratio <= -1.0 ? vsg::PI : acos(acos_ratio));
                    auto cross_start_end = cross(start, end);
                    auto length_cross = length(cross_start_end);
                    if (angle != 0.0 && length_cross != 0.0)
                    {
                        cross_start_end /= length_cross;
                        auto rotation = vsg::rotate(angle * ratio, cross_start_end);
                        dvec3 new_dir = normalize(rotation * start);
                        return new_dir * mix(length_start, length_end, ratio);
                    }
                    else
                    {
                        return mix(start, end, ratio);
                    }
                };

                auto interpolate_arc = [](const dvec3& start, const dvec3& end, double ratio, double arc_height = 0.0) -> dvec3 {
                    if (ratio >= 1.0) return end;

                    double length_start = length(start);
                    double length_end = length(end);
                    double acos_ratio = dot(start, end) / (length_start * length_end);
                    double angle = acos_ratio >= 1.0 ? 0.0 : (acos_ratio <= -1.0 ? vsg::PI : acos(acos_ratio));
                    auto cross_start_end = cross(start, end);
                    auto length_cross = length(cross_start_end);
                    if (angle != 0.0 && length_cross != 0.0)
                    {
                        cross_start_end /= length_cross;
                        auto rotation = vsg::rotate(angle * ratio, cross_start_end);
                        dvec3 new_dir = normalize(rotation * start);
                        double target_length = mix(length_start, length_end, ratio) + (ratio - ratio * ratio) * arc_height * 4.0;
                        return new_dir * target_length;
                    }
                    else
                    {
                        return mix(start, end, ratio);
                    }
                };

                double length_center_start = length(_startLookAt->center);
                double length_center_end = length(_endLookAt->center);
                double length_center_mid = (length_center_start + length_center_end) * 0.5;
                double distance_between = length(_startLookAt->center - _endLookAt->center);

                double transition_length = length_center_mid + distance_between;

                double length_eye_start = length(_startLookAt->eye);
                double length_eye_end = length(_endLookAt->eye);
                double length_eye_mid = (length_eye_start + length_eye_end) * 0.5;

                double arc_height = (transition_length > length_eye_mid) ? (transition_length - length_eye_mid) : 0.0;

                _lookAt->eye = interpolate_arc(_startLookAt->eye, _endLookAt->eye, r, arc_height);
                _lookAt->center = interpolate(_startLookAt->center, _endLookAt->center, r);
                _lookAt->up = interpolate(_startLookAt->up, _endLookAt->up, r);
            }
            else
            {
                _lookAt->eye = mix(_startLookAt->eye, _endLookAt->eye, r);
                _lookAt->center = mix(_startLookAt->center, _endLookAt->center, r);

                double angle = acos(dot(_startLookAt->up, _endLookAt->up) / (length(_startLookAt->up) * length(_endLookAt->up)));
                if (angle > 1.0e-6)
                {
                    auto rotation = vsg::rotate(angle * r, normalize(cross(_startLookAt->up, _endLookAt->up)));
                    _lookAt->up = rotation * _startLookAt->up;
                }
                else
                {
                    _lookAt->up = _endLookAt->up;
                }
            }
        }
        else
        {
            _lookAt->eye = _endLookAt->eye;
            _lookAt->center = _endLookAt->center;
            _lookAt->up = _endLookAt->up;

            _endLookAt = nullptr;
            _animationDuration = 0.0;
        }
    }
    else if (_thrown)
    {
        double scale = _previousDelta > 0.0 ? std::chrono::duration<double, std::chrono::seconds::period>(frame.time - _previousTime).count() / _previousDelta : 0.0;
        switch (_updateMode)
        {
        case (ROTATE):
            rotate(_rotateAngle * scale, _rotateAxis);
            break;
        case (PAN):
            pan(_pan * scale);
            break;
        case (ZOOM):
            zoom(_zoomPreviousRatio * scale);
            break;
        default:
            break;
        }
    }

    _previousTime = frame.time;
}

// 旋转相机
// 围绕指定轴旋转相机
// angle: 旋转角度（弧度）
// axis: 旋转轴
void Trackball::rotate(double angle, const dvec3& axis)
{
    dmat4 rotation = vsg::rotate(angle, axis);  // 创建旋转矩阵
    dmat4 lv = lookAt(_lookAt->eye, _lookAt->center, _lookAt->up);  // 创建LookAt视图矩阵
    dvec3 centerEyeSpace = (lv * _lookAt->center);  // 将中心点转换到眼睛空间

    // 构建变换矩阵：先转换到眼睛空间，围绕中心旋转，再转换回世界空间
    dmat4 matrix = inverse(lv) * translate(centerEyeSpace) * rotation * translate(-centerEyeSpace) * lv;

    // 应用变换到LookAt参数
    _lookAt->up = normalize(matrix * (_lookAt->eye + _lookAt->up) - matrix * _lookAt->eye);
    _lookAt->center = matrix * _lookAt->center;
    _lookAt->eye = matrix * _lookAt->eye;

    // 约束到地球表面
    clampToGlobe();
}

// 缩放相机（拉近/拉远）
// 沿着视线方向移动相机
// ratio: 缩放比例（正数拉近，负数拉远）
void Trackball::zoom(double ratio)
{
    dvec3 lookVector = _lookAt->center - _lookAt->eye;  // 视线向量
    _lookAt->eye = _lookAt->eye + lookVector * ratio;  // 沿视线方向移动眼睛位置

    // 约束到地球表面
    clampToGlobe();
}

// 平移相机（平移视图）
// 在垂直于视线的平面上平移相机
// delta: 平移增量（归一化设备坐标）
void Trackball::pan(const dvec2& delta)
{
    dvec3 lookVector = _lookAt->center - _lookAt->eye;  // 视线向量
    dvec3 lookNormal = normalize(lookVector);  // 视线方向
    dvec3 upNormal = _lookAt->up;  // 上方向
    dvec3 sideNormal = cross(lookNormal, upNormal);  // 侧方向（视线和上方向的叉积）

    double distance = length(lookVector);  // 视线距离
    distance *= 0.25;  // 缩放平移距离

    if (_ellipsoidModel)
    {
        // 使用椭球模型：围绕地球中心旋转
        double scale = distance;
        double angle = (length(delta) * scale) / _ellipsoidModel->radiusEquator();  // 计算旋转角度

        if (angle != 0.0)
        {
            dvec3 globeNormal = normalize(_lookAt->center);  // 地球中心方向
            dvec3 m = upNormal * (-delta.y) + sideNormal * (delta.x); // 计算眼睛平面中相对于中心的位置
            dvec3 v = m + lookNormal * dot(m, globeNormal);           // 补偿相对于地球法线的倾斜
            dvec3 axis = normalize(cross(globeNormal, v));            // 计算旋转轴以映射鼠标平移

            dmat4 matrix = vsg::rotate(-angle, axis);  // 创建旋转矩阵

            // 应用旋转到LookAt参数
            _lookAt->up = normalize(matrix * (_lookAt->eye + _lookAt->up) - matrix * _lookAt->eye);
            _lookAt->center = matrix * _lookAt->center;
            _lookAt->eye = matrix * _lookAt->eye;

            clampToGlobe();  // 约束到地球表面
        }
    }
    else
    {
        // 不使用椭球模型：简单平移
        dvec3 translation = sideNormal * (-delta.x * distance) + upNormal * (delta.y * distance);

        _lookAt->eye = _lookAt->eye + translation;
        _lookAt->center = _lookAt->center + translation;
    }
}

void Trackball::addWindow(ref_ptr<Window> window, const ivec2& offset)
{
    windowOffsets[observer_ptr<Window>(window)] = offset;
}

void Trackball::addKeyViewpoint(KeySymbol key, ref_ptr<LookAt> lookAt, double duration)
{
    keyViewpointMap[key].lookAt = lookAt;
    keyViewpointMap[key].duration = duration;
}

void Trackball::addKeyViewpoint(KeySymbol key, double latitude, double longitude, double altitude, double duration)
{
    if (!_ellipsoidModel) return;

    auto lookAt = LookAt::create();
    lookAt->eye = _ellipsoidModel->convertLatLongAltitudeToECEF(dvec3(latitude, longitude, altitude));
    lookAt->center = _ellipsoidModel->convertLatLongAltitudeToECEF(dvec3(latitude, longitude, 0.0));
    lookAt->up = normalize(cross(lookAt->center, dvec3(-lookAt->center.y, lookAt->center.x, 0.0)));

    keyViewpointMap[key].lookAt = lookAt;
    keyViewpointMap[key].duration = duration;
}

void Trackball::setViewpoint(ref_ptr<LookAt> lookAt, double duration)
{
    if (!lookAt) return;

    _thrown = false;

    if (duration == 0.0)
    {
        _lookAt->eye = lookAt->eye;
        _lookAt->center = lookAt->center;
        _lookAt->up = lookAt->up;

        _startLookAt = nullptr;
        _endLookAt = nullptr;
        _animationDuration = 0.0;

        clampToGlobe();
    }
    else
    {
        _startTime = _previousTime;
        _startLookAt = vsg::LookAt::create(*_lookAt);
        _endLookAt = lookAt;
        _animationDuration = duration;
    }
}
