// XYcontrol.cpp
#include "XYcontrol.h"
#include <Arduino.h>

// 初始化位置管理器
PositionManager::PositionManager() {
    currentPosition = 0;
    // 初始化其他必要的变量
}

// 添加料盘
void PositionManager::addTray(int trayId, int position) {
    TrayPosition tray;
    tray.trayId = trayId;
    tray.position = position;
    tray.isActive = true;
    trayMap[trayId] = tray;
}

// 获取位置
int PositionManager::getPositionFromTrayId(int trayId) {
    if (trayMap.find(trayId) != trayMap.end()) {
        return trayMap[trayId].position;
    }
    return -1;  // 返回-1表示未找到
}

// 设置当前位置
void PositionManager::setCurrentPosition(int position) {
    currentPosition = position;
}

// 获取当前位置
int PositionManager::getCurrentPosition() {
    return currentPosition;
}

bool PositionManager::isTrayActive(int trayId) {
    if (trayMap.find(trayId) != trayMap.end()) {
        return trayMap[trayId].isActive;
    }
    return false;
}

void PositionManager::deactivateTray(int trayId) {
    if (trayMap.find(trayId) != trayMap.end()) {
        trayMap[trayId].isActive = false;
    }
}

void PositionManager::activateTray(int trayId) {
    if (trayMap.find(trayId) != trayMap.end()) {
        trayMap[trayId].isActive = true;
    }
}

int PositionManager::getTrayCount() {
    return trayMap.size();
}

void PositionManager::printTrayInfo() {
    Serial.println("当前料盘信息：");
    for (const auto& pair : trayMap) {
        Serial.print("料盘ID: ");
        Serial.print(pair.first);
        Serial.print(", 位置: ");
        Serial.print(pair.second.position);
        Serial.print(", 状态: ");
        Serial.println(pair.second.isActive ? "激活" : "未激活");
    }
}

bool PositionManager::validatePosition(int position) {
    // 检查位置是否在有效范围内
    return position >= 0 && position <= 1000;  // 假设最大范围是1000
}

bool PositionManager::validateTrayId(int trayId) {
    return trayMap.find(trayId) != trayMap.end();
}

bool PositionManager::moveToTray(int trayId) {
    if (!validateTrayId(trayId)) {
        Serial.println("无效的料盘ID");
        return false;
    }
    
    int targetPosition = getPositionFromTrayId(trayId);
    if (!validatePosition(targetPosition)) {
        Serial.println("无效的目标位置");
        return false;
    }
    
    // TODO: 实现实际的移动控制
    currentPosition = targetPosition;
    return true;
}
