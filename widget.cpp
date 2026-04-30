/****************************************************************************
**
** Copyright (C) 2024
** DJS-103 Emulator Widget - Qt GUI Implementation
**
** 完整的 DJS-103 (M-3) 模拟器界面
** 支持 M3 文件格式: :AAAA(地址), =xxx(十进制小数), @AAAA(起始执行地址)
** 支持交互式输入对话框
**
****************************************************************************/

#include <QtWidgets>
#include <QTimer>
#include <QHeaderView>
#include <QFont>
#include <QSplitter>
#include <QMessageBox>
#include <QInputDialog>
#include <QRegularExpression>
#include <sstream>
#include <iomanip>

#include "widget.h"

// ============ DJS103Widget Implementation ============

DJS103Widget::DJS103Widget(QWidget *parent)
    : QWidget(parent)
    , m_isRunning(false)
    , m_runSpeed(1)
    , m_updatingMemory(false)
{
    m_emulator.setOutputCallback([this](const std::string &s) {
        appendOutput(s);
    });

    m_emulator.setInputCallback([this]() -> double {
        double val = 0.0;
        // This is called from the emulator thread context (timer tick),
        // so we need to use a synchronous dialog
        QMetaObject::invokeMethod(this, [this, &val]() {
            bool ok = false;
            val = QInputDialog::getDouble(this, tr("103机输入"),
                                          tr("请输入一个定点小数 (|x|<1):"),
                                          0.0, -1.0, 1.0, 10, &ok);
            if (!ok) val = 0.0;
        }, Qt::BlockingQueuedConnection);
        return val;
    });

    createUI();
}

void DJS103Widget::createUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout;

    // === Top: Register Display ===
    QGroupBox *regGroup = new QGroupBox(tr(""));
    QGridLayout *regLayout = new QGridLayout;

    QFont monoFont("Monospace", 10);
    QFont memFont("Monospace", 11, QFont::Bold);

    // === 103机物理控制面板 (含寄存器C/选存/启存) ===
    QWidget *frontPanel = createFrontPanel();
    regLayout->addWidget(frontPanel, 0, 0, 1, 3, Qt::AlignLeft | Qt::AlignTop);

    regGroup->setLayout(regLayout);

    mainLayout->addWidget(regGroup);

    // === Middle: Code Editor (left) + Output (middle) + Settings (right) ===
    QSplitter *midSplitter = new QSplitter(Qt::Horizontal);

    // -- Left: Code Editor --
    QGroupBox *codeGroup = new QGroupBox(tr("程序代码"));
    QVBoxLayout *codeLayout = new QVBoxLayout;

    m_codeEdit = new QPlainTextEdit;
    m_codeEdit->setFont(monoFont);
    m_codeEdit->setPlaceholderText(tr("在此输入103机程序可载入执行"));
    codeLayout->addWidget(m_codeEdit);

    // Example buttons
    QHBoxLayout *exampleLayout = new QHBoxLayout;
    m_exampleButton = new QPushButton(tr("示例: 加法"));
    m_example2Button = new QPushButton(tr("示例: 乘法"));
    m_example3Button = new QPushButton(tr("示例: sin(x)"));
    m_example4Button = new QPushButton(tr("示例: 鸡兔同笼"));
    exampleLayout->addWidget(m_exampleButton);
    exampleLayout->addWidget(m_example2Button);
    exampleLayout->addWidget(m_example3Button);
    exampleLayout->addWidget(m_example4Button);
    exampleLayout->addStretch();
    codeLayout->addLayout(exampleLayout);

    codeGroup->setLayout(codeLayout);
    midSplitter->addWidget(codeGroup);

    // -- Middle: Output Console --
    QGroupBox *outGroup = new QGroupBox(tr("输出控制台"));
    QVBoxLayout *outLayout = new QVBoxLayout;

    m_outputEdit = new QPlainTextEdit;
    m_outputEdit->setFont(monoFont);
    m_outputEdit->setReadOnly(true);
    outLayout->addWidget(m_outputEdit);

    outGroup->setLayout(outLayout);
    midSplitter->addWidget(outGroup);

    // -- Right: Settings & Help --
    QGroupBox *settingsGroup = new QGroupBox(tr("设置与帮助"));
    QVBoxLayout *settingsLayout = new QVBoxLayout;

    QHBoxLayout *speedLayout = new QHBoxLayout;
    speedLayout->addWidget(new QLabel(tr("速度:")));
    QComboBox *speedCombo = new QComboBox;
    speedCombo->addItem(tr("磁鼓存储器(30次/秒)"), 3);
    speedCombo->addItem(tr("磁芯存储器(1800次/秒)"), 180);
    speedCombo->setCurrentIndex(0);
    m_runSpeed = 3;
    speedLayout->addWidget(speedCombo);
    speedLayout->addStretch();
    settingsLayout->addLayout(speedLayout);

    m_helpButton = new QPushButton(tr("帮助"));
    settingsLayout->addWidget(m_helpButton);
    settingsLayout->addStretch();

    settingsGroup->setLayout(settingsLayout);
    midSplitter->addWidget(settingsGroup);

    midSplitter->setSizes(QList<int>() << 500 << 350 << 200);
    mainLayout->addWidget(midSplitter, 1);

    // === Bottom: Memory View ===
    QGroupBox *memGroup = new QGroupBox(tr("内存视图 (1024字)"));
    QVBoxLayout *memLayout = new QVBoxLayout;

    m_memTable = new QTableWidget(128, 9);
    m_memTable->setFont(memFont);
    m_memTable->horizontalHeader()->setDefaultSectionSize(120);
    m_memTable->setColumnWidth(0, 60);
    m_memTable->verticalHeader()->setDefaultSectionSize(20);

    // Set headers: column 0 = address, columns 1-8 = octal words
    QStringList headers;
    headers << tr("地址");
    for (int i = 0; i < 8; ++i)
        headers << QString("+%1").arg(i);
    m_memTable->setHorizontalHeaderLabels(headers);
    m_memTable->verticalHeader()->hide();
    m_memTable->horizontalHeader()->setStretchLastSection(true);
    m_memTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    memLayout->addWidget(m_memTable);
    memGroup->setLayout(memLayout);
    mainLayout->addWidget(memGroup, 1);

    setLayout(mainLayout);

    // === Connections ===
    connect(m_exampleButton, &QPushButton::clicked, this, &DJS103Widget::onLoadExample);
    connect(m_example2Button, &QPushButton::clicked, this, &DJS103Widget::onLoadExample2);
    connect(m_example3Button, &QPushButton::clicked, this, &DJS103Widget::onLoadExample3);
    connect(m_example4Button, &QPushButton::clicked, this, &DJS103Widget::onLoadExample4);
    connect(m_helpButton, &QPushButton::clicked, this, &DJS103Widget::onHelp);
    connect(m_memTable, &QTableWidget::cellChanged, this, &DJS103Widget::onMemoryCellChanged);
    connect(speedCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this, speedCombo]() {
        m_runSpeed = speedCombo->currentData().toInt();
    });

    // Front panel connections
    connect(m_fpStart, &QPushButton::clicked, this, &DJS103Widget::onFrontPanelStart);
    connect(m_fpSinglePulse, &QPushButton::clicked, this, &DJS103Widget::onFrontPanelSinglePulse);
    connect(m_fpClear0, &QPushButton::clicked, this, &DJS103Widget::onFrontPanelClear);
    connect(m_fpClear1, &QPushButton::clicked, this, &DJS103Widget::onFrontPanelClear);
    connect(m_fpClear2, &QPushButton::clicked, this, &DJS103Widget::onFrontPanelClear);
    connect(m_inputStart, &QPushButton::clicked, this, &DJS103Widget::onLoadProgram);
    connect(m_inputStop, &QPushButton::clicked, this, &DJS103Widget::onInputStop);
    connect(m_outputStart, &QPushButton::clicked, this, &DJS103Widget::onOutputStart);
    connect(m_outputStop, &QPushButton::clicked, this, &DJS103Widget::onOutputStop);
    connect(m_clearPulseDiv, &QPushButton::clicked, this, &DJS103Widget::onClearPulseDiv);
    connect(m_magRead, &QPushButton::clicked, this, &DJS103Widget::onMagMemoryRead);
    connect(m_magRecord, &QPushButton::clicked, this, &DJS103Widget::onMagMemoryRecord);

    // Run timer
    m_runTimer = new QTimer(this);
    connect(m_runTimer, &QTimer::timeout, this, &DJS103Widget::onRunTick);

    // Initialize display
    updateRegisterDisplay();
    updateMemoryDisplay();
}

void DJS103Widget::onLoadProgram()
{
    QString text = m_codeEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        appendOutput(tr("ERROR: 代码为空"));
        return;
    }

    m_emulator.reset();
    loadM3Program(text);
    updateRegisterDisplay();
    updateMemoryDisplay();
    appendOutput(tr("程序已装入, PC=%1 (八进制)")
                 .arg(m_emulator.getProgramCounter(), 4, 8, QChar('0')));
}

void DJS103Widget::onStep()
{
    if (m_emulator.isHalted()) {
        appendOutput(tr("已停机，请装入程序或复位"));
        return;
    }
    m_emulator.step();
    updateRegisterDisplay();
    updateMemoryDisplay();
}

void DJS103Widget::onRun()
{
    if (m_emulator.isHalted()) {
        appendOutput(tr("已停机，请装入程序或复位"));
        return;
    }

    m_isRunning = true;
    m_runTimer->start(100);
}

void DJS103Widget::onStop()
{
    m_runTimer->stop();
    m_isRunning = false;
    m_emulator.stop();

    updateRegisterDisplay();
    updateMemoryDisplay();
}

void DJS103Widget::onReset()
{
    m_runTimer->stop();
    m_isRunning = false;
    m_emulator.reset();

    updateRegisterDisplay();
    updateMemoryDisplay();
    appendOutput(tr("--- 复位 ---"));
}

void DJS103Widget::onLoadExample()
{
    // 示例: 计算 0.3 + 0.5 并打印结果
    //
    // 103机汇编手册格式: ±操作码 地址1 地址2
    // 操作码为两位八进制XY: X=修饰符, Y=操作(0加,1减,2除,3乘)
    //
    // +05: 传送 (addr1)->(addr2), r<-数据
    // +00: 加法 b,r=a+b  (X=0,Y=0: 结果存addr2和r)
    // +45: 传送并打印
    // +04: 停机
    //
    // 单元分配:
    // 0000-0003: 程序
    // 0004: =0.3
    // 0005: =0 (临时)
    // 0006: =0.5

    QString program =
        "; 示例: 计算 0.3 + 0.5 并打印结果\n"
        "; 汇编格式: ±操作码(八进制) 地址1 地址2\n"
        ";\n"
        "0000\n"
        "+05  0004  0005   ; 传送 [4]->[5]\n"
        "+00  0004  0006   ; 加 [4]+[6]->[6]\n"
        "+45  0006  0006   ; 打印 [6]\n"
        "+04  0000  0000   ; 停机\n"
        ";\n"
        "0004\n"
        "=0.3               ; 常数\n"
        "=0                 ; 临时\n"
        "=0.5               ; 常数\n"
        "@0000\n";

    m_codeEdit->setPlainText(program);
    appendOutput(tr("示例程序已载入: 加法 0.3 + 0.5\n点击\"装入程序\"后执行"));
}

