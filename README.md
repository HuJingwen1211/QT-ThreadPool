---

# Qt线程池项目文档

## 项目概述

本项目是一个基于Qt框架实现的线程池系统，支持动态线程管理、任务调度、线程安全和可视化界面。采用Qt信号槽机制实现线程池与UI的实时联动，适合多线程任务并发处理和教学演示。

---

## 功能特性

- **动态线程管理**：支持最小/最大线程数配置，自动扩容和缩容
- **任务队列管理**：支持任务添加、执行、清空等操作
- **实时状态监控**：可视化显示线程状态、任务队列、运行日志
- **线程安全**：使用Qt的互斥锁和条件变量保证线程安全
- **信号槽机制**：线程池与UI解耦，支持跨线程通信

---

## 项目结构

```
ThreadPool/
├── main.cpp                 # 程序入口
├── mainwindow.cpp           # 主窗口实现
├── mainwindow.h             # 主窗口头文件
├── mainwindow.ui            # 主窗口UI设计文件
├── threadpool.cpp           # 线程池核心实现
├── threadpool.h             # 线程池头文件
├── taskqueue.cpp            # 任务队列实现
├── taskqueue.h              # 任务队列头文件
└── ThreadPool.pro           # Qt项目文件
```

---

## 软件设计图

### 1. 类图

```mermaid
classDiagram
    class MainWindow {
        -Ui::MainWindow* ui
        -ThreadPool* m_pool
        -int m_totalTasks
        +on_startButton_clicked()
        +on_stopButton_clicked()
        +on_addTaskButton_clicked()
        +on_clearQueueButton_clicked()
        +onTaskCompleted()
        +onTaskRemoved()
        +onThreadStateChanged(int)
        +onLogMessage(QString)
        +updateStatistics()
    }

    class ThreadPool {
        -QMutex m_lock
        -QWaitCondition m_notEmpty
        -QList~WorkerThread*~ m_threads
        -TaskQueue* m_taskQ
        -ManagerThread* m_managerThread
        -int m_minNum, m_maxNum
        -int m_busyNum, m_aliveNum, m_exitNum
        -int m_finishedTasks
        -bool m_shutdown
        +addTask(Task)
        +addTask(int, callback, void*)
        +getWaitingTaskNumber() int
        +getRunningTaskNumber() int
        +getFinishedTaskNumber() int
        +getBusyNumber() int
        +getAliveNumber() int
        +getThreadState(int) int
        +clearTaskQueue()
        +threadStateChanged(int)
        +taskCompleted(int)
        +taskRemoved()
        +logMessage(QString)
    }

    class TaskQueue {
        -QMutex m_mutex
        -QQueue~Task~ m_queue
        +addTask(Task)
        +addTask(int, callback, void*)
        +takeTask() Task
        +taskNumber() int
        +clearQueue()
        +taskAdded()
        +taskRemoved()
        +queueCleared()
    }

    class WorkerThread {
        -ThreadPool* m_pool
        -int m_id
        -int m_state
        +run()
        +id() int
        +state() int
        +setState(int)
    }

    class ManagerThread {
        -ThreadPool* m_pool
        +run()
    }

    class Task {
        +int id
        +callback function
        +void* arg
        +Task()
        +Task(int, callback, void*)
    }

    MainWindow --> ThreadPool : uses
    ThreadPool --> TaskQueue : contains
    ThreadPool --> WorkerThread : manages
    ThreadPool --> ManagerThread : manages
    TaskQueue --> Task : contains
    WorkerThread --> ThreadPool : belongs to
    ManagerThread --> ThreadPool : belongs to
```

---

### 2. 数据流图

```mermaid
graph TD
    A[用户界面] --> B[MainWindow]
    B --> C[ThreadPool]
    C --> D[TaskQueue]
    C --> E[WorkerThread]
    C --> F[ManagerThread]
    
    G[任务参数] --> H[Task对象]
    H --> D
    D --> E
    E --> I[任务执行]
    I --> J[任务完成]
    J --> K[内存释放]
    
    L[线程状态变化] --> M[信号发射]
    M --> N[UI更新]
    
    O[任务统计] --> P[统计信息]
    P --> Q[UI显示]
    
    R[线程管理] --> S[线程创建/销毁]
    S --> T[负载均衡]
```

