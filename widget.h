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
    QPushButton *m_loadButton;
    QPushButton *m_stepButton;
    QPushButton *m_runButton;
    QPushButton *m_stopButton;
    QPushButton *m_resetButton;
    QPushButton *m_exampleButton;
    QPushButton *m_example2Button;
    QPushButton *m_example3Button;
    QPushButton *m_example4Button;
    QPushButton *m_helpButton;

    // Register display
    QLabel *m_regCLabel;
    QLabel *m_regCValueLabel;
    QLabel *m_pcLabel;
    QLabel *m_statusLabel;
    QLabel *m_instLabel;

    // LED display for 31-bit register
    static const int NUM_LEDS = 31;
    static const int SELECT_MEMORY_BITS = 12;  // 选存位数：12位
    static const int START_MEMORY_BITS = 12;   // 启存位数：12位
    QLabel *m_leds[NUM_LEDS];      // last instruction LEDs
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
    QWidget* createRegCLedDisplay(QFont& monoFont, QFont& boldFont);
    void updateRegCLedDisplay(int32_t value);
    QWidget* createSelectMemoryDisplay(QFont& monoFont, QFont& boldFont);
    QWidget* createStartMemoryDisplay(QFont& monoFont, QFont& boldFont);
};

#endif // DJS103WIDGET_H