void DJS103Widget::onLoadExample2()
{
    // 示例: 计算 0.5 * 0.5 = 0.25 并打印
    // +03: X=0,Y=3 乘法 b,r=a*b
    //
    // 单元分配:
    // 0000-0003: 程序
    // 0004: =0.5
    // 0005: =0 (结果)

    QString program =
        "; 示例: 计算 0.5 * 0.5 = 0.25 并打印结果\n"
        ";\n"
        "0000\n"
        "+05  0004  0005   ; 传送 [4]->[5]\n"
        "+03  0004  0005   ; 乘 [4]*[5]->[5]\n"
        "+45  0005  0005   ; 打印 [5]\n"
        "+04  0000  0000   ; 停机\n"
        ";\n"
        "0004\n"
        "=0.5               ; 常数\n"
        "=0                 ; 结果\n"
        "@0000\n";

    m_codeEdit->setPlainText(program);
    appendOutput(tr("示例程序已载入: 乘法 0.5 * 0.5\n点击\"装入程序\"后执行"));
}

void DJS103Widget::onLoadExample3()
{
    // 示例: 计算 sin x (来自103机汇编手册)
    //
    // 计算公式: 1/2 * sin(pi/2 * x) = (((((c11*x^2+c9)*x^2+c7)*x^2+c5)*x^2+c3)*x^2+c1)*x
    //
    // 单元分配:
    // 0001: 自变量 x
    // 0002: 函数结果
    // 0010--0035: 子程序(20条), 0030-0035为常数
    //
    // 常数:
    // c1 = +0.62 20 77 32 50
    // c3 = -0.24 52 73 63 50
    // c5 = +0.02 43 15 36 67
    // c7 = -0.00 11 45 53 25
    // c9 = +0.00 00 25 03 43
    // c11= -0.00 00 00 36 04

    QString program =
        "; 示例: 计算 1/2*sin(pi/2*x) (来自103机汇编手册)\n"
        "; 公式: (((((c11*x^2+c9)*x^2+c7)*x^2+c5)*x^2+c3)*x^2+c1)*x\n"
        ";\n"
        "; --- 数据区 (单元分配) ---\n"
        ":0001\n"
        "=0.8               ; 自变量 x\n"
        ";\n"
        ":0002\n"
        "=0                 ; 结果存放单元\n"
        ";\n"
        "; --- 子程序 ---\n"
        ":0010\n"
        "+13  0001  0001     ; 乘 [1]*[1]->r = x^2\n"
        "+24  0012  0002     ; 跳转 PC<-[12],r->[02]\n"
        "+33  0030  0000     ; 乘 r*[30]->r\n"
        "+30  0031  0000     ; 加 r+[31]->r\n"
        "+33  0002  0000     ; 乘 r*[02]->r\n"
        "+30  0032  0000     ; 加 r+[32]->r\n"
        "+33  0002  0000     ; 乘 r*[02]->r\n"
        "+30  0033  0000     ; 加 r+[33]->r\n"
        "+33  0002  0000     ; 乘 r*[02]->r\n"
        "+30  0034  0000     ; 加 r+[34]->r\n"
        "+33  0002  0000     ; 乘 r*[02]->r\n"
        "+30  0035  0000     ; 加 r+[35]->r\n"
        "+63  0001  0002     ; 乘 r*[01]->[02],r  打印结果\n"
        "+04  0000  0000     ; 停机\n"
        "; --- 常数区 ---\n"
        ":0030\n"
        "-00  0000  3604     ; c11\n"
        "+00  0025  0343     ; c9\n"
        "-00  1145  5325     ; c7\n"
        "+02  4315  3667     ; c5\n"
        "-24  5273  6350     ; c3\n"
        "+62  2077  3250     ; c1\n"
        "@0010               ; 程序入口\n\n";

    m_codeEdit->setPlainText(program);
    appendOutput(tr("示例程序已载入: sin(x) 子程序 (来自103机汇编手册)\n点击\"装入程序\"后执行"));
}

void DJS103Widget::onLoadExample4()
{
    // 今有雉兔同笼，上有三十五头，下有九十四足，问雉兔各几何？
    // 鸡兔同笼问题程序: 已知总头数h、总脚数f，求鸡x、兔y
    // 存储单元分配:
    // 0020: 常数 0.5 (用于计算 f/2)
    // 0021: 总头数 h (输入值，需除以100，如35→0.35)
    // 0022: 总脚数 f (输入值，需除以100，如94→0.94)
    // 0023: 临时单元 - 存储 f/2
    // 0024: 兔的数量 y (结果，除以100后的值)
    // 0025: 鸡的数量 x (结果，除以100后的值)

    QString program =
        "; 今有雉兔同笼，上有三十五头，下有九十四足，问雉兔各几何？\n"
        "; 鸡兔同笼问题程序: 已知总头数h、总脚数f，求鸡x、兔y\n"
        ";\n"
        ":0000                  ; 程序起始地址\n"
        "+05  0022  0023       ; 传送: f → 0023，同时 r ← f\n"
        "+03  0020  0023       ; 乘法: r(f) × 0020(0.5) → 0023和r (现在r=f/2)\n"
        "+21  0021  0024       ; 减法: r(f/2) - 0021(h) → 0024和r (现在r=y=f/2-h)\n"
        "+05  0021  0000       ; 传送: h → 0000(仅用于加载r)，r ← h\n"
        "+21  0024  0025       ; 减法: r(h) - 0024(y) → 0025和r   (现在r=x=h-y)\n"
        "+45  0025  0025       ; 打印: 输出鸡的数量 (除以100后的值)\n"
        "+45  0024  0024       ; 打印: 输出兔的数量 (除以100后的值)\n"
        "+04  0000  0000       ; 停机\n"
        ";\n"
        "; 常数区\n"
        ":0020\n"
        "=0.5                  ; 常数 0.5 (用于除以2)\n"
        "=0.35                 ; h 总头数 (输入值，需除以100，如35→0.35)\n"
        "=0.94                 ; f 总脚数 (输入值，需除以100，如94→0.94)\n"
        "=0                    ; 临时单元\n"
        "=0                    ; y 兔的数量 (结果，除以100后的值)\n"
        "=0                    ; x 鸡的数量 (结果，除以100后的值)\n"
        "@0000                 ; 程序入口地址\n";

    m_codeEdit->setPlainText(program);
    appendOutput(tr("示例程序已载入: 鸡兔同笼问题\n点击\"装入程序\"后执行"));
}

void DJS103Widget::onMemoryCellChanged(int row, int col)
{
    if (m_updatingMemory || col == 0)
        return;

    QTableWidgetItem *item = m_memTable->item(row, col);
    if (!item)
        return;

    QString text = item->text().trimmed();
    int32_t value;

    if (text.startsWith("=")) {
        // Decimal fraction input
        bool ok = false;
        double dval = text.mid(1).toDouble(&ok);
        if (!ok) return;
        value = DJS103Emulator::doubleToWord(dval);
    } else {
        bool ok = false;
        value = text.toLong(&ok, 8);
        if (!ok) {
            // Try decimal
            value = text.toInt(&ok);
            if (!ok) return;
        }
    }

    int addr = row * 8 + (col - 1);
    if (addr >= 0 && addr < DJS103Emulator::MEMORY_SIZE) {
        m_emulator.setMemory(addr, value);
    }
}

void DJS103Widget::onRunTick()
{
    for (int i = 0; i < m_runSpeed; ++i) {
        if (m_emulator.isHalted()) {
            onStop();
            return;
        }
        m_emulator.step();
    }
    updateRegisterDisplay();
    updateMemoryDisplay();
}

void DJS103Widget::updateRegisterDisplay()
{
    int32_t regC = m_emulator.getAccumulator();
    int32_t pc = m_emulator.getProgramCounter();
    double regCValue = m_emulator.getAccumulatorValue();


    // Update regC LED display
    updateRegCLedDisplay(regC);

    // 在输出控制台打印累加器的值
    appendOutput(tr("累加器 C: %1, %2").arg(regC & DJS103Emulator::WORD_MASK, 11, 8, QChar('0'))
                 .arg(regCValue));

    // Update 操部 (opcode) LED display - 6 bits
    {
        int opcode = m_emulator.getLastOpcode();
        for (int i = 0; i < 6; ++i) {
            int bitPos = 5 - i;  // 反转位序：i=0对应最高位bit5，i=5对应最低位bit0
            bool bitOn = (opcode >> bitPos) & 1;
            if (bitOn) {
                m_caobuLeds[i]->setStyleSheet(
                    "QLabel { background-color: qradialgradient(cx:0.5, cy:0.5, radius:0.5, "
                    "fx:0.35, fy:0.35, stop:0 #ffcc77, stop:0.3 #ff8a4a, "
                    "stop:0.7 #ff6a3d, stop:1 #cc4400); "
                    "border: 1px solid #ffaa66; border-radius: 7px; }");
            } else {
                m_caobuLeds[i]->setStyleSheet(
                    "QLabel { background-color: #3a3a3a; border: 1px solid #555555; "
                    "border-radius: 7px; }");
            }
        }
    }

    // Update Select Memory LED display with PC value (12 bits)
    for (int i = 0; i < SELECT_MEMORY_BITS; ++i) {
        bool bitOn = (pc >> i) & 1;
        if (bitOn) {
            m_selectMemoryLeds[i]->setStyleSheet(
                "QLabel { background-color: qradialgradient(cx:0.5, cy:0.5, radius:0.5, "
                "fx:0.35, fy:0.35, stop:0 #ffcc77, stop:0.3 #ff8a4a, "
                "stop:0.7 #ff6a3d, stop:1 #cc4400); "
                "border: 1px solid #ffaa66; border-radius: 7px; }");
        } else {
            m_selectMemoryLeds[i]->setStyleSheet(
                "QLabel { background-color: #3a3a3a; border: 1px solid #555555; "
                "border-radius: 7px; }");
        }
    }

    // Update Start Memory LED display with entry address (12 bits)
    int32_t entryAddr = m_emulator.getEntryAddress();
    for (int i = 0; i < START_MEMORY_BITS; ++i) {
        bool bitOn = (entryAddr >> i) & 1;
        if (bitOn) {
            m_startMemoryLeds[i]->setStyleSheet(
                "QLabel { background-color: qradialgradient(cx:0.5, cy:0.5, radius:0.5, "
                "fx:0.35, fy:0.35, stop:0 #ffcc77, stop:0.3 #ff8a4a, "
                "stop:0.7 #ff6a3d, stop:1 #cc4400); "
                "border: 1px solid #ffaa66; border-radius: 7px; }");
        } else {
            m_startMemoryLeds[i]->setStyleSheet(
                "QLabel { background-color: #3a3a3a; border: 1px solid #555555; "
                "border-radius: 7px; }");
        }
    }

    highlightCurrentLine();
}

