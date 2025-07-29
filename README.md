# Qt6 线程池可视化系统

---

## 项目概述

本项目是一个基于 Qt6/C++17 的线程池系统，支持动态线程管理、多种任务调度算法、实时性能监控和专业级可视化界面。  
采用信号槽机制实现线程池与 UI 的彻底解耦，支持6种经典调度算法（FIFO、LIFO、SJF、LJF、PRIO、HRRN），具备完整的性能指标统计和优雅的可视化展示。

---

## 功能特性

### 核心功能
- **动态线程管理**：支持最小/最大线程数配置，自动扩容和缩容
- **多算法调度**：支持6种经典任务调度算法
- **任务队列管理**：支持单任务/批量任务添加，支持间隔添加
- **实时状态监控**：可视化显示线程状态、任务队列、已完成任务
- **性能指标统计**：平均等待时间、平均响应比、吞吐量、CPU利用率

### 调度算法
- **FIFO（先进先出）**：按任务ID排序，先到先服务
- **LIFO（后进先出）**：按任务ID倒序，后到先服务
- **SJF（短作业优先）**：按执行时间升序，短任务优先
- **LJF（长作业优先）**：按执行时间降序，长任务优先
- **PRIO（优先级优先）**：按优先级降序，高优先级优先
- **HRRN（最高响应比优先）**：动态计算响应比，防止饥饿

### 技术特性
- **线程安全**：QMutex/QWaitCondition 保护所有共享资源
- **信号槽机制**：UI与业务彻底解耦，所有刷新统一由快照数据驱动
- **优雅架构**：虚函数实现调度器多态，代码结构清晰
- **动态排序**：HRRN算法支持实时重新排序，响应比随时间动态变化

---

## 设计亮点

### 1. 调度器架构设计
- **基类抽象**：`TaskScheduler` 基类定义统一接口
- **多态实现**：每种算法独立实现，支持运行时切换
- **虚函数优化**：`needsDynamicSort()` 方法优雅处理动态排序需求
- **代码复用**：`insertByPolicy()` 调用 `sortQueue()` 减少重复代码

### 2. HRRN算法实现
- **动态响应比**：响应比 = (等待时间 + 服务时间) / 服务时间
- **实时排序**：每次取任务时重新计算和排序
- **防止饥饿**：等待时间越长，响应比越高，优先级越高
- **性能优化**：只对HRRN算法重新排序，其他算法保持静态排序

### 3. 性能指标系统
- **平均等待时间**：Σ(完成时间 - 到达时间) / 已完成任务数
- **平均响应比**：Σ(各任务响应比) / 已完成任务数
- **吞吐量**：已完成任务数 / 线程池运行时间
- **CPU利用率**：(忙碌线程数 / 总线程数) × 100%

### 4. 任务添加优化
- **QToolButton设计**：单按钮支持单击和下拉菜单
- **批量添加**：支持5个、10个、20个任务快速添加
- **间隔添加**：支持自定义间隔时间，模拟真实任务到达
- **优雅实现**：使用 `QTimer::singleShot` 避免UI阻塞

### 5. 数据结构设计
- **字段分离**：`totalTimeMs` 在任务结构体，`curTimeMs` 在线程结构体
- **时间戳统一**：使用 `QTime::currentTime().msecsSinceStartOfDay()` 统一时间处理
- **快照机制**：UI只依赖快照数据，与业务逻辑彻底解耦

---

## 项目结构

```
ThreadPool/
├── main.cpp # 程序入口
├── mainwindow.cpp/h/ui # 主窗口及UI，性能指标计算
├── poolview.cpp/h # 可视化区域（自定义QGraphicsView）
├── threadpool.cpp/h # 线程池核心，性能指标统计
├── taskqueue.cpp/h # 任务队列，调度器集成
├── scheduler.cpp/h # 调度算法实现
├── visualinfo.h # 可视化快照结构体
└── ThreadPool.pro # Qt项目文件
```

---


---

## 软件设计图

### 1. 完整类图

