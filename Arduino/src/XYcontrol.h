/*
    这个文件是用于管理料盘的位置的，包括料盘的添加、删除、移动等操作
    1. 目前能做什么
    1.1 位置管理：
        1.1.1 可以添加和管理多个料盘的位置
        1.1.2 可以获取和设置当前位置
        1.1.3 可以查询料盘的位置信息
    1.2 配置管理：
        1.2.1 可以保存料盘位置配置
        1.2.2 可以读取料盘位置配置
        1.2.3 配置在重启后保持
    1.3 状态处理：
        1.3.1 可以检测是否需要移动
        1.3.2 可以获取目标位置
        1.3.3 可以触发移动命令

    2. 还不能做什么
    2.1 实际移动控制：
        2.1.1 还没有实现实际的龙门架移动控制
        2.1.2 没有位置传感器支持
        2.1.3 没有位置校准功能
    2.2 安全机制：
        2.2.1 没有限位保护
        2.2.2 没有碰撞检测
        2.2.3 没有紧急停止功能
    2.3 高级功能：
        2.3.1 没有自动校准
        2.3.2 没有位置补偿
        2.3.3 没有错误恢复机制

    3. 下一步需要实现
    3.1 龙门架控制：
        3.1.1 实现实际的移动控制
        3.1.2 添加位置传感器支持
        3.1.3 实现位置反馈
    3.2 安全机制：
        3.2.1 添加限位开关
        3.2.2 实现碰撞检测
        3.2.3 添加紧急停止
    3.3 高级功能：
        3.3.1 实现自动校准
        3.3.2 添加位置补偿
        3.3.3 实现错误恢复
*/

// 在原有变量声明区域添加
struct TrayPosition {
    int trayId;      // 料盘ID
    int position;    // 物理位置
    bool isActive;   // 是否激活
};



// 添加位置管理类
class PositionManager {
private:
    int currentPosition;
    std::map<int, TrayPosition> trayMap;

    bool validatePosition(int position); //验证位置的函数
    bool validateTrayId(int trayId); //验证料盘ID的函数

public:
    PositionManager() ;
    void addTray(int trayId, int position); //添加料盘的函数，料盘ID和物理位置
    int getPositionFromTrayId(int trayId); //根据料盘ID获取物理位置的函数
    void setCurrentPosition(int position); //设置当前位置的函数
    int getCurrentPosition(); //获取当前位置的函数

        // 新增方法
    bool isTrayActive(int trayId); //验证料盘是否激活的函数
    void deactivateTray(int trayId); //禁用料盘的函数
    void activateTray(int trayId); //启用料盘的函数
    int getTrayCount(); //获取料盘数量的函数
    void printTrayInfo();  // 调试用，打印料盘信息的函数
    bool moveToTray(int trayId);  // 移动到指定料盘

};

// 创建位置管理器实例
PositionManager positionManager;