void DJS103Widget::updateRegCLedDisplay(int32_t value)
{
    int32_t regCLedBits = value & DJS103Emulator::WORD_MASK;
    for (int i = 0; i < NUM_LEDS; ++i) {
        bool bitOn = (regCLedBits >> i) & 1;
        if (bitOn) {
            m_regCLeds[i]->setStyleSheet(
                "QLabel { background-color: qradialgradient(cx:0.5, cy:0.5, radius:0.5, "
                "fx:0.35, fy:0.35, stop:0 #ffcc77, stop:0.3 #ff8a4a, "
                "stop:0.7 #ff6a3d, stop:1 #cc4400); "
                "border: 1px solid #ffaa66; border-radius: 7px; }");
        } else {
            m_regCLeds[i]->setStyleSheet(
                "QLabel { background-color: #3a3a3a; border: 1px solid #555555; "
                "border-radius: 7px; }");
        }
    }
}

/**
 * @brief 创建寄存器C的LED显示和开关控制面板
 * @return 包含LED显示和开关控制的QWidget
 *
 * 寄存器C共有31位（位0-位30），分为3组：7位、12位、12位
 * 每位包含：LED指示灯（显示当前值）+ 开关按钮（可切换值）
 * 布局采用每3位一组，便于查看八进制/二进制值
 * 每组3位对应的权重标签为：4、2、1（高位到低位）
 */
QWidget* DJS103Widget::createRegCLedDisplay()
{
    static const int regCLedGroups[] = {7, 12, 12};
    static const int regCNumGroups = 3;

    QGroupBox *regCLedFrame = new QGroupBox(tr("寄存器 C"));
    regCLedFrame->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    regCLedFrame->setStyleSheet(
        "QGroupBox {"
        "  color: #333; border: 1px solid #bbb; border-radius: 6px;"
        "  margin-top: 10px; font-size: 12px; font-weight: bold;"
        "}"
        "QGroupBox::title { subcontrol-origin: margin;  left: 45%; padding: 0 4px; }"
    );

    // 主垂直布局
    QVBoxLayout *regCLedVLayout = new QVBoxLayout(regCLedFrame);

    // 外层水平布局：包含位显示区域 + 弹性空间
    QHBoxLayout *regCLedOuterLayout = new QHBoxLayout;
    // 内层水平布局：放置各分组（组间距20px）
    QHBoxLayout *regCLedRowLayout = new QHBoxLayout;
    regCLedRowLayout->setSpacing(20);

    // 从最高位（位30）开始处理
    int regCBitIndex = 30;
    // 遍历每个分组（7位组、12位组、12位组）
    for (int g = 0; g < regCNumGroups; ++g) {
        int groupSize = regCLedGroups[g];  // 当前组的位数

        // 每组创建一个垂直布局：LED行 + 间距 + Switch行
        QVBoxLayout *groupVLayout = new QVBoxLayout;
        groupVLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        groupVLayout->setSpacing(2);  // LED和Switch行内间距

        // LED指示灯水平布局（显示当前位值），每3位一组
        QHBoxLayout *ledHLayout = new QHBoxLayout;
        ledHLayout->setSpacing(2);  // LED之间的间距
        ledHLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // 开关按钮水平布局（用于设置位值），与LED对齐
        QHBoxLayout *switchHLayout = new QHBoxLayout;
        switchHLayout->setSpacing(2);  // 开关之间的间距
        switchHLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        int bitPos = 0;  // 当前组内已处理的位数
        // 计算余数：如果组大小不是3的倍数，最高位的余数单独处理
        int remaining = groupSize % 3;

        // 先处理可能单独的最高位（余数位）
        if (remaining > 0) {
            // 创建余数位的LED指示灯（1位或2位），从高位到低位排列
            for (int j = 0; j < remaining; ++j) {
                int i = regCBitIndex - j;  // 位索引（从高位开始）
                m_regCLeds[i] = new QLabel;
                m_regCLeds[i]->setFixedSize(14, 14);  // 14x14像素的方形LED
                m_regCLeds[i]->setAlignment(Qt::AlignCenter);
                m_regCLeds[i]->setMargin(0);
                m_regCLeds[i]->setToolTip(tr("位 %1").arg(i));  // 鼠标悬停显示位号
                ledHLayout->addWidget(m_regCLeds[i]);
            }

            // 创建余数位对应的开关按钮（带权重标签）
            for (int j = 0; j < remaining; ++j) {
                int i = regCBitIndex - j;
                // 权重计算：如果只有1位权重为1，如果有2位则第一位权重为2
                int weight = (remaining == 1) ? 1 : (j == 0 ? 2 : 1);

                // 创建开关容器（垂直布局：权重标签 + 按钮）
                QWidget *switchContainer = new QWidget;
                QVBoxLayout *switchVLayout = new QVBoxLayout(switchContainer);
                switchVLayout->setSpacing(1);  // 标签和按钮之间的间距
                switchVLayout->setContentsMargins(0, 0, 0, 0);  // 无外边距
                switchVLayout->setAlignment(Qt::AlignCenter);

                // 权重标签（显示该位的二进制权重）
                QLabel *weightLabel = new QLabel(QString::number(weight));
                weightLabel->setFixedSize(14, 10);  // 小号标签
                weightLabel->setAlignment(Qt::AlignCenter);
                weightLabel->setFont(QFont("Monospace", 6));  // 6号等宽字体
                switchVLayout->addWidget(weightLabel);

                // 开关按钮（可切换状态，模拟开关）
                m_regCSwitches[i] = new QPushButton;
                m_regCSwitches[i]->setFixedSize(14, 14);  // 与LED相同大小
                m_regCSwitches[i]->setCheckable(true);  // 可切换状态
                m_regCSwitches[i]->setChecked(false);  // 默认关闭
                m_regCSwitches[i]->setToolTip(tr("位 %1 开关").arg(i));
                // 样式：灰色未选中，绿色选中
                m_regCSwitches[i]->setStyleSheet(
                    "QPushButton { background-color: #ddd; border: 1px solid #999; border-radius: 7px; }"
                    "QPushButton:checked { background-color: #4CAF50; border: 1px solid #388E3C; }"
                );
                switchVLayout->addWidget(m_regCSwitches[i]);

                switchHLayout->addWidget(switchContainer);
            }
            bitPos = remaining;  // 更新已处理的位数

            // 在余数位和后续的3位组之间添加间距
            ledHLayout->addSpacing(4);
            switchHLayout->addSpacing(4);
        }

        // 处理完整的3位组（每3位一组，方便查看八进制/二进制值）
        int numFullFrames = groupSize / 3;
        for (int f = 0; f < numFullFrames; ++f) {
            // 创建3个LED指示灯（高位到低位：位2、位1、位0）
            for (int j = 0; j < 3; ++j) {
                int i = regCBitIndex - bitPos - j;  // 计算当前位索引
                m_regCLeds[i] = new QLabel;
                m_regCLeds[i]->setFixedSize(14, 14);  // 14x14像素方形LED
                m_regCLeds[i]->setAlignment(Qt::AlignCenter);
                m_regCLeds[i]->setMargin(0);
                m_regCLeds[i]->setToolTip(tr("位 %1").arg(i));  // 鼠标悬停显示位号
                ledHLayout->addWidget(m_regCLeds[i]);
            }

            // 创建3个对应的开关按钮（带权重标签：4、2、1）
            for (int j = 0; j < 3; ++j) {
                int i = regCBitIndex - bitPos - j;
                // 权重计算：最高位=4，中间位=2，最低位=1（二进制4-2-1码）
                int weight = 1 << (2 - j);

                // 创建开关容器（垂直布局：权重标签在上，按钮在下）
                QWidget *switchContainer = new QWidget;
                QVBoxLayout *switchVLayout = new QVBoxLayout(switchContainer);
                switchVLayout->setSpacing(1);  // 标签和按钮间距1px
                switchVLayout->setContentsMargins(0, 0, 0, 0);  // 无外边距
                switchVLayout->setAlignment(Qt::AlignCenter);

                // 权重标签（4、2、1）
                QLabel *weightLabel = new QLabel(QString::number(weight));
                weightLabel->setFixedSize(14, 10);  // 小号标签与按钮同宽
                weightLabel->setAlignment(Qt::AlignCenter);
                weightLabel->setFont(QFont("Monospace", 6));  // 6号等宽字体
                switchVLayout->addWidget(weightLabel);

                // 开关按钮（可切换状态）
                m_regCSwitches[i] = new QPushButton;
                m_regCSwitches[i]->setFixedSize(14, 14);  // 与LED相同大小
                m_regCSwitches[i]->setCheckable(true);  // 可切换状态
                m_regCSwitches[i]->setChecked(false);  // 默认关闭
                m_regCSwitches[i]->setToolTip(tr("位 %1 开关").arg(i));
                // 样式：未选中灰色，选中绿色
                m_regCSwitches[i]->setStyleSheet(
                    "QPushButton { background-color: #ddd; border: 1px solid #999; border-radius: 7px; }"
                    "QPushButton:checked { background-color: #4CAF50; border: 1px solid #388E3C; }"
                );
                switchVLayout->addWidget(m_regCSwitches[i]);

                switchHLayout->addWidget(switchContainer);
            }
            bitPos += 3;  // 更新已处理的位数

            // 在3位组之间添加小间距（最后一组不加）
            if (f < numFullFrames - 1) {
                ledHLayout->addSpacing(4);
                switchHLayout->addSpacing(4);
            }
        }
        regCBitIndex -= groupSize;  // 更新到下一组的起始位索引

        // 将LED行和Switch行添加到组垂直布局（中间加15px垂直间距）
        groupVLayout->addLayout(ledHLayout);
        groupVLayout->addSpacing(15);  // LED和Switch之间的垂直间距
        groupVLayout->addLayout(switchHLayout);
        // 将当前组（包含LED和Switch）添加到行布局
        regCLedRowLayout->addLayout(groupVLayout);
    }

    // 将行布局添加到外层布局，并添加右侧弹性空间（右对齐效果）
    regCLedOuterLayout->addLayout(regCLedRowLayout);
    regCLedOuterLayout->addStretch(1);  // 右侧留白，使内容左对齐
    regCLedVLayout->addLayout(regCLedOuterLayout);

    return regCLedFrame;  // 返回完整的寄存器C显示控件
}