```mermaid
classDiagram
    class MainWindow {
        -Ui::MainWindow* ui
        -ThreadPool* m_pool
        -int m_totalTasks
        -QMap~int,int~ m_taskIdToTotalTimeMs
        +on_startButton_clicked()
        +on_stopButton_clicked()
        +on_addTaskToolButton_clicked()
        +on_addTaskToolButton_triggered()
        +refreshAllUI()
        +addSingleTask()
        +setAddTaskMenu()
        +on_scheduleComboBox_currentIndexChanged()
    }

    class ThreadPool {
        -QMutex m_lock
        -QWaitCondition m_notEmpty
        -QList~WorkerThread*~ m_threads
        -TaskQueue* m_taskQ
        -ManagerThread* m_managerThread
        -int m_minNum, m_maxNum
        -int m_busyNum, m_aliveNum, m_exitNum
        -QList~TaskVisualInfo~ m_finishedTasks
        -int m_poolStartTimestamp
        -bool m_shutdown
        +addTask(Task)
        +getWaitingTaskNumber() int
        +getRunningTaskNumber() int
        +getFinishedTaskNumber() int
        +getTotalWaitingTimeMs() int
        +getTotalResponseRatio() double
        +getTotalTimeMs() int
        +setSchedulePolicy(SchedulePolicy)
        +getThreadVisualInfo() QList~ThreadVisualInfo~
        +getWaitingTaskVisualInfo() QList~TaskVisualInfo~
        +getFinishedTaskVisualInfo() QList~TaskVisualInfo~
        +threadStateChanged(int)
        +taskListChanged()
        +logMessage(QString)
    }

    class TaskQueue {
        -QMutex m_mutex
        -QList~Task~ m_queue
        -TaskScheduler* m_scheduler
        +addTask(Task)
        +takeTask() Task
        +getTasks() QList~Task~
        +setScheduler(TaskScheduler*)
        +taskNumber() int
        +clearQueue()
    }

    class TaskScheduler {
        <<abstract>>
        +insertByPolicy(QList~Task~&, Task) virtual
        +sortQueue(QList~Task~&) virtual
        +needsDynamicSort() virtual bool
    }

    class FIFOScheduler {
        +insertByPolicy(QList~Task~&, Task)
        +sortQueue(QList~Task~&)
    }

    class LIFOScheduler {
        +insertByPolicy(QList~Task~&, Task)
        +sortQueue(QList~Task~&)
    }

    class SJFScheduler {
        +insertByPolicy(QList~Task~&, Task)
        +sortQueue(QList~Task~&)
    }

    class LJFScheduler {
        +insertByPolicy(QList~Task~&, Task)
        +sortQueue(QList~Task~&)
    }

    class PRIOScheduler {
        +insertByPolicy(QList~Task~&, Task)
        +sortQueue(QList~Task~&)
    }

    class HRRNScheduler {
        +insertByPolicy(QList~Task~&, Task)
        +sortQueue(QList~Task~&)
        +needsDynamicSort() bool
    }

    class WorkerThread {
        -ThreadPool* m_pool
        -int m_id
        -int m_state
        -int m_curTaskId
        -int m_curTimeMs
        +run()
        +id() int
        +state() int
        +setState(int)
        +curTaskId() int
        +setCurTaskId(int)
        +curTimeMs() int
        +setCurTimeMs(int)
    }

    class ManagerThread {
        -ThreadPool* m_pool
        +run()
    }

    class Task {
        +int id
        +callback function
        +void* arg
        +int totalTimeMs
        +int priority
        +int arrivalTimestampMs
        +int finishTimestampMs
    }

    class TaskVisualInfo {
        +int taskId
        +int state
        +int curThreadId
        +int totalTimeMs
        +int priority
        +int arrivalTimestampMs
        +int finishTimestampMs
    }

    class ThreadVisualInfo {
        +int threadId
        +int state
        +int curTaskId
        +int curTimeMs
    }

    class PoolView {
        -QGraphicsScene* m_scene
        -SchedulePolicy m_currentPolicy
        -QMap~int,int~ m_taskIdToTotalTimeMs
        -QList~ThreadVisualInfo~ m_lastThreadInfos
        -QList~TaskVisualInfo~ m_lastWaitingTasks
        -QList~TaskVisualInfo~ m_lastFinishedTasks
        +visualizeAll(...)
        +drawThreads(...)
        +drawWaitingTasks(...)
        +drawFinishedTasks(...)
        +setCurrentPolicy(SchedulePolicy)
        +setTaskIdToTotalTimeMs(QMap~int,int~)
        +clear()
        +resizeEvent(QResizeEvent*)
    }

    MainWindow --> ThreadPool : uses
    MainWindow --> PoolView : uses
    ThreadPool --> TaskQueue : contains
    ThreadPool --> TaskScheduler : uses
    TaskQueue --> TaskScheduler : uses
    TaskScheduler <|-- FIFOScheduler
    TaskScheduler <|-- LIFOScheduler
    TaskScheduler <|-- SJFScheduler
    TaskScheduler <|-- LJFScheduler
    TaskScheduler <|-- PRIOScheduler
    TaskScheduler <|-- HRRNScheduler
    ThreadPool --> WorkerThread : manages
    ThreadPool --> ManagerThread : manages
    TaskQueue --> Task : contains
    WorkerThread --> ThreadPool : belongs to
    ManagerThread --> ThreadPool : belongs to
    PoolView --> TaskVisualInfo : displays
    PoolView --> ThreadVisualInfo : displays
    ThreadPool --> TaskVisualInfo : creates
    ThreadPool --> ThreadVisualInfo : creates
```

