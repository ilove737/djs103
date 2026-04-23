#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>

class DJS103Widget;

class Window : public QWidget
{
    Q_OBJECT

public:
    Window();

private:
    DJS103Widget *m_djs103Widget;
};

#endif