QWidget* DJS103Widget::createSelectMemoryDisplay()
{
    QGroupBox *selectMemoryFrame = new QGroupBox(tr("选存"));
    selectMemoryFrame->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    selectMemoryFrame->setStyleSheet(
        "QGroupBox {"
        "  color: #333; border: 1px solid #bbb; border-radius: 6px;"
        "  margin-top: 10px; font-size: 12px; font-weight: bold;"
        "}"
        "QGroupBox::title { subcontrol-origin: margin;  left: 45%; padding: 0 4px; }"
    );

    // 主垂直布局
    QVBoxLayout *selectMemoryVLayout = new QVBoxLayout(selectMemoryFrame);

    // 外层水平布局：包含位显示区域 + 弹性空间
    QHBoxLayout *selectMemoryOuterLayout = new QHBoxLayout;

    // 内层垂直布局：放置LED行和Switch行
    QVBoxLayout *bitsVLayout = new QVBoxLayout;
    bitsVLayout->setSpacing(2);  // LED和Switch之间的垂直间距

    // LED行水平布局（连续显示，每3位之间加小间距）
    QHBoxLayout *ledHLayout = new QHBoxLayout;
    ledHLayout->setSpacing(2);
    ledHLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 开关行水平布局
    QHBoxLayout *switchHLayout = new QHBoxLayout;
    switchHLayout->setSpacing(2);
    switchHLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 12位，位索引从11到0，连续显示
    for (int i = 11; i >= 0; --i) {
        // 每3位一组计算权重（4,2,1循环）
        int posInGroup = (11 - i) % 3;
        int weight = (posInGroup == 0) ? 4 : (posInGroup == 1 ? 2 : 1);

        // LED指示灯
        m_selectMemoryLeds[i] = new QLabel;
        m_selectMemoryLeds[i]->setFixedSize(14, 14);
        m_selectMemoryLeds[i]->setAlignment(Qt::AlignCenter);
        m_selectMemoryLeds[i]->setMargin(0);
        m_selectMemoryLeds[i]->setStyleSheet(
            "QLabel { background-color: #3a3a3a; border: 1px solid #555555; border-radius: 7px; }");
        m_selectMemoryLeds[i]->setToolTip(tr("选存位 %1").arg(i));
        ledHLayout->addWidget(m_selectMemoryLeds[i]);

        // 开关容器（垂直布局：权重标签 + 按钮）
        QWidget *switchContainer = new QWidget;
        QVBoxLayout *switchVLayout = new QVBoxLayout(switchContainer);
        switchVLayout->setSpacing(1);
        switchVLayout->setContentsMargins(0, 0, 0, 0);
        switchVLayout->setAlignment(Qt::AlignCenter);

        // 权重标签
        QLabel *weightLabel = new QLabel(QString::number(weight));
        weightLabel->setFixedSize(14, 10);
        weightLabel->setAlignment(Qt::AlignCenter);
        weightLabel->setFont(QFont("Monospace", 6));
        switchVLayout->addWidget(weightLabel);

        // 开关按钮
        m_selectMemorySwitches[i] = new QPushButton;
        m_selectMemorySwitches[i]->setFixedSize(14, 14);
        m_selectMemorySwitches[i]->setCheckable(true);
        m_selectMemorySwitches[i]->setChecked(false);
        m_selectMemorySwitches[i]->setToolTip(tr("选存位 %1 开关").arg(i));
        m_selectMemorySwitches[i]->setStyleSheet(
            "QPushButton { background-color: #ddd; border: 1px solid #999; border-radius: 7px; }"
            "QPushButton:checked { background-color: #4CAF50; border: 1px solid #388E3C; }"
        );
        switchVLayout->addWidget(m_selectMemorySwitches[i]);

        switchHLayout->addWidget(switchContainer);

        // 每3位之间加小间距（最后一组不加）
        if (i > 0 && (11 - i) % 3 == 2) {
            ledHLayout->addSpacing(4);
            switchHLayout->addSpacing(4);
        }
    }

    // 将LED行和Switch行添加到垂直布局
    bitsVLayout->addLayout(ledHLayout);
    bitsVLayout->addSpacing(15);  // LED和Switch之间的垂直间距
    bitsVLayout->addLayout(switchHLayout);

    // 添加到外层布局，并添加右侧弹性空间
    selectMemoryOuterLayout->addLayout(bitsVLayout);
    selectMemoryOuterLayout->addStretch(1);
    selectMemoryVLayout->addLayout(selectMemoryOuterLayout);

    return selectMemoryFrame;
}

QWidget* DJS103Widget::createStartMemoryDisplay()
{
    QGroupBox *startMemoryFrame = new QGroupBox(tr("启存"));
    startMemoryFrame->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    startMemoryFrame->setStyleSheet(
        "QGroupBox {"
        "  color: #333; border: 1px solid #bbb; border-radius: 6px;"
        "  margin-top: 10px; font-size: 12px; font-weight: bold;"
        "}"
        "QGroupBox::title { subcontrol-origin: margin;  left: 45%; padding: 0 4px; }"
    );

    // 主垂直布局
    QVBoxLayout *startMemoryVLayout = new QVBoxLayout(startMemoryFrame);

    // 外层水平布局：包含位显示区域 + 弹性空间
    QHBoxLayout *startMemoryOuterLayout = new QHBoxLayout;

    // 内层垂直布局：放置LED行和Switch行
    QVBoxLayout *bitsVLayout = new QVBoxLayout;
    bitsVLayout->setSpacing(2);  // LED和Switch之间的垂直间距

    // LED行水平布局（连续显示，每3位之间加小间距）
    QHBoxLayout *ledHLayout = new QHBoxLayout;
    ledHLayout->setSpacing(2);
    ledHLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 开关行水平布局
    QHBoxLayout *switchHLayout = new QHBoxLayout;
    switchHLayout->setSpacing(2);
    switchHLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 12位，位索引从11到0，连续显示
    for (int i = 11; i >= 0; --i) {
        // 每3位一组计算权重（4,2,1循环）
        int posInGroup = (11 - i) % 3;
        int weight = (posInGroup == 0) ? 4 : (posInGroup == 1 ? 2 : 1);

        // LED指示灯
        m_startMemoryLeds[i] = new QLabel;
        m_startMemoryLeds[i]->setFixedSize(14, 14);
        m_startMemoryLeds[i]->setAlignment(Qt::AlignCenter);
        m_startMemoryLeds[i]->setMargin(0);
        m_startMemoryLeds[i]->setStyleSheet(
            "QLabel { background-color: #3a3a3a; border: 1px solid #555555; border-radius: 7px; }");
        m_startMemoryLeds[i]->setToolTip(tr("启存位 %1").arg(i));
        ledHLayout->addWidget(m_startMemoryLeds[i]);

        // 开关容器（垂直布局：权重标签 + 按钮）
        QWidget *switchContainer = new QWidget;
        QVBoxLayout *switchVLayout = new QVBoxLayout(switchContainer);
        switchVLayout->setSpacing(1);
        switchVLayout->setContentsMargins(0, 0, 0, 0);
        switchVLayout->setAlignment(Qt::AlignCenter);

        // 权重标签
        QLabel *weightLabel = new QLabel(QString::number(weight));
        weightLabel->setFixedSize(14, 10);
        weightLabel->setAlignment(Qt::AlignCenter);
        weightLabel->setFont(QFont("Monospace", 6));
        switchVLayout->addWidget(weightLabel);

        // 开关按钮
        m_startMemorySwitches[i] = new QPushButton;
        m_startMemorySwitches[i]->setFixedSize(14, 14);
        m_startMemorySwitches[i]->setCheckable(true);
        m_startMemorySwitches[i]->setChecked(false);
        m_startMemorySwitches[i]->setToolTip(tr("启存位 %1 开关").arg(i));
        m_startMemorySwitches[i]->setStyleSheet(
            "QPushButton { background-color: #ddd; border: 1px solid #999; border-radius: 7px; }"
            "QPushButton:checked { background-color: #4CAF50; border: 1px solid #388E3C; }"
        );
        switchVLayout->addWidget(m_startMemorySwitches[i]);

        switchHLayout->addWidget(switchContainer);

        // 每3位之间加小间距（最后一组不加）
        if (i > 0 && (11 - i) % 3 == 2) {
            ledHLayout->addSpacing(4);
            switchHLayout->addSpacing(4);
        }
    }

    // 将LED行和Switch行添加到垂直布局
    bitsVLayout->addLayout(ledHLayout);
    bitsVLayout->addSpacing(15);  // LED和Switch之间的垂直间距
    bitsVLayout->addLayout(switchHLayout);

    // 添加到外层布局，并添加右侧弹性空间
    startMemoryOuterLayout->addLayout(bitsVLayout);
    startMemoryOuterLayout->addStretch(1);
    startMemoryVLayout->addLayout(startMemoryOuterLayout);

    return startMemoryFrame;
}

void DJS103Widget::updateMemoryDisplay()
{
    m_updatingMemory = true;

    int pc = m_emulator.getProgramCounter();
    for (int row = 0; row < 128; ++row) {
        int baseAddr = row * 8;

        // Address column
        QTableWidgetItem *addrItem = m_memTable->item(row, 0);
        if (!addrItem) {
            addrItem = new QTableWidgetItem;
            m_memTable->setItem(row, 0, addrItem);
        }
        addrItem->setText(QString("%1").arg(baseAddr, 4, 8, QChar('0')));
        addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsEditable);
        addrItem->setBackground(QColor(230, 230, 230));

        // Highlight current PC row
        QFont f = addrItem->font();
        bool isPcRow = (pc >= baseAddr && pc < baseAddr + 8);
        f.setBold(isPcRow);
        addrItem->setFont(f);

        // Memory data columns
        for (int col = 1; col <= 8; ++col) {
            int addr = baseAddr + (col - 1);
            QTableWidgetItem *item = m_memTable->item(row, col);
            if (!item) {
                item = new QTableWidgetItem;
                m_memTable->setItem(row, col, item);
            }

            int32_t val = m_emulator.getMemory(addr);
            item->setText(QString("%1").arg(val & DJS103Emulator::WORD_MASK, 11, 8, QChar('0')));

            // Highlight PC address
            QFont cf = item->font();
            cf.setBold(addr == pc);
            item->setFont(cf);

            if (addr == pc) {
                item->setBackground(QColor(200, 255, 200));
            } else if (val != 0) {
                item->setBackground(QColor(255, 255, 230));
                item->setForeground(QColor(180, 0, 0));
            } else {
                item->setBackground(Qt::white);
                item->setForeground(Qt::black);
            }

            // Tooltip showing decimal value (smart precision)
            double dval = DJS103Emulator::wordToDouble(val);
            QString dvalStr;
            double absDval = fabs(dval);
            if (absDval == 0.0) {
                dvalStr = "0";
            } else {
                for (int prec = 1; prec <= 10; ++prec) {
                    QString test = QString::number(dval, 'f', prec);
                    if (fabs(test.toDouble() - dval) < 1e-12) {
                        dvalStr = test;
                        break;
                    }
                }
                if (dvalStr.isEmpty())
                    dvalStr = QString::number(dval, 'f', 10);
            }
            item->setToolTip(tr("八进制: %1\n十进制小数: %2")
                             .arg(val & DJS103Emulator::WORD_MASK, 11, 8, QChar('0'))
                             .arg(dvalStr));
        }
    }

    m_updatingMemory = false;
}