### 2. 系统架构图

```mermaid
graph TB
    subgraph "用户界面层"
        UI[MainWindow]
        PV[PoolView]
        UI --> PV
    end
    
    subgraph "业务逻辑层"
        TP[ThreadPool]
        TQ[TaskQueue]
        WT[WorkerThread]
        MT[ManagerThread]
        TP --> TQ
        TP --> WT
        TP --> MT
    end
    
    subgraph "调度算法层"
        TS[TaskScheduler]
        FIFO[FIFOScheduler]
        LIFO[LIFOScheduler]
        SJF[SJFScheduler]
        LJF[LJFScheduler]
        PRIO[PRIOScheduler]
        HRRN[HRRNScheduler]
        TS --> FIFO
        TS --> LIFO
        TS --> SJF
        TS --> LJF
        TS --> PRIO
        TS --> HRRN
    end
    
    subgraph "数据结构层"
        TASK[Task]
        TVI[TaskVisualInfo]
        THVI[ThreadVisualInfo]
    end
    
    UI --> TP
    TQ --> TS
    TP --> TASK
    TP --> TVI
    TP --> THVI
    WT --> TASK
```

### 3. 线程池生命周期图

```mermaid
stateDiagram-v2
    [*] --> 未启动
    未启动 --> 运行中 : 点击开始按钮
    运行中 --> 运行中 : 添加任务
    运行中 --> 运行中 : 任务执行
    运行中 --> 运行中 : 线程扩容
    运行中 --> 运行中 : 线程缩容
    运行中 --> 已停止 : 点击停止按钮
    已停止 --> [*] : 析构完成
    
    state 运行中 {
        [*] --> 空闲
        空闲 --> 忙碌 : 获取任务
        忙碌 --> 空闲 : 任务完成
        空闲 --> 退出 : 收到退出信号
        忙碌 --> 退出 : 收到退出信号
    }
```

### 4. 任务调度流程图

```mermaid
flowchart TD
    用户操作 --> 选择调度算法
    选择调度算法 --> 创建调度器实例
    创建调度器实例 --> 设置到任务队列
    
    任务到达 --> 添加到队列
    添加到队列 --> 调用insertByPolicy
    调用insertByPolicy --> 调用sortQueue
    调用sortQueue --> 按算法排序
    
    线程取任务 --> 检查是否需要动态排序
    检查是否需要动态排序 -->|HRRN| 重新计算响应比
    检查是否需要动态排序 -->|其他算法| 直接取队头
    重新计算响应比 --> 按响应比排序
    按响应比排序 --> 取队头任务
    直接取队头 --> 取队头任务
    取队头任务 --> 线程执行
    
    线程执行 --> 任务完成
    任务完成 --> 更新性能指标
    更新性能指标 --> UI刷新
    UI刷新 --> 可视化更新
```


## 5. HRRN算法详细流程图

```mermaid
flowchart TD
    A[任务到达] --> B[记录到达时间戳]
    B --> C[插入队列末尾]
    C --> D[计算当前响应比]
    D --> E[响应比公式: 等待时间 + 服务时间 / 服务时间]
    E --> F[按响应比降序排序]
    F --> G[等待执行]
    
    G --> H[线程请求任务]
    H --> I{检查调度器类型}
    I -->|HRRN| J[重新计算所有任务响应比]
    I -->|其他算法| K[直接取队头]
    
    J --> L[等待时间 = 当前时间 - 到达时间]
    L --> M[响应比 = 等待时间 + 服务时间 / 服务时间]
    M --> N[按响应比降序排序]
    N --> O[取队头任务]
    
    K --> O
    O --> P[线程执行任务]
    P --> Q[任务完成]
    Q --> R[记录完成时间戳]
    R --> S[计算实际等待时间]
    S --> T[更新性能统计]
```

## 6. 性能指标计算详细图

