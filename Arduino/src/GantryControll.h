#include <map>

// 添加龙门架控制类
class GantryController {
private:
    int currentPosition;
    int targetPosition;
    bool isMoving;
    int lastTrayId;  // 记录上一次的料盘ID
    std::map<int, int> trayPositions;  // 料盘ID到位置的映射
    
public:
    GantryController() {
        currentPosition = 0;
        targetPosition = 0;
        isMoving = false;
        lastTrayId = 0;
    }
    
    void moveToPosition(int position) {
        targetPosition = position;
        isMoving = true;
        // TODO: 实现实际的龙门架移动控制
    }
    
    bool isAtPosition() {
        return currentPosition == targetPosition;
    }
    
    void update() {
        if(isMoving) {
            // TODO: 更新当前位置
            // 这里需要根据实际的硬件实现来编写
        }
    }

    // 添加位置管理相关函数
    void addTrayPosition(int trayId, int position) {
        trayPositions[trayId] = position;
    }

    int getPositionFromTrayId(int trayId) {
        return trayPositions[trayId];
    }

    bool needToMove(int nextTrayId) {
        // 如果下一个料盘和当前料盘相同，不需要移动
        if (nextTrayId == lastTrayId) {
            return false;
        }
        
        // 更新记录
        lastTrayId = nextTrayId;
        return true;
    }

    bool isValidPosition(int trayId) {
        return trayPositions.find(trayId) != trayPositions.end();
    }

    void setCurrentPosition(int position) {
        currentPosition = position;
    }

    int getCurrentPosition() {
        return currentPosition;
    }

    int getLastTrayId() {
        return lastTrayId;
    }
};

// 创建龙门架控制器实例
GantryController gantryController;