void DJS103Widget::highlightCurrentLine()
{
    int pc = m_emulator.getProgramCounter();
    QTextCursor cursor = m_codeEdit->textCursor();
    QTextCharFormat defaultFmt;

    // Clear all previous highlighting
    QTextDocument *doc = m_codeEdit->document();
    QList<QTextEdit::ExtraSelection> selections;

    // Highlight the line corresponding to current PC
    if (m_addrToLine.contains(pc)) {
        int lineNum = m_addrToLine[pc];
        QTextBlock block = doc->findBlockByNumber(lineNum);
        if (block.isValid()) {
            QTextEdit::ExtraSelection sel;
            sel.cursor = QTextCursor(block);
            sel.cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            sel.format.setBackground(QColor(255, 220, 80));  // 黄色高亮
            sel.format.setForeground(Qt::black);
            selections.append(sel);

            // Scroll to make the highlighted line visible
            m_codeEdit->setTextCursor(sel.cursor);
        }
    }

    // Also highlight any other lines that map to the same PC (shouldn't happen normally,
    // but handle edge case where multiple source lines produce same address)
    m_codeEdit->setExtraSelections(selections);
}

void DJS103Widget::appendOutput(const QString &text)
{
    m_outputEdit->appendPlainText(text);
}

void DJS103Widget::appendOutput(const std::string &text)
{
    appendOutput(QString::fromStdString(text));
}

void DJS103Widget::loadM3Program(const QString &text)
{
    QStringList lines = text.split('\n');
    int currentAddr = 0;
    int startAddr = 0;
    int execAddr = -1;
    std::vector<int32_t> program(DJS103Emulator::MEMORY_SIZE, 0);
    std::vector<bool> loaded(DJS103Emulator::MEMORY_SIZE, false);
    m_addrToLine.clear();

    for (int lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
        const QString &line = lines[lineIdx];
        QString trimmed = line.trimmed();

        // Skip empty lines and comments
        if (trimmed.isEmpty() || trimmed.startsWith(';'))
            continue;

        // :AAAA - set current address
        if (trimmed.startsWith(':')) {
            QString addrStr = trimmed.mid(1);
            int semiPos = addrStr.indexOf(';');
            if (semiPos >= 0)
                addrStr = addrStr.left(semiPos).trimmed();
            bool ok = false;
            int addr = addrStr.toInt(&ok, 8);
            if (ok) {
                currentAddr = addr % DJS103Emulator::MEMORY_SIZE;
                if (startAddr == 0 && execAddr < 0)
                    startAddr = currentAddr;
            }
            continue;
        }

        // @AAAA - set execution start address
        if (trimmed.startsWith('@')) {
            QString addrStr = trimmed.mid(1);
            int semiPos = addrStr.indexOf(';');
            if (semiPos >= 0)
                addrStr = addrStr.left(semiPos).trimmed();
            bool ok = false;
            int addr = addrStr.toInt(&ok, 8);
            if (ok) {
                execAddr = addr % DJS103Emulator::MEMORY_SIZE;
                m_emulator.setEntryAddress(addr);
            }
            continue;
        }

        // =xxx - decimal fraction value
        if (trimmed.startsWith('=')) {
            // Remove inline comments before parsing the number
            QString valueStr = trimmed.mid(1);
            int semiPos = valueStr.indexOf(';');
            if (semiPos >= 0)
                valueStr = valueStr.left(semiPos).trimmed();
            bool ok = false;
            double val = valueStr.toDouble(&ok);
            if (ok) {
                int32_t word = DJS103Emulator::doubleToWord(val);
                if (currentAddr < DJS103Emulator::MEMORY_SIZE) {
                    program[currentAddr] = word;
                    loaded[currentAddr] = true;
                    m_addrToLine[currentAddr] = lineIdx;
                    currentAddr++;
                }
            }
            continue;
        }

        // Try to parse instruction
        // Supported formats:
        //   Manual format (from assembly handbook): "+05 0004 0005" or "-05 0004 0005"
        //   M3 format: "S OP A1 A2" (4 space-separated, S=0/1)
        //   Compact format: "地址 指令(八进制)"
        //   Sequential: "指令(八进制)"
        //   Address-only line: "0010" (sets current address, like :AAAA)

        // Remove inline comments first
        int semiPos = trimmed.indexOf(';');
        if (semiPos >= 0)
            trimmed = trimmed.left(semiPos).trimmed();

        if (trimmed.isEmpty())
            continue;

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
#else
        QStringList parts = trimmed.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
#endif

        // Manual format: "±YY AAAA BBBB" (starts with + or -)
        if (parts.size() == 3 && (trimmed.startsWith('+') || trimmed.startsWith('-'))) {
            bool opOk, a1Ok, a2Ok;
            int sign = (trimmed.startsWith('+')) ? 0 : 1;  // + -> sign bit 0, - -> sign bit 1
            int op = parts[0].mid(1).toInt(&opOk, 8);      // skip the +/- prefix
            int a1 = parts[1].toInt(&a1Ok, 8);
            int a2 = parts[2].toInt(&a2Ok, 8);

            if (opOk && a1Ok && a2Ok && op >= 0 && op < 64) {
                int32_t instruction = (sign << 30) | (op << 24) | (a1 << 12) | a2;
                instruction &= DJS103Emulator::WORD_MASK;
                if (currentAddr < DJS103Emulator::MEMORY_SIZE) {
                    program[currentAddr] = instruction;
                    loaded[currentAddr] = true;
                    m_addrToLine[currentAddr] = lineIdx;
                    currentAddr++;
                }
                continue;
            }
        }

        // Address-only line: "0010" (pure octal address, sets currentAddr)
        if (parts.size() == 1) {
            bool addrOk = false;
            int addr = parts[0].toInt(&addrOk, 8);
            if (addrOk && addr >= 0 && addr < DJS103Emulator::MEMORY_SIZE) {
                currentAddr = addr;
                if (startAddr == 0 && execAddr < 0)
                    startAddr = currentAddr;
                continue;
            }

            // Sequential instruction: pure octal instruction word
            QString noSpace = parts[0];
            noSpace.remove(' ');
            bool instrOk = false;
            int32_t instr = noSpace.toLong(&instrOk, 8);
            if (instrOk && noSpace.length() >= 6) {
                if (currentAddr < DJS103Emulator::MEMORY_SIZE) {
                    program[currentAddr] = instr & DJS103Emulator::WORD_MASK;
                    loaded[currentAddr] = true;
                    m_addrToLine[currentAddr] = lineIdx;
                    currentAddr++;
                }
                continue;
            }
        }

        // Try "地址 指令" format (M3 compact: "AAAA 0500040005")
        if (parts.size() == 2) {
            bool addrOk = false;
            int parsedAddr = parts[0].toInt(&addrOk, 8);
            if (addrOk && parsedAddr >= 0 && parsedAddr < DJS103Emulator::MEMORY_SIZE) {
                // Try to parse second part as octal instruction
                QString instrStr = parts[1];
                instrStr.remove(' ');
                bool instrOk = false;
                int32_t instr = instrStr.toLong(&instrOk, 8);
                if (instrOk) {
                    program[parsedAddr] = instr & DJS103Emulator::WORD_MASK;
                    loaded[parsedAddr] = true;
                    m_addrToLine[parsedAddr] = lineIdx;
                    currentAddr = parsedAddr + 1;
                    if (startAddr == 0 && execAddr < 0)
                        startAddr = parsedAddr;
                    continue;
                }
            }
        }

        // M3 format: "S OP A1 A2" (4 space-separated octal values)
        if (parts.size() >= 4) {
            bool sOk, opOk, a1Ok, a2Ok;
            int s = parts[0].toInt(&sOk, 8);
            int op = parts[1].toInt(&opOk, 8);
            int a1 = parts[2].toInt(&a1Ok, 8);
            int a2 = parts[3].toInt(&a2Ok, 8);

            if (sOk && opOk && a1Ok && a2Ok && s <= 1 && op >= 0 && op < 64) {
                int32_t instruction = (s << 30) | (op << 24) | (a1 << 12) | a2;
                instruction &= DJS103Emulator::WORD_MASK;
                if (currentAddr < DJS103Emulator::MEMORY_SIZE) {
                    program[currentAddr] = instruction;
                    loaded[currentAddr] = true;
                    m_addrToLine[currentAddr] = lineIdx;
                    currentAddr++;
                }
                continue;
            }
        }

        // Simple format: just instruction (no address)
        QString noSpace = trimmed;
        noSpace.remove(' ');
        bool ok = false;
        int32_t instr = noSpace.toLong(&ok, 8);
        if (ok && noSpace.length() >= 6) {
            if (currentAddr < DJS103Emulator::MEMORY_SIZE) {
                program[currentAddr] = instr & DJS103Emulator::WORD_MASK;
                loaded[currentAddr] = true;
                m_addrToLine[currentAddr] = lineIdx;
                currentAddr++;
            }
        }
    }

    // Load all program words into emulator
    m_emulator.reset();
    for (int i = 0; i < DJS103Emulator::MEMORY_SIZE; ++i) {
        if (loaded[i])
            m_emulator.setMemory(i, program[i]);
    }

    // Set execution address
    if (execAddr >= 0) {
        m_emulator.setProgramCounter(execAddr);
    } else {
        m_emulator.setProgramCounter(startAddr);
    }
    // 总是设置入口地址（@指定时使用指定值，否则使用起始地址）
    m_emulator.setEntryAddress(execAddr >= 0 ? execAddr : startAddr);

    m_emulator.setAccumulator(0);
    // 显示程序入口地址信息到输出控制台
    appendOutput(tr("启存地址: %1 (八进制)").arg(m_emulator.getEntryAddress(), 4, 8, QChar('0')));
}