```mermaid
flowchart TD
    A[任务完成] --> B[记录完成时间戳]
    B --> C[计算等待时间: 完成时间 - 到达时间]
    C --> D[计算响应比: 等待时间 + 服务时间 / 服务时间]
    D --> E[添加到已完成任务列表]
    E --> F[更新统计]
    
    F --> G[平均等待时间计算]
    F --> H[平均响应比计算]
    F --> I[吞吐量计算]
    F --> J[CPU利用率计算]
    
    G --> K[总等待时间累加]
    K --> L[平均等待时间 = 总等待时间 / 已完成任务数]
    L --> M[转换为秒: 平均等待时间秒 = 平均等待时间毫秒 / 1000]
    
    H --> N[总响应比累加]
    N --> O[平均响应比 = 总响应比 / 已完成任务数]
    
    I --> P[获取线程池运行时间]
    P --> Q[吞吐量 = 已完成任务数 / 运行时间秒]
    
    J --> R[获取忙碌线程数]
    R --> S[获取总线程数]
    S --> T[CPU利用率 = 忙碌线程数 / 总线程数 * 100%]
    
    M --> U[UI显示]
    O --> U
    Q --> U
    T --> U
```


### 7. UI数据流图

```mermaid
flowchart LR
    subgraph "数据源"
        TP[ThreadPool]
        TQ[TaskQueue]
        WT[WorkerThread]
    end
    
    subgraph "数据转换"
        TVI[TaskVisualInfo]
        THVI[ThreadVisualInfo]
        PI[PerformanceInfo]
    end
    
    subgraph "UI组件"
        MW[MainWindow]
        PV[PoolView]
        TL[TaskLists]
        SL[StatisticsLabels]
    end
    
    subgraph "可视化"
        GS[QGraphicsScene]
        GR[QGraphicsRect]
        GT[QGraphicsText]
    end
    
    TP --> TVI
    TQ --> TVI
    WT --> THVI
    TP --> PI
    
    TVI --> MW
    THVI --> MW
    PI --> MW
    
    MW --> TL
    MW --> SL
    MW --> PV
    
    PV --> GS
    GS --> GR
    GS --> GT
```

### 8. 线程状态转换详细图

```mermaid
stateDiagram-v2
    [*] --> 创建 : new WorkerThread()
    创建 --> 启动 : thread->start()
    启动 --> 空闲 : 进入工作循环
    
    空闲 --> 等待任务 : 队列为空
    等待任务 --> 空闲 : 收到wakeOne信号
    等待任务 --> 退出 : 收到退出信号
    
    空闲 --> 忙碌 : takeTask()成功
    忙碌 --> 执行中 : 开始执行任务
    执行中 --> 执行中 : 更新进度(每100ms)
    执行中 --> 任务完成 : 执行完毕
    任务完成 --> 空闲 : 重置状态
    
    忙碌 --> 退出 : 收到退出信号
    空闲 --> 退出 : 收到退出信号
    
    退出 --> 销毁 : thread->wait()
    销毁 --> [*] : delete thread
```

### 9. 调度算法比较图

```mermaid
graph LR
    subgraph "静态算法"
        FIFO[FIFO<br/>先进先出<br/>按ID排序]
        LIFO[LIFO<br/>后进先出<br/>按ID倒序]
        SJF[SJF<br/>短作业优先<br/>按时间升序]
        LJF[LJF<br/>长作业优先<br/>按时间降序]
        PRIO[PRIO<br/>优先级优先<br/>按优先级降序]
    end
    
    subgraph "动态算法"
        HRRN[HRRN<br/>最高响应比<br/>动态计算排序]
    end
    
    subgraph "特点对比"
        A[静态算法<br/>排序依据固定<br/>性能稳定]
        B[动态算法<br/>排序依据变化<br/>防止饥饿]
    end
    
    静态算法 --> A
    动态算法 --> B
```

### 10. 内存管理图

```mermaid
graph TD
    subgraph "对象创建"
        MW[MainWindow创建]
        TP[ThreadPool创建]
        TQ[TaskQueue创建]
        WT[WorkerThread创建]
        MT[ManagerThread创建]
    end
    
    subgraph "对象关系"
        MW --> TP
        TP --> TQ
        TP --> WT
        TP --> MT
    end
    
    subgraph "对象销毁"
        MW销毁 --> TP销毁
        TP销毁 --> TQ销毁
        TP销毁 --> WT销毁
        TP销毁 --> MT销毁
    end
    
    subgraph "内存管理"
        QMutex[QMutex自动管理]
        QThread[QThread自动管理]
        QTimer[QTimer自动管理]
    end
```

### 11. 信号槽连接图

```mermaid
graph LR
    subgraph "信号源"
        TP[ThreadPool]
        WT[WorkerThread]
        MT[ManagerThread]
    end
    
    subgraph "信号类型"
        TSC[threadStateChanged]
        TLC[taskListChanged]
        LM[logMessage]
    end
    
    subgraph "槽函数"
        MW[MainWindow]
        PV[PoolView]
        LB[LogBrowser]
    end
    
    TP --> TSC
    WT --> TSC
    TP --> TLC
    TP --> LM
    MT --> LM
    
    TSC --> MW
    TLC --> MW
    LM --> LB
    
    MW --> PV
```

