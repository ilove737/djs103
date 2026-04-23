QT += widgets network
requires(qtConfig(combobox))
TARGET = djs103

HEADERS     = window.h \
              emulator.h \
              widget.h
SOURCES     = main.cpp \
              window.cpp \
              emulator.cpp \
              widget.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/widgets/djs103
INSTALLS += target