void DJS103Widget::onHelp()
{
    QString css =
        "<style>"
        "  body { font-family: 'Microsoft YaHei', 'PingFang SC', sans-serif; "
        "         color: #2c3e50; padding: 8px; }"
        "  h3 { color: #1a5276; font-size: 20px; border-bottom: 3px solid #2980b9; "
        "       padding-bottom: 8px; margin-bottom: 16px; }"
        "  h4 { color: #2471a3; font-size: 15px; margin-top: 18px; margin-bottom: 8px; "
        "       padding-left: 8px; border-left: 4px solid #3498db; }"
        "  p { line-height: 1.6; margin: 6px 0; }"
        "  code { color: #1a5276; padding: 2px 6px; font-size: 13px; }"
        "  table { border-collapse: collapse; width: 100%; margin: 8px 0 12px 0; "
        "          font-size: 13px; }"
        "  th { background: #2980b9; color: #ffffff; padding: 7px 10px; "
        "       text-align: left; font-weight: 600; }"
        "  td { padding: 6px 10px; border: 1px solid #d5dbdb; }"
        "  hr { border: none; border-top: 1px solid #d5dbdb; margin: 14px 0; }"
        "  ul, ol { line-height: 1.8; padding-left: 20px; }"
        "  li { margin-bottom: 4px; }"
        "  pre { color: #1a5276; padding: 10px 14px; "
        "        border: 1px solid #d5dbdb; font-size: 12px; line-height: 1.6; "
        "        white-space: pre-wrap; margin: 6px 0; }"
        "  i { color: #5d6d7e; }"
        "  b { color: #1a5276; }"
        "</style>";

    QString helpText = css + tr(
        "<h3>DJS-103 机（八一型）指令系统详解</h3>"

        "<p>103机是中国第一台通用数字电子计算机，采用<b>两地址指令格式</b>，"
        "每条指令占一个存储单元。基于苏联M-3机设计，略有本土修改。"
        "机器为异步设计（无统一时钟），一条指令执行分为八拍，"
        "运算器中有一个隐含的<b>累加寄存器</b>（r，用于暂存中间结果）。</p>"

        // ── 1. 基本技术规格 ──
        "<h4>1. 基本技术规格</h4>"
        "<table>"
        "<tr><th>项目</th><th>规格</th></tr>"
        "<tr><td>字长</td><td>31位（1位符号 + 30位二进制定点数）</td></tr>"
        "<tr><td>存储容量</td><td>初期磁鼓1024字，后期磁芯2048字</td></tr>"
        "<tr><td>地址长度</td><td>指令中12位地址字段（支持扩展）</td></tr>"
        "<tr><td>指令表示</td><td>八进制形式，如 <code>+12 3456 7012</code></td></tr>"
        "<tr><td>输入输出</td><td>五单位穿孔纸带 + 电传打字机</td></tr>"
        "<tr><td>执行速度</td><td>磁鼓~30次/秒，磁芯1800–2300次/秒</td></tr>"
        "</table>"

        "<p><b>指令格式（位分配）</b>：</p>"
        "<table>"
        "<tr><th>位范围</th><th>字段</th><th>说明</th></tr>"
        "<tr><td>位0</td><td>符号位</td><td>通常0（写为 +）</td></tr>"
        "<tr><td>位1–6</td><td>操作码 XY</td><td>6位 = 两位八进制数</td></tr>"
        "<tr><td>位7–18</td><td>第一地址 A</td><td>12位</td></tr>"
        "<tr><td>位19–30</td><td>第二地址 B</td><td>12位（结果地址）</td></tr>"
        "</table>"

        "<p>每条指令本质上是「对A和B（或上次结果）进行操作，结果放B（或不存）」。</p>"

        // ── 2. 数据表示 ──
        "<h4>2. 数据表示</h4>"
        "<p>DJS-103 采用<b>定点小数</b>表示，所有数值满足 |x| &lt; 1 。<br>"
        "内部格式：1位符号 + 29位尾数，值 = 尾数 / 2<sup>30</sup></p>"

        // ���─ 3. 操作码结构 ──
        "<h4>3. 操作码结构（XY，两位八进制）</h4>"

        "<p><b>Y（第二位八进制）</b>：基本操作种类（5种算术/逻辑运算）</p>"
        "<table>"
        "<tr><th>Y</th><th>操作</th><th>符号</th></tr>"
        "<tr><td>0</td><td>加法</td><td>+</td></tr>"
        "<tr><td>1</td><td>减法</td><td>−</td></tr>"
        "<tr><td>2</td><td>除法</td><td>÷</td></tr>"
        "<tr><td>3</td><td>乘法</td><td>×</td></tr>"
        "<tr><td>6</td><td>逻辑乘法（按位与）</td><td>AND</td></tr>"
        "</table>"

        "<p><b>X（第一位八进制）</b>：操作修饰符（决定操作数来源、结果写回、打印、绝对值等）</p>"
        "<table>"
        "<tr><th>X</th><th>操作数</th><th>结果存放</th><th>含义</th></tr>"
        "<tr><td>0</td><td>a○b</td><td>b和r</td><td>a与b运算，写回b，存r</td></tr>"
        "<tr><td>1</td><td>a○b</td><td>r</td><td>a与b运算，仅存r</td></tr>"
        "<tr><td>2</td><td>r○a</td><td>b和r</td><td>r与a运算，写回b，存r</td></tr>"
        "<tr><td>3</td><td>r○a</td><td>r</td><td>r与a运算，仅存r</td></tr>"
        "<tr><td>4</td><td>a○b</td><td>b和r</td><td>a与b运算，写回b，存r，并打印</td></tr>"
        "<tr><td>5</td><td>|a|○|b|</td><td>r</td><td>a与b绝对值运算，仅存r</td></tr>"
        "<tr><td>6</td><td>r○a</td><td>b和r</td><td>r与a运算，写回b，存r，并打印</td></tr>"
        "<tr><td>7</td><td>|r|○|b|</td><td>r</td><td>r与b绝对值运算，仅存r</td></tr>"
        "</table>"
        "<p><i>注：a=第一地址A的值，b=第二地址B的值，r=累加器（运算器寄存器）的值。<br>"
        "以上5种Y × 8种X共形成 <b>40条算术/逻辑指令</b>。所有运算为定点，结果可能溢出需程序处理。</i></p>"

        // ── 4. 控制传送指令 (Y=5) ──
        "<h4>4. 控制传送指令 (Y=5)</h4>"
        "<table>"
        "<tr><th>操作码</th><th>操作</th><th>功能</th></tr>"
        "<tr><td>+05</td><td>传送</td><td>A→B，r←A</td></tr>"
        "<tr><td>+15</td><td>传送</td><td>A→B，r←A</td></tr>"
        "<tr><td>+45</td><td>传输并打印</td><td>A→B，打印</td></tr>"
        "<tr><td>+55</td><td>传输并打印</td><td>A→B，打印</td></tr>"
        "</table>"

        // ── 5. 控制转移指令 (Y=4) ──
        "<h4>5. 控制转移指令 (Y=4)</h4>"
        "<table>"
        "<tr><th>操作码</th><th>操作</th><th>功能</th></tr>"
        "<tr><td>+24</td><td>无条件跳转</td><td>PC←A，r→B</td></tr>"
        "<tr><td>+34</td><td>条件分支</td><td>r≥0则PC←B，r&lt;0则PC←A</td></tr>"
        "<tr><td>+64</td><td>无条件跳转</td><td>PC←A，r→B，打印</td></tr>"
        "<tr><td>+74</td><td>无条件跳转</td><td>PC←B，|r|→r</td></tr>"
        "</table>"

        // ── 6. 输入/停机指令 (Y=7) ──
        "<h4>6. 输入/停机指令 (Y=7)</h4>"
        "<table>"
        "<tr><th>操作码</th><th>操作</th><th>功能</th></tr>"
        "<tr><td>+07, +27</td><td>输入</td><td>从穿孔带输入一个数字，写入第二地址，不保存在寄存器中</td></tr>"
        "<tr><td>+04, +14, +44, +54, +17, +37, +57, +77</td><td>停机</td><td>机器停止</td></tr>"
        "</table>"

        // ── 7. 指令执行流程 ──
        "<h4>7. 指令执行流程</h4>"
        "<ol>"
        "<li>取指令：从当前PC地址读出31位字</li>"
        "<li>译码操作码XY</li>"
        "<li>根据X/Y取A、B操作数（或r中上次结果）</li>"
        "<li>执行运算（加/减/乘/除/AND）</li>"
        "<li>根据X决定是否写回B、是否打印、是否更新r</li>"
        "<li>更新程序计数器（PC），或跳转</li>"
        "</ol>"

        // ── 8. M3 文件格式 ──
        "<h4>8. M3 文件格式</h4>"
        "<table>"
        "<tr><th>格式</th><th>示例</th><th>说明</th></tr>"
        "<tr><td>:AAAA</td><td>:0010</td><td>设置当前地址（八进制）</td></tr>"
        "<tr><td>=X.XXX</td><td>=0.5</td><td>十进制小数常量（存入当前地址）</td></tr>"
        "<tr><td>@AAAA</td><td>@0020</td><td>设置起始执行地址</td></tr>"
        "<tr><td>+XY A B</td><td>+05 0050 0060</td><td>指令（八进制）</td></tr>"
        "</table>"

        // ── 9. 编程示例 ──
        "<h4>9. 编程示例</h4>"
        "<p>典型短程序片段（演示乘法 + 转移 + 打印 + 停机）：</p>"
        "<pre>"
        "; 示例: 计算 0.5 * 0.5 = 0.25 并打印结果\n"
        ":0000\n"
        "+05  0004  0005   ; MOV [4]-&gt;[5]\n"
        "+03  0004  0005   ; MUL [4]*[5]-&gt;[5]\n"
        "+45  0005  0005   ; PRN [5]\n"
        "+04  0000  0000   ; HLT\n"
        ";\n"
        ":0004\n"
        "=0.5              ; 常数\n"
        "=0                ; 结果\n"
        "@0000             ; 程序入口\n"
        "</pre>"
        "<p><i>实际编程需用《103电子计算机程序汇编》（1961年科学出版社）中的子程序库。</i></p>"

        // ── 10. 常用编程技巧 ──
        "<h4>10. 常用编程技巧</h4>"
        "<ul>"
        "<li><b>减法存入</b>：DJS-103无直接减法存入指令，可用「取反再加」模式：<br>"
        "&nbsp;&nbsp;<code>+01 0052 0063</code> (r = 0 - [0063]，取反)<br>"
        "&nbsp;&nbsp;<code>+00 0063 0061</code> ([0061] = [0061] + r，完成减法)</li>"
        "<li><b>除以整数</b>：103机不支持直接除以整数&gt;1，<br>"
        "需转为乘以倒数：x/3 → x×(1/3)，将1/3存为常量</li>"
        "<li><b>循环</b>：用 <code>+34</code> 条件转移实现，注意退出条件（r≥0走B，r&lt;0走A）</li>"
        "<li><b>累加器利用</b>：连续运算时善用X=2/3（以r为操作数），可减少中间存储</li>"
        "</ul>"

        // ── 历史意义 ──
        "<h4>历史意义</h4>"
        "<p>103机指令系统简单实用，体现了早期计算机「先仿制后自主」的特点：<br>"
        "全部指令用8拍异步完成，无现代流水线/中断。<br>"
        "程序主要靠手编八进制机器码或简单汇编，配合纸带输入。<br>"
        "正是这套系统，让中国从零起步，培养了第一批计算机人才，<br>"
        "并为后续104机、109机等奠基。</p>"
    );

    QDialog dlg(this);
    dlg.setWindowTitle(tr("DJS-103 汇编帮助"));
    dlg.resize(960, 800);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QTextBrowser *browser = new QTextBrowser(&dlg);
    browser->setHtml(helpText);
    browser->setOpenExternalLinks(false);
    browser->setStyleSheet(
        "QTextBrowser { border: none; padding: 4px; }"
    );
    layout->addWidget(browser);

    QFrame *sep = new QFrame(&dlg);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #d5dbdb;");
    layout->addWidget(sep);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(12, 8, 12, 8);
    btnLayout->addStretch();
    QPushButton *closeBtn = new QPushButton(tr("关闭"), &dlg);
    closeBtn->setDefault(true);
    closeBtn->setFixedSize(90, 32);
    closeBtn->setStyleSheet(
        "QPushButton { background: #2980b9; color: white; border: none; "
        "              border-radius: 4px; font-size: 14px; }"
        "QPushButton:hover { background: #2471a3; }"
        "QPushButton:pressed { background: #1a5276; }"
    );
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    dlg.exec();
}