### 12. 可视化布局图

```mermaid
graph TB
    subgraph "PoolView布局"
        subgraph "等待队列区域"
            WQ[等待任务<br/>圆角矩形<br/>红色边框<br/>显示ID/时间/优先级/HR]
        end
        
        subgraph "线程池区域"
            TP[线程矩形<br/>进度条显示<br/>绿色=已完成<br/>红色=未完成]
        end
        
        subgraph "已完成区域"
            FT[已完成任务<br/>圆角矩形<br/>绿色边框<br/>显示ID和执行线程]
        end
    end
    
    subgraph "布局特性"
        A[自动换行]
        B[居中显示]
        C[紧凑排列]
        D[响应式布局]
    end
    
    WQ --> A
    TP --> B
    FT --> C
    A --> D
    B --> D
    C --> D
```

---

## UI界面说明

### 主要区域
- **线程池配置区**：线程数设置、调度算法选择、开始/停止按钮
- **任务添加区**：QToolButton支持单击添加单任务，下拉菜单支持批量添加
- **统计信息区**：线程状态、任务状态、性能指标（平均等待时间、响应比、吞吐量、CPU利用率）
- **任务队列区**：等待执行、正在执行、已完成任务列表
- **线程状态区**：工作线程、空闲线程列表
- **可视化区**：PoolView图形化展示线程池状态
- **运行日志区**：实时日志输出

### 可视化特性
- **等待队列**：圆角矩形，红色边框，显示任务ID和相关信息
- **线程池**：矩形进度条，绿色表示已完成，红色表示未完成
- **已完成任务**：圆角矩形，绿色边框，显示任务ID和执行线程
- **HRRN显示**：等待任务显示响应比(hr值)，实时更新

---

## 主要流程与原理

### 1. 调度算法切换
```cpp
// 运行时切换调度算法
m_pool->setSchedulePolicy(static_cast<SchedulePolicy>(index));
// 自动重新排序现有队列
m_scheduler->sortQueue(m_queue);
```

### 2. HRRN动态排序
```cpp
// 每次取任务前重新排序
if (m_scheduler && m_scheduler->needsDynamicSort()) {
    m_scheduler->sortQueue(m_queue);
}
```

### 3. 性能指标计算
```cpp
// 平均等待时间
avgWaitingTimeS = totalWaitingTimeMs / finishedTasks / 1000.0;

// 平均响应比
avgResponseRatio = totalResponseRatio / finishedTasks;

// 吞吐量
throughput = finishedTasks / (totalTimeMs / 1000.0);

// CPU利用率
cpuUtilization = (busyThreads / totalThreads) * 100.0;
```

### 4. 批量任务添加
```cpp
// 使用QTimer::singleShot实现间隔添加
for (int i = 1; i < count; ++i) {
    QTimer::singleShot(interval * i, this, [this]() {
        if (m_pool) {
            addSingleTask();
        }
    });
}
```

---

## 编译与运行

### 环境要求
- Qt 6.x
- C++17 及以上
- 支持多线程

### 编译步骤
1. 用 Qt Creator 打开 `ThreadPool.pro`
2. 配置编译环境
3. 编译并运行

### 使用说明
1. 设置线程数范围和调度算法
2. 点击"开始"启动线程池
3. 使用"添加任务"按钮添加任务
4. 观察实时统计信息和可视化效果
5. 可随时切换调度算法观察不同效果

---

## 扩展建议

### 功能扩展
- **更多调度算法**：时间片轮转(RR)、多级反馈队列(MLFQ)
- **任务管理**：任务取消、暂停、恢复
- **性能优化**：任务优先级动态调整、负载均衡
- **监控增强**：任务执行时间分布、队列长度统计

### 技术优化
- **内存管理**：智能指针替代原始指针
- **异常处理**：完善的错误处理和恢复机制
- **配置持久化**：保存和加载线程池配置
- **日志系统**：结构化日志，支持日志级别

---

## 结语

本项目成功实现了6种经典调度算法，特别是HRRN算法的动态排序特性，具备完整的性能监控和优雅的可视化界面。  
通过虚函数设计实现了调度器的优雅扩展，通过信号槽机制实现了UI与业务逻辑的彻底解耦。  
代码结构清晰，易于理解和扩展，适合教学演示和实际工程使用。  
欢迎二次开发和贡献！

---