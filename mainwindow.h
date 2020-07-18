#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QApplication>
#include <QActionGroup>
#include <QTranslator>
#include <QTimer>
#include <QElapsedTimer>
#include <QMessageBox>

//#include <QEvent>
#include "util/systemmonitor.h"
#include "dialogs/errordialog.h"
#include "util/versioncheckerparent.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setupUsagePlots();
private slots:
    void updateWidget();
    void onStopUpdatingWidget();
    void slotLanguageChanged(QAction* action);
    void slotSettingsChanged(QAction* action);
    void slotHelpChanged(QAction* action);
    void versionChecked(bool status);
    void timerTimeOut();
    void onHddUsagePlotClicked(bool newStatus);
signals:
    void stopUpdatingPowerWidget();
protected:
    void closeEvent(QCloseEvent* e) override;
private:
    Ui::MainWindow *ui;
    void loadLanguage(const QString& rLanguage);
    void switchTranslator(QString language);
    QActionGroup* languageGroup = nullptr;
    QActionGroup* settingsGroup = nullptr;
    QActionGroup* helpGroup = nullptr;
    QHash<QWidget*,ErrorDialog*> localErrorDialogHandler;
    bool alreadyEvaluated = false;
    QLabel* CPUusedLabel = nullptr;
    QLabel* versionActual = nullptr;

    int timerCounter = 0;
    QTimer* timer = nullptr;
    VersionCheckerParent* vcp = nullptr;
};
#endif // MAINWINDOW_H
