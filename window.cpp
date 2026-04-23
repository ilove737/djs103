#include <QtWidgets>

#include "window.h"
#include "widget.h"

Window::Window()
{
    m_djs103Widget = new DJS103Widget;

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(m_djs103Widget);
    setLayout(layout);

    setWindowTitle(tr("103机模拟器"));
    resize(1100, 900);
}
