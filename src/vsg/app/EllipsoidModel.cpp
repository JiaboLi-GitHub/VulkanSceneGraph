/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/EllipsoidModel.h>
#include <vsg/maths/transform.h>

using namespace vsg;

// EllipsoidModel类的构造函数
// 创建椭球模型，用于地球坐标系统转换
// rEquator: 赤道半径
// rPolar: 极地半径
EllipsoidModel::EllipsoidModel(double rEquator, double rPolar) :
    _radiusEquator(rEquator),  // 赤道半径
    _radiusPolar(rPolar)  // 极地半径
{
    _computeEccentricitySquared();  // 计算偏心率平方
}

// 计算偏心率平方
// 根据赤道半径和极地半径计算椭球的偏心率平方
void EllipsoidModel::_computeEccentricitySquared()
{
    // 计算扁平率
    double flattening = (_radiusEquator - _radiusPolar) / _radiusEquator;
    // 计算偏心率平方：e² = 2f - f²
    _eccentricitySquared = 2 * flattening - flattening * flattening;
}

// 从输入流读取椭球模型数据
// 读取赤道半径和极地半径，然后重新计算偏心率平方
// input: 输入流对象
void EllipsoidModel::read(Input& input)
{
    Object::read(input);

    input.read("radiusEquator", _radiusEquator);
    input.read("radiusPolar", _radiusPolar);

    _computeEccentricitySquared();  // 重新计算偏心率平方
}

// 将椭球模型数据写入输出流
// 写入赤道半径和极地半径
// output: 输出流对象
void EllipsoidModel::write(Output& output) const
{
    Object::write(output);

    output.write("radiusEquator", _radiusEquator);
    output.write("radiusPolar", _radiusPolar);
}

// 将经纬度高度转换为地心地固坐标（ECEF）
// 将纬度、经度、高度（LLA）坐标转换为地心地固坐标系（ECEF）坐标
// lla: 纬度、经度、高度向量（度，度，米）
// 返回值：地心地固坐标向量（米）
// 数学细节参见：https://en.wikipedia.org/wiki/ECEF
dvec3 EllipsoidModel::convertLatLongAltitudeToECEF(const dvec3& lla) const
{
    const double latitude = radians(lla[0]);  // 纬度（弧度）
    const double longitude = radians(lla[1]);  // 经度（弧度）
    const double height = lla[2];  // 高度（米）

    // 计算纬度的正弦和余弦
    double sin_latitude = sin(latitude);
    double cos_latitude = cos(latitude);
    // 计算曲率半径N（子午圈曲率半径）
    double N = _radiusEquator / sqrt(1.0 - _eccentricitySquared * sin_latitude * sin_latitude);
    // 计算ECEF坐标
    return dvec3((N + height) * cos_latitude * cos(longitude),
                 (N + height) * cos_latitude * sin(longitude),
                 (N * (1 - _eccentricitySquared) + height) * sin_latitude);
}

// 将地心地固坐标（ECEF）转换为经纬度高度
// 将地心地固坐标系（ECEF）坐标转换为纬度、经度、高度（LLA）坐标
// ecef: 地心地固坐标向量（米）
// 返回值：纬度、经度、高度向量（度，度，米）
// 参考：http://www.colorado.edu/geography/gcraft/notes/datum/gif/xyzllh.gif
dvec3 EllipsoidModel::convertECEFToLatLongAltitude(const dvec3& ecef) const
{
    double latitude, longitude, height;
    const double PI_2 = PI * 0.5;

    // 处理极点和地心情况的特殊情况
    if (ecef.x != 0.0)
        // 正常情况：计算经度
        longitude = atan2(ecef.y, ecef.x);
    else
    {
        // x坐标为0的特殊情况
        if (ecef.y > 0.0)
            longitude = PI_2;
        else if (ecef.y < 0.0)
            longitude = -PI_2;
        else
        {
            // 在极点或地心
            longitude = 0.0;
            if (ecef.z > 0.0)
            { // 北极
                latitude = PI_2;
                height = ecef.z - _radiusPolar;
            }
            else if (ecef.z < 0.0)
            { // 南极
                latitude = -PI_2;
                height = -ecef.z - _radiusPolar;
            }
            else
            { // 地心
                latitude = PI_2;
                height = -_radiusPolar;
            }
            return dvec3(degrees(latitude), degrees(longitude), height);
        }
    }

    // 计算水平距离p
    double p = sqrt(ecef.x * ecef.x + ecef.y * ecef.y);
    // 计算辅助角度theta
    double theta = atan2(ecef.z * _radiusEquator, (p * _radiusPolar));
    // 计算第二偏心率平方
    double eDashSquared = (_radiusEquator * _radiusEquator - _radiusPolar * _radiusPolar) /
                          (_radiusPolar * _radiusPolar);

    double sin_theta = sin(theta);
    double cos_theta = cos(theta);

    // 使用迭代方法计算纬度
    latitude = atan((ecef.z + eDashSquared * _radiusPolar * sin_theta * sin_theta * sin_theta) /
                    (p - _eccentricitySquared * _radiusEquator * cos_theta * cos_theta * cos_theta));

    // 计算曲率半径N
    double sin_latitude = sin(latitude);
    double N = _radiusEquator / sqrt(1.0 - _eccentricitySquared * sin_latitude * sin_latitude);

    // 计算高度
    height = p / cos(latitude) - N;
    return dvec3(degrees(latitude), degrees(longitude), height);
}

// 计算从局部坐标系到世界坐标系的变换矩阵
// 计算在指定经纬度高度位置的局部坐标系到地心地固坐标系的变换矩阵
// lla: 纬度、经度、高度向量（度，度，米）
// 返回值：局部到世界的变换矩阵
dmat4 EllipsoidModel::computeLocalToWorldTransform(const dvec3& lla) const
{
    // 将LLA坐标转换为ECEF坐标
    dvec3 ecef = convertLatLongAltitudeToECEF(lla);

    const double latitude = radians(lla[0]);  // 纬度（弧度）
    const double longitude = radians(lla[1]);  // 经度（弧度）

    // 计算上、东、北向量（局部坐标系的方向向量）
    // 上向量：指向天空的方向
    dvec3 up(cos(longitude) * cos(latitude), sin(longitude) * cos(latitude), sin(latitude));
    // 东向量：指向东方的方向
    dvec3 east(-sin(longitude), cos(longitude), 0.0);
    // 北向量：指向北方的方向（上向量和东向量的叉积）
    dvec3 north = cross(up, east);

    // 创建平移矩阵（平移到ECEF位置）
    dmat4 localToWorld = vsg::translate(ecef);

    // 设置旋转矩阵（将局部坐标系的轴设置为东、北、上）
    localToWorld(0, 0) = east[0];
    localToWorld(0, 1) = east[1];
    localToWorld(0, 2) = east[2];

    localToWorld(1, 0) = north[0];
    localToWorld(1, 1) = north[1];
    localToWorld(1, 2) = north[2];

    localToWorld(2, 0) = up[0];
    localToWorld(2, 1) = up[1];
    localToWorld(2, 2) = up[2];

    return localToWorld;
}

// 计算从世界坐标系到局部坐标系的变换矩阵
// 计算从地心地固坐标系到指定经纬度高度位置的局部坐标系的变换矩阵
// lla: 纬度、经度、高度向量（度，度，米）
// 返回值：世界到局部的变换矩阵（局部到世界变换矩阵的逆矩阵）
dmat4 EllipsoidModel::computeWorldToLocalTransform(const dvec3& lla) const
{
    return vsg::inverse(computeLocalToWorldTransform(lla));
}