// ==================== Front Panel Slot Implementations ====================

void DJS103Widget::onFrontPanelStart()
{
    if (m_emulator.isHalted()) {
        appendOutput(tr("[面板] 已停机，无法起动"));
        return;
    }
    if (m_autoStep->isChecked()) {
        // 连续运行
        onRun();
    } else {
        // 步进模式：执行一步
        m_emulator.step();
        appendOutput(tr("[面板] 单步执行"));
    }
    updateRegisterDisplay();
    updateMemoryDisplay();
}

void DJS103Widget::onFrontPanelSinglePulse()
{
    if (m_emulator.isHalted()) {
        appendOutput(tr("[面板] 已停机，单脉冲无效"));
        return;
    }
    m_emulator.step();
    updateRegisterDisplay();
    updateMemoryDisplay();
    appendOutput(tr("[面板] 单脉冲"));
}

void DJS103Widget::onFrontPanelClear()
{
    m_emulator.reset();
    m_runTimer->stop();
    m_isRunning = false;
    updateRegisterDisplay();
    updateMemoryDisplay();
    appendOutput(tr("[面板] 清除 - 复位完成"));
}

void DJS103Widget::onInputStop()
{
    appendOutput(tr("[面板-输入] 停止"));
}

void DJS103Widget::onOutputStart()
{
    appendOutput(tr("[面板-输出] 起动 - 接通%1, %2行, %3")
                 .arg(m_outputConnect->isChecked() ? tr("接通") : tr("断开"))
                 .arg(m_output4_5line->isChecked() ? "4行" : "5行")
                 .arg(m_output8_10bit->isChecked() ? tr("8进制") : tr("10进制")));
}

void DJS103Widget::onOutputStop()
{
    appendOutput(tr("[面板-输出] 停止"));
}

void DJS103Widget::onClearPulseDiv()
{
    appendOutput(tr("[面板] 清除脉分 - 脉冲分配器已清零"));
}

void DJS103Widget::onMagMemoryRead()
{
    appendOutput(tr("[磁存锗] 读出操作"));
}

void DJS103Widget::onMagMemoryRecord()
{
    appendOutput(tr("[磁存锗] 记录操作"));
}

// ==================== Front Panel Implementation ====================

/**
 * 创建物理103机控制面板，模拟真实机器的前面板布局
 * 包括: 操部、脉分、输入/输出区、工作区、自动区、磁存锗区等所有开关和按钮
 */
QWidget* DJS103Widget::createFrontPanel()
{
    QWidget *panel = new QWidget;
    panel->setStyleSheet(
        "QLabel { color: #333; }"
        "QGroupBox {"
        "  color: #333; border: 1px solid #bbb; border-radius: 6px;"
        "  margin-top: 10px; font-size: 12px; font-weight: bold;"
        "}"
        "QGroupBox::title { subcontrol-origin: margin;  left: 45%; padding: 0 4px; }"
    );
    panel->setFixedSize(1040, 400);

    // ===== 顶部: 寄存器C / 选存 / 启存 =====
    QWidget *regC = createRegCLedDisplay();
    regC->setParent(panel);
    regC->setGeometry(5, 8, 575, 85);

    QWidget *selMem = createSelectMemoryDisplay();
    selMem->setParent(panel);
    selMem->setGeometry(590, 8, 220, 85);

    QWidget *startMem = createStartMemoryDisplay();
    startMem->setParent(panel);
    startMem->setGeometry(820, 8, 220, 85);

    // ===== 下方: 原有控制面板内容 (y偏移95px) =====
    // 左列: 操部 / 脉分 / 清除脉分
    QGroupBox *caobu = createCaobuGroup();
    caobu->setParent(panel);
    caobu->setGeometry(5, 95, 120, 75);

    QGroupBox *maifen = createMaifenGroup();
    maifen->setParent(panel);
    maifen->setGeometry(5, 178, 120, 70);

    createClearPulseDivBtn(panel, 40, 268);

    // 左中列: 输入 / 输出
    QGroupBox *inputGrp = createInputGroup();
    inputGrp->setParent(panel);
    inputGrp->setGeometry(140, 145, 140, 240);

    QGroupBox *outputGrp = createOutputGroup();
    outputGrp->setParent(panel);
    outputGrp->setGeometry(290, 145, 140, 240);

    // 中央列: 工作 / 中间按钮 / 自动 (全部在 createWorkSection 内)
    QWidget *workSection = createWorkSection();
    workSection->setParent(panel);
    workSection->setGeometry(450, 100, 650, 260);

    // 磁存锗
    QGroupBox *mag = createMagGroup();
    mag->setParent(panel);
    mag->setGeometry(720, 285, 190, 100);

    // 右列: S1/S2区 
    QWidget *topRight = createTopRightSection();
    topRight->setParent(panel);
    topRight->setGeometry(920, 280, 90, 120);

    return panel;
}

QGroupBox* DJS103Widget::createCaobuGroup()
{
    QGroupBox *caobuGroup = new QGroupBox(tr("操部"));
    QVBoxLayout *caobuLayout = new QVBoxLayout(caobuGroup);
    caobuLayout->setSpacing(4);
    caobuLayout->setContentsMargins(6, 12, 6, 6);

    QHBoxLayout *caobuLedLayout = new QHBoxLayout;
    caobuLedLayout->setSpacing(4);
    for (int i = 0; i < 6; ++i) {
        m_caobuLeds[i] = new QLabel;
        m_caobuLeds[i]->setFixedSize(14, 14);
        m_caobuLeds[i]->setAlignment(Qt::AlignCenter);
        m_caobuLeds[i]->setMargin(0);
        m_caobuLeds[i]->setStyleSheet(
            "QLabel { background-color: #3a3a3a; border: 1px solid #555555; "
            "border-radius: 7px; }");
        m_caobuLeds[i]->setToolTip(tr("位 %1").arg(i));
        caobuLedLayout->addWidget(m_caobuLeds[i]);
    }
    caobuLedLayout->addStretch();
    caobuLayout->addLayout(caobuLedLayout);
    return caobuGroup;
}

QGroupBox* DJS103Widget::createMaifenGroup()
{
    QGroupBox *maifenGroup = new QGroupBox(tr("脉分"));
    QVBoxLayout *maifenLayout = new QVBoxLayout(maifenGroup);
    maifenLayout->setSpacing(4);
    maifenLayout->setContentsMargins(6, 12, 6, 6);

    QHBoxLayout *maifenLedLayout = new QHBoxLayout;
    maifenLedLayout->setSpacing(4);
    for (int i = 0; i < 3; ++i) {
        m_maifenLeds[i] = new QLabel;
        m_maifenLeds[i]->setFixedSize(14, 14);
        m_maifenLeds[i]->setAlignment(Qt::AlignCenter);
        m_maifenLeds[i]->setMargin(0);
        m_maifenLeds[i]->setStyleSheet(
            "QLabel { background-color: #3a3a3a; border: 1px solid #555555; "
            "border-radius: 7px; }");
        m_maifenLeds[i]->setToolTip(tr("位 %1").arg(i));
        maifenLedLayout->addWidget(m_maifenLeds[i]);
    }
    maifenLedLayout->addStretch();
    maifenLayout->addLayout(maifenLedLayout);
    return maifenGroup;
}

QGroupBox* DJS103Widget::createInputGroup()
{
    QGroupBox *inputGroup = new QGroupBox(tr("输入"));
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->setSpacing(4);
    inputLayout->setContentsMargins(6, 12, 6, 6);

    QHBoxLayout *inputToggleRow = new QHBoxLayout;
    inputToggleRow->setSpacing(4);
    m_inputConnect = createToggleSwitch(tr("接通"), tr("断开"), inputToggleRow);
    m_inputContinuous = createToggleSwitch(tr("连续"), tr("步进"), inputToggleRow);
    m_input8_10bit = createToggleSwitch(tr("8进位"), tr("10进位"), inputToggleRow);
    inputLayout->addLayout(inputToggleRow);

    QHBoxLayout *inputBtnRow = new QHBoxLayout;
    inputBtnRow->setSpacing(8);
    m_inputStart = createPushSwitch(tr("起动"), inputBtnRow);
    m_inputStop = createPushSwitch(tr("停止"), inputBtnRow);
    inputLayout->addLayout(inputBtnRow);
    return inputGroup;
}

QGroupBox* DJS103Widget::createOutputGroup()
{
    QGroupBox *outputGroup = new QGroupBox(tr("输出"));
    QVBoxLayout *outputLayout = new QVBoxLayout(outputGroup);
    outputLayout->setSpacing(4);
    outputLayout->setContentsMargins(6, 12, 6, 6);

    QHBoxLayout *outputToggleRow = new QHBoxLayout;
    outputToggleRow->setSpacing(4);
    m_outputConnect = createToggleSwitch(tr("接通"), tr("断开"), outputToggleRow);
    m_output4_5line = createToggleSwitch(tr("4行"), tr("5行"), outputToggleRow);
    m_output8_10bit = createToggleSwitch(tr("8进位"), tr("10进位"), outputToggleRow);
    outputLayout->addLayout(outputToggleRow);

    QHBoxLayout *outputBtnRow = new QHBoxLayout;
    outputBtnRow->setSpacing(8);
    m_outputStart = createPushSwitch(tr("起动"), outputBtnRow);
    m_outputStop = createPushSwitch(tr("停止"), outputBtnRow);
    outputLayout->addLayout(outputBtnRow);
    return outputGroup;
}

QWidget* DJS103Widget::createWorkSection()
{
    QWidget *workWidget = new QWidget;

    QWidget *switchRow = createWorkSwitchRow();
    switchRow->setParent(workWidget);
    switchRow->setGeometry(85, 0, 120, 90);

    QWidget *haltRow = createHaltAddrRow();
    haltRow->setParent(workWidget);
    haltRow->setGeometry(230, 50, 355, 90);

    QWidget *centerBtns = createCenterButtons();
    centerBtns->setParent(workWidget);
    centerBtns->setGeometry(0, 94, 450, 60);

    QWidget *autoSection = createAutoSection();
    autoSection->setParent(workWidget);
    autoSection->setGeometry(40, 158, 200, 90);

    return workWidget;
}

QWidget* DJS103Widget::createWorkSwitchRow()
{
    QWidget *widget = new QWidget;
    QHBoxLayout *workSwitchRow = new QHBoxLayout(widget);
    workSwitchRow->setContentsMargins(0, 0, 0, 0);
    workSwitchRow->setSpacing(6);
    // workSwitchRow->addStretch();
    m_workC = createToggleSwitch(tr("C"), tr(" "), workSwitchRow);
    m_workSelectMem = createToggleSwitch(tr("选存"), tr("记存"), workSwitchRow);
    m_workStartMem = createToggleSwitch(tr("启存"), tr(" "), workSwitchRow);
    return widget;
}