---

### 3. 序列图（任务执行流程）

```mermaid
sequenceDiagram
    participant UI as MainWindow
    participant TP as ThreadPool
    participant TQ as TaskQueue
    participant WT as WorkerThread
    participant MT as ManagerThread

    UI->>TP: addTask(taskId, func, arg)
    TP->>TQ: addTask(task)
    TQ->>TQ: 加锁保护
    TQ->>TP: taskAdded信号
    TP->>WT: wakeOne()唤醒线程
    
    loop 工作线程循环
        WT->>TQ: takeTask()
        TQ->>WT: 返回任务
        WT->>WT: 设置忙碌状态
        WT->>TP: threadStateChanged信号
        WT->>WT: 执行任务函数
        WT->>WT: 释放任务参数内存
        WT->>TP: taskCompleted信号
        WT->>TP: threadStateChanged信号
    end
    
    loop 管理者线程循环
        MT->>TP: 检查负载
        alt 需要扩容
            MT->>TP: 创建新线程
        else 需要缩容
            MT->>TP: 销毁多余线程
        end
    end
```

---

### 4. 状态图（线程状态转换）

```mermaid
stateDiagram-v2
    [*] --> 空闲: 线程创建完成
    空闲 --> 忙碌: 获取到任务
    忙碌 --> 空闲: 任务执行完成
    空闲 --> 退出: 收到退出信号
    忙碌 --> 退出: 收到退出信号
    退出 --> [*]: 线程销毁
```

---

### 5. 组件图

```mermaid
graph TB
    subgraph "GUI层"
        A[MainWindow]
        B[UI界面]
    end
    
    subgraph "业务逻辑层"
        C[ThreadPool]
        D[TaskQueue]
    end
    
    subgraph "线程层"
        E[WorkerThread]
        F[ManagerThread]
    end
    
    subgraph "数据层"
        G[Task对象]
        H[线程状态]
    end
    
    A --> C
    B --> A
    C --> D
    C --> E
    C --> F
    D --> G
    E --> H
    F --> H
```

---

## 线程状态管理

- **0**：空闲（等待任务）
- **1**：忙碌（执行任务）
- **-1**：退出（线程已销毁）

状态流转：  
`创建线程 → 空闲(0) → 忙碌(1) → 空闲(0) → 退出(-1)`

---

## 动态线程管理

- **扩容**：任务数 > 存活线程数 且 存活线程数 < 最大线程数，每次最多创建2个线程，每5秒检查一次
- **缩容**：忙线程数 × 2 < 存活线程数 且 存活线程数 > 最小线程数，每次最多销毁2个线程

---

## 使用方法

1. 设置最小/最大线程数，点击“开始”启动线程池
2. 点击“添加任务”生成并添加任务
3. 实时监控线程状态、任务队列、统计信息和日志
4. 点击“清空队列”可清空等待任务
5. 点击“停止”安全关闭线程池

---

## 技术特点

- **线程安全**：QMutex/QWaitCondition保护共享资源
- **信号槽机制**：UI与线程池解耦，支持跨线程通信
- **性能优化**：延迟信号发射、智能线程管理
- **用户体验**：实时状态更新、可视化界面、详细日志

---

## 编译运行

- Qt 6.x，C++17及以上
- 用Qt Creator打开 `ThreadPool.pro`，配置编译环境，点击运行

---

## 扩展建议

- 支持自定义任务函数、任务优先级、任务取消/暂停
- 性能监控图表、配置持久化、异常处理、内存管理优化、多线程池支持

---

## 总结

本项目展示了Qt在并发编程中的强大能力，通过信号槽机制实现了线程池与UI的高效结合。代码结构清晰，功能完整，易于扩展和维护。

---