#ifndef AITARGETTYPE_H
#define AITARGETTYPE_H

#include <QString>

// ============================================================================
// AiTargetType - AI 目标类型名称映射（纯逻辑，无 UI / 无平台依赖）
// ============================================================================
// 协议参考（AIInfo.Object.<id>）：
//   Class：目标类型  0xA0 空中目标 / 0xA1 人·飞机 / 0xA2 车·直升机
//                    0xA3 船·鸟 / 0xA4 无人机
//   Model：算法模型 = 高段 × 10 + 低段
//          高段 0 可见光 / 1 红外
//          低段 2 人车识别 / 3 船识别 / 4 无人机识别 / 5 飞机直升机识别 / 6 鸟识别
//
// 同一 Class 码在不同模型下含义不同（如 0xA1：人车模型=人，飞机直升机模型=飞机），
// 故必须结合当前 Model 才能显示正确类型。
// 未覆盖的组合返回十六进制原始码（如 "0xA2"）便于排查。
namespace AiTargetType {

inline QString name(int model, int classCode)
{
    const int modelLow = model % 10;
    switch (classCode) {
    case 0xA0:
        return QStringLiteral("空中目标");
    case 0xA1:
        if (modelLow == 2) return QStringLiteral("人");
        if (modelLow == 5) return QStringLiteral("飞机");
        break;
    case 0xA2:
        if (modelLow == 2) return QStringLiteral("车");
        if (modelLow == 5) return QStringLiteral("直升机");
        break;
    case 0xA3:
        if (modelLow == 3) return QStringLiteral("船");
        if (modelLow == 6) return QStringLiteral("鸟");
        break;
    case 0xA4:
        if (modelLow == 4) return QStringLiteral("无人机");
        break;
    default:
        break;
    }
    return QString(QStringLiteral("0x")) + QString::number(classCode, 16).toUpper().rightJustified(2, QLatin1Char('0'));
}

} // namespace AiTargetType

#endif // AITARGETTYPE_H
