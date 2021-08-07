# ----------------------------------------------------
# This file is generated by the Qt Visual Studio Tools.
# ------------------------------------------------------

# This is a reminder that you are using a generated .pro file.
# Remove it when you are finished editing this file.
message("You are running qmake on a generated .pro file. This may not work!")


HEADERS += ./src/include/AppCore.h \
    ./src/include/resource.h \
    ./src/include/stdafx.h \
    ./src/include/UI/DisplayData.h \
    ./src/include/UI/DisplaySystem.h \
    ./src/include/UI/BrowsePictDialog.h \
    ./src/include/UI/EditLevelDialog.h \
    ./src/include/UI/PreviewConfigWindow.h \
    ./src/include/UI/ScreenEditWidget.h \
    ./src/include/UI/ExportScenarioDialog.h \
    ./src/include/UI/TerminalEditorWindow.h \
    ./src/include/UI/ScenarioBrowserView.h \
    ./src/include/UI/HuxQt.h \
    ./src/include/Scenario/Level.h \
    ./src/include/Scenario/Scenario.h \
    ./src/include/Scenario/ScenarioManager.h \
    ./src/include/Scenario/Terminal.h \
    ./src/include/Scenario/ScenarioBrowserModel.h \
    ./src/include/Utils/Utilities.h \
    ./src/include/Utils/ScenarioBrowserWidget.h
SOURCES += ./src/source/AppCore.cpp \
    ./src/source/main.cpp \
    ./src/source/stdafx.cpp \
    ./src/source/Scenario/Level.cpp \
    ./src/source/Scenario/Scenario.cpp \
    ./src/source/Scenario/ScenarioBrowserModel.cpp \
    ./src/source/Scenario/ScenarioManager.cpp \
    ./src/source/Scenario/Terminal.cpp \
    ./src/source/UI/EditLevelDialog.cpp \
    ./src/source/UI/BrowsePictDialog.cpp \
    ./src/source/UI/DisplaySystem.cpp \
    ./src/source/UI/ExportScenarioDialog.cpp \
    ./src/source/UI/HuxQt.cpp \
    ./src/source/UI/PreviewConfigWindow.cpp \
    ./src/source/UI/ScenarioBrowserView.cpp \
    ./src/source/UI/ScreenEditWidget.cpp \
    ./src/source/UI/TerminalEditorWindow.cpp \
    ./src/source/Utils/ScenarioBrowserWidget.cpp
FORMS += ./src/forms/EditLevelDialog.ui \
    ./src/forms/BrowsePictDialog.ui \
    ./src/forms/ExportScenarioDialog.ui \
    ./src/forms/HuxQt.ui \
    ./src/forms/PreviewConfigWindow.ui \
    ./src/forms/ScreenEditWidget.ui \
    ./src/forms/TerminalEditorWindow.ui \
    ./src/source/UI/ScenarioBrowserView.ui
RESOURCES += src/resources/HuxQt.qrc
