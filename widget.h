/****************************************************************************
**
** Copyright (C) 2024
** DJS-103 Emulator Widget - Qt GUI for the 103 computer emulator
**
****************************************************************************/

#ifndef DJS103WIDGET_H
#define DJS103WIDGET_H

#include <QWidget>
#include <QMap>
#include "emulator.h"

QT_BEGIN_NAMESPACE
class QPushButton;
class QPlainTextEdit;
class QTableWidget;
class QLabel;
class QSpinBox;
class QLineEdit;
class QSplitter;
class QComboBox;
class QCheckBox;
class QGroupBox;
class QBoxLayout;
class QButtonGroup;
QT_END_NAMESPACE

/**
 * @brief 103机模拟器界面控件
 *
 * 包含代码编辑器、控制面板、寄存器/内存视图和输出控制台
 * 支持 M3 文件格式和交互式输入
 */
class DJS103Widget : public QWidget
{
    Q_OBJECT

public:
    explicit DJS103Widget(QWidget *parent = nullptr);

private slots:
    void onLoadProgram();
    void onStep();
    void onRun();
    void onStop();
    void onReset();
    void onLoadExample();
    void onLoadExample2();
    void onLoadExample3();
    void onLoadExample4();
    void onMemoryCellChanged(int row, int col);
    void onRunTick();
    void onHelp();

    // Front panel slots
    void onFrontPanelStart();
    void onFrontPanelSinglePulse();
    void onFrontPanelClear();
    void onInputStop();
    void onOutputStart();
    void onOutputStop();
    void onClearPulseDiv();
    void onMagMemoryRead();
    void onMagMemoryRecord();

private:
    void createUI();
    void updateRegisterDisplay();
    void updateMemoryDisplay();
    void appendOutput(const QString &text);
    void appendOutput(const std::string &text);
    void loadM3Program(const QString &text);

    // Emulator core
    DJS103Emulator m_emulator;

    // Controls
    QPushButton *m_exampleButton;
    QPushButton *m_example2Button;
    QPushButton *m_example3Button;
    QPushButton *m_example4Button;
    QPushButton *m_helpButton;

    // Register display
    QLabel *m_pcLabel;

    // LED display for 31-bit register
    static const int NUM_LEDS = 31;
    static const int SELECT_MEMORY_BITS = 12;
    static const int START_MEMORY_BITS = 12;

    QLabel *m_regCLeds[NUM_LEDS];   // reg C LEDs
    QPushButton *m_regCSwitches[NUM_LEDS];   // reg C DIP switches
    QLabel *m_selectMemoryLeds[SELECT_MEMORY_BITS];   // 选存 LEDs
    QPushButton *m_selectMemorySwitches[SELECT_MEMORY_BITS];   // 选存 DIP switches
    QLabel *m_startMemoryLeds[START_MEMORY_BITS];   // 启存 LEDs
    QPushButton *m_startMemorySwitches[START_MEMORY_BITS];   // 启存 DIP switches

    // Code editor
    QPlainTextEdit *m_codeEdit;

    // Memory view
    QTableWidget *m_memTable;

    // Output console
    QPlainTextEdit *m_outputEdit;

    // Run timer
    QTimer *m_runTimer;
    bool m_isRunning;
    int m_runSpeed;

    // Memory update guard
    bool m_updatingMemory;

    // Source line tracking: memory address -> editor line number (0-based)
    QMap<int, int> m_addrToLine;

    void highlightCurrentLine();

    // LED display helpers
    QWidget* createRegCLedDisplay();
    void updateRegCLedDisplay(int32_t value);
    QWidget* createSelectMemoryDisplay();
    QWidget* createStartMemoryDisplay();

    // Front panel (103机物理面板)
    QWidget* createFrontPanel();
    QGroupBox* createCaobuGroup();
    QGroupBox* createMaifenGroup();
    QGroupBox* createInputGroup();
    QGroupBox* createOutputGroup();
    QWidget* createWorkSection();
    QWidget* createWorkSwitchRow();
    QWidget* createHaltAddrRow();
    QWidget* createCenterButtons();
    QWidget* createAutoSection();
    QWidget* createTopRightSection();
    QGroupBox* createMagGroup();
    void createClearPulseDivBtn(QWidget *parent, int x, int y);
    QCheckBox* createToggleSwitch(const QString &topLabel = QString(), const QString &bottomLabel = QString(), QBoxLayout *layout = nullptr, int switchWidth = 28, int switchHeight = 80);
    QPushButton* createPushSwitch(const QString &name, QBoxLayout *layout = nullptr, int buttonSize = 28);
    QWidget* createDipSwitchRow(int count, const QString &weightLabels = QString());

    // 操部 LEDs: 6 bits
    QLabel *m_caobuLeds[6];
    // 脉分 LEDs: 3 bits
    QLabel *m_maifenLeds[3];

    // 输入 section toggles and buttons
    QCheckBox *m_inputConnect;       // 接通/断开
    QCheckBox *m_inputContinuous;    // 连续/步进
    QCheckBox *m_input8_10bit;       // 8位/10位
    QPushButton *m_inputStart;        // 起动
    QPushButton *m_inputStop;         // 停止

    // 输出 section toggles and buttons
    QCheckBox *m_outputConnect;      // 接通/断开
    QCheckBox *m_output4_5line;      // 4行/5行
    QCheckBox *m_output8_10bit;      // 8位/10位
    QPushButton *m_outputStart;       // 起动
    QPushButton *m_outputStop;        // 停止

    // 工作 section toggles
    QCheckBox *m_workC;              // C 开关
    QCheckBox *m_workSelectMem;      // 选存开关
    QCheckBox *m_workStartMem;       // 启存开关
    QCheckBox *m_haltAddr0;
    QCheckBox *m_haltAddr1;
    QCheckBox *m_haltAddr2;
    QCheckBox *m_haltAddr3;
    QCheckBox *m_haltAddr4;
    QCheckBox *m_haltAddr5;
    QCheckBox *m_haltAddr6;
    QCheckBox *m_haltAddr7;
    QCheckBox *m_haltAddr8;
    QCheckBox *m_haltAddr9;
    QCheckBox *m_haltAddr10;
    QCheckBox *m_haltAddr11;
    QButtonGroup *m_haltAddrGroup;

    // 中间按钮行
    QPushButton *m_fpStart;          // 起动
    QPushButton *m_fpSinglePulse;    // 单脉冲
    QPushButton *m_fpClear0;          // 清除
    QPushButton *m_fpClear1;          // 清除
    QPushButton *m_fpClear2;          // 清除

    // 自动 section toggles
    QCheckBox *m_autoStep;           // 步进 SW1
    QCheckBox *m_autoHalt;           // 停机 SW4
    QCheckBox *m_autoSelectMem;      // 选存 SW5

    // 清除脉分 button
    QPushButton *m_clearPulseDiv;    // 清除脉分

    // 磁存锗 buttons
    QPushButton *m_magRead;          // 读出
    QPushButton *m_magRecord;        // 记录

    // S1, S2 top toggle switches
    QPushButton *m_s1Switch;
    QPushButton *m_s2Switch;
};

#endif // DJS103WIDGET_H