QWidget* DJS103Widget::createHaltAddrRow()
{
    QWidget *widget = new QWidget;
    QVBoxLayout *haltAddrVL = new QVBoxLayout(widget);
    haltAddrVL->setContentsMargins(0, 0, 0, 0);
    haltAddrVL->setSpacing(1);

    QHBoxLayout *haltAddrRow = new QHBoxLayout;
    haltAddrRow->setSpacing(2);
    const int compactW = 28, compactH = 80;
    m_haltAddr0 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    m_haltAddr1 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    m_haltAddr2 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    m_haltAddr3 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    m_haltAddr4 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    m_haltAddr5 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    m_haltAddr6 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    m_haltAddr7 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    m_haltAddr8 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    m_haltAddr9 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    m_haltAddr10 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    m_haltAddr11 = createToggleSwitch(tr(" "), tr(" "), haltAddrRow, compactW, compactH);
    haltAddrVL->addLayout(haltAddrRow);

    QLabel *haltAddrLabel = new QLabel(tr("停机地址"));
    haltAddrLabel->setAlignment(Qt::AlignCenter);
    haltAddrLabel->setStyleSheet("color: #333; font-size: 9px; font-weight: bold; background: transparent;");
    haltAddrVL->addWidget(haltAddrLabel);

    m_haltAddrGroup = new QButtonGroup(this);
    m_haltAddrGroup->setExclusive(false);
    m_haltAddrGroup->addButton(m_haltAddr0);
    m_haltAddrGroup->addButton(m_haltAddr1);
    m_haltAddrGroup->addButton(m_haltAddr2);
    m_haltAddrGroup->addButton(m_haltAddr3);
    m_haltAddrGroup->addButton(m_haltAddr4);
    m_haltAddrGroup->addButton(m_haltAddr5);
    m_haltAddrGroup->addButton(m_haltAddr6);
    m_haltAddrGroup->addButton(m_haltAddr7);
    m_haltAddrGroup->addButton(m_haltAddr8);
    m_haltAddrGroup->addButton(m_haltAddr9);
    m_haltAddrGroup->addButton(m_haltAddr10);
    m_haltAddrGroup->addButton(m_haltAddr11);

    return widget;
}

QWidget* DJS103Widget::createCenterButtons()
{
    QWidget *widget = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    m_fpStart = createPushSwitch(tr("起动"), layout);
    m_fpSinglePulse = createPushSwitch(tr("单脉冲"), layout);
    m_fpClear0 = createPushSwitch(tr(" "), layout);
    m_fpClear1 = createPushSwitch(tr("清除"), layout);
    m_fpClear2 = createPushSwitch(tr(" "), layout);
    layout->addStretch();
    return widget;
}

QWidget* DJS103Widget::createAutoSection()
{
    QWidget *autoWidget = new QWidget;
    QVBoxLayout *autoVLayout = new QVBoxLayout(autoWidget);
    autoVLayout->setContentsMargins(0, 0, 0, 0);
    autoVLayout->setSpacing(2);

    QHBoxLayout *autoRow1 = new QHBoxLayout;
    autoRow1->setSpacing(4);
    QWidget *stepSw = new QWidget;
    QVBoxLayout *stepVL = new QVBoxLayout(stepSw);
    stepVL->setContentsMargins(0,0,0,0); stepVL->setSpacing(1);
    m_autoStep = createToggleSwitch(tr("自动"), tr("步进"), stepVL);
    QWidget *haltSw = new QWidget;
    QVBoxLayout *haltVL = new QVBoxLayout(haltSw);
    haltVL->setContentsMargins(0,0,0,0); haltVL->setSpacing(1);
    m_autoHalt = createToggleSwitch(tr("自动工作"), tr("停机"), haltVL);
    QWidget *selmemSw = new QWidget;
    QVBoxLayout *selmemVL = new QVBoxLayout(selmemSw);
    selmemVL->setContentsMargins(0,0,0,0); selmemVL->setSpacing(1);
    m_autoSelectMem = createToggleSwitch(tr("启存"), tr("选存"), selmemVL);
    autoRow1->addWidget(stepSw);
    autoRow1->addWidget(haltSw);
    autoRow1->addWidget(selmemSw);
    autoVLayout->addLayout(autoRow1);
    return autoWidget;
}

QWidget* DJS103Widget::createTopRightSection()
{
    QWidget *widget = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(4);

    QHBoxLayout *s1Row = new QHBoxLayout;
    m_s1Switch = createPushSwitch(tr("S1"), s1Row);
    s1Row->addStretch();
    layout->addLayout(s1Row);

    QHBoxLayout *s2Row = new QHBoxLayout;
    m_s2Switch = createPushSwitch(tr("S2"), s2Row);
    s2Row->addStretch();
    layout->addLayout(s2Row);
    return widget;
}

QGroupBox* DJS103Widget::createMagGroup()
{
    QGroupBox *magGroup = new QGroupBox(tr("磁存锗"));
    QVBoxLayout *magLayout = new QVBoxLayout(magGroup);
    magLayout->setSpacing(8);
    magLayout->setContentsMargins(10, 14, 10, 10);

    QHBoxLayout *magBtnRow = new QHBoxLayout;
    magBtnRow->setSpacing(12);
    m_magRead = createPushSwitch(tr("读出"), magBtnRow);
    m_magRecord = createPushSwitch(tr("记录"), magBtnRow);
    magLayout->addLayout(magBtnRow);
    return magGroup;
}

void DJS103Widget::createClearPulseDivBtn(QWidget *parent, int x, int y)
{
    QWidget *container = new QWidget(parent);
    QVBoxLayout *vLayout = new QVBoxLayout(container);
    vLayout->setSpacing(2);
    vLayout->setContentsMargins(2, 2, 2, 2);
    m_clearPulseDiv = createPushSwitch(tr("清除脉分"), vLayout);
    container->setGeometry(x, y, 70, 56);
}

QCheckBox* DJS103Widget::createToggleSwitch(const QString &topLabel, const QString &bottomLabel, QBoxLayout *layout, int switchWidth, int switchHeight)
{
    // QCheckBox 作为整体容器，隐藏默认 indicator，用 QSlider 实现拨动效果
    QCheckBox *sw = new QCheckBox;
    sw->setChecked(true);
    sw->setFixedSize(switchWidth, switchHeight);
    sw->setStyleSheet("QCheckBox { spacing: 0px; background: transparent; padding: 0px; }"
                      "QCheckBox::indicator { width: 0px; height: 0px; margin: 0px; border: 0px; padding: 0px; }");

    QVBoxLayout *vLayout = new QVBoxLayout(sw);
    vLayout->setContentsMargins(1, 1, 1, 1);
    vLayout->setSpacing(0);
    vLayout->setAlignment(Qt::AlignCenter);

    // 上方文字
    if (!topLabel.isEmpty()) {
        QLabel *top = new QLabel(topLabel);
        top->setAlignment(Qt::AlignCenter);
        top->setStyleSheet("color: #333; font-size: 9px; font-weight: bold; background: transparent;");
        vLayout->addWidget(top);
    }

    // 可见的 QSlider 实现上下拨动效果
    int grooveWidth = qMax(switchWidth - 6, 16);
    int trackHeight = switchHeight;
    if (!topLabel.isEmpty()) trackHeight -= 14;
    if (!bottomLabel.isEmpty()) trackHeight -= 14;

    QSlider *slider = new QSlider(Qt::Vertical);
    slider->setRange(0, 1);
    slider->setValue(1);
    slider->setFixedSize(switchWidth, qMax(trackHeight, 20));

    // 滑块尺寸：handle 宽高相等，形成可抓握的圆形
    int handleSize = grooveWidth-2;

    slider->setStyleSheet(QString(
        "QSlider::groove:vertical {"
        "  width: %1px; height: %2px;"
        "  border: 1px solid #555; border-radius: %3px;"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "      stop:0 #444, stop:0.5 #555, stop:1 #666);"
        "}"
        "QSlider::handle:vertical {"
        "  width: %4px; height: %4px;"
        "  margin: 0 -%5px;"
        "  border: 1px solid #999; border-radius: %6px;"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "      stop:0 #f0f0f0, stop:0.4 #ccc, stop:1 #999);"
        "}"
        "QSlider::handle:vertical:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "      stop:0 #fff, stop:0.4 #ddd, stop:1 #aaa);"
        "  border-color: #bba;"
        "}"
    ).arg(grooveWidth).arg(trackHeight - handleSize + handleSize / 2)
     .arg(grooveWidth / 2 + 1)
     .arg(handleSize)
     .arg((handleSize - grooveWidth) / 2 + 1)
     .arg(handleSize / 2 + 1));

    vLayout->addWidget(slider, 0, Qt::AlignHCenter);

    // 同步 slider 和 checkbox 状态
    QObject::connect(slider, &QSlider::valueChanged, sw, [sw](int val) {
        sw->setChecked(val == 1);
    });
    QObject::connect(sw, &QCheckBox::toggled, slider, [slider](bool checked) {
        slider->setValue(checked ? 1 : 0);
    });

    // indicator 隐藏后点击不会自动切换，手动切换 slider
    QObject::connect(sw, &QCheckBox::clicked, slider, [slider]() {
        slider->setValue(slider->value() == 0 ? 1 : 0);
    });

    // 下方文字
    if (!bottomLabel.isEmpty()) {
        QLabel *bottom = new QLabel(bottomLabel);
        bottom->setAlignment(Qt::AlignCenter);
        bottom->setStyleSheet("color: #333; font-size: 9px; font-weight: bold; background: transparent;");
        vLayout->addWidget(bottom);
    }

    if (layout)
        layout->addWidget(sw);

    return sw;
}

QPushButton* DJS103Widget::createPushSwitch(const QString &name, QBoxLayout *layout, int buttonSize)
{
    // 外层容器：圆形按钮 + 下方文字标签
    QWidget *container = new QWidget;
    QVBoxLayout *vLayout = new QVBoxLayout(container);
    vLayout->setSpacing(2);
    vLayout->setContentsMargins(2, 2, 2, 2);
    vLayout->setAlignment(Qt::AlignCenter);

    // 圆形按钮
    QPushButton *btn = new QPushButton;
    btn->setFixedSize(buttonSize, buttonSize);
    btn->setToolTip(name);
    QString style = QString(
        "QPushButton {"
        "  background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "      stop:0 #e8d5a0, stop:1 #c4a030);"
        "  border: 2px solid #8b6914;"
        "  border-radius: %1px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "      stop:0 #f0e0b0, stop:1 #d4b040);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "      stop:0 #b09020, stop:1 #8b6914);"
        "  border-style: inset;"
        "}"
    ).arg(buttonSize / 2);
    btn->setStyleSheet(style);

    // 文字标签
    QLabel *label = new QLabel(name);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("color: #333; font-size: 9px; font-weight: bold; background: transparent;");

    vLayout->addWidget(btn, 0, Qt::AlignHCenter);
    vLayout->addWidget(label);

    if (layout)
        layout->addWidget(container);

    return btn;
}