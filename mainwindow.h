#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "hlavni_program/t_b_ho.h"
#include "hlavni_program/zalozky.h"
#include "vykon/cpuwidget.h"
#include "vykon/hddusageplot.h"
#include "vykon/memorywidget.h"
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void setupUsagePlots();
private slots:
    void updateWidget();
private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
