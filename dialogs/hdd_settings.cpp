#include <string>
#include <io.h>
#include <sstream>
#include <sys/types.h>  // For stat().
#include <sys/stat.h>   // For stat().
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <windows.h>
//#include "stdafx.h"

#include "hdd_settings.h"
#include "ui_hdd_settings.h"
#include "shared_staff/globalsettings.h"
#include "util/files_folders_operations.h"

#include <QDebug>

void ReplaceStringInPlace(std::string& subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}

HDD_Settings::HDD_Settings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HDD_Settings)
{
    ui->setupUi(this);
    connect(ui->buttonBox,SIGNAL(accepted()),this,SLOT(slot_accepted()));
    connect(ui->buttonBox,SIGNAL(rejected()),this,SLOT(slot_rejected()));

    setLabelIcon(IconType::GREY);
    timer = new QTimer(this);
}

HDD_Settings::~HDD_Settings()
{
    delete ui;
}

void HDD_Settings::slot_accepted(){
    GlobalSettings::getSettings()->setHDDCounter(scriptResults.at(0),scriptResults.at(1));
}

void HDD_Settings::slot_rejected(){
    timer->stop();
}

void HDD_Settings::setLabelIcon(IconType i_type){
    switch (i_type) {
    case IconType::GREEN:
        ui->diodeLabel->setPixmap(QPixmap(":/images/greenDiode.png"));
        break;
    case IconType::GREY:
        ui->diodeLabel->setPixmap(QPixmap(":/images/greyDiode.png"));
        break;
    case IconType::RED:
        ui->diodeLabel->setPixmap(QPixmap(":/images/redDiode.png"));
        break;
    case IconType::ORANGE:
        ui->diodeLabel->setPixmap(QPixmap(":/images/orangeDiode.png"));
        break;
    }
}

void HDD_Settings::setLabelText(QString i_text){
    ui->infoLabel->setText(i_text);
}


void HDD_Settings::on_pushButton_clicked()
{
    std::string fileName = QString("/GetLocalCounterName.ps1").toLocal8Bit().constData();

    std::string strPath = GlobalSettings::getSettings()->getAppPath().toLocal8Bit().constData()+fileName;
    if(access(strPath.c_str(),0) == 0)
    {
        qDebug()<<"File accessible";
        std::string dirPath = GlobalSettings::getSettings()->getAppPath().toLocal8Bit().constData();
        std::string complete = dirPath+fileName;
        ReplaceStringInPlace(complete,"/","\\");
        std::ostringstream os;
        os << "-ExecutionPolicy ByPass -NoProfile -NonInteractive -WindowStyle Hidden -File \""+complete+"\"";
        std::string op = "open";
        std::string ps = "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\PowerShell.exe";
        std::string param = os.str();
        DWORD res = (int)(ShellExecuteA(NULL, op.c_str(), ps.c_str(), param.c_str(), NULL, SW_HIDE));
        qDebug()<<res;
        if (res <= 32){
            setLabelIcon(IconType::RED);
            QString errorMessage = QString(tr("An error occured when launching the script: %i")).arg(res);
            setLabelText(errorMessage);
        }
        else{
            setLabelIcon(IconType::ORANGE);
            QString errorMessage = QString(tr("Processing"));
            setLabelText(errorMessage);
            timer->setInterval(2000);
            timer->setTimerType(Qt::PreciseTimer);
            connect(timer, SIGNAL(timeout()), this, SLOT(scanFolder()));
            timer->start();
        }
    }
    else{
        setLabelIcon(IconType::RED);
        QString errorMessage = QString(tr("Unable to load the script"));
        setLabelText(errorMessage);
    }
}

void HDD_Settings::scanFolder(){
    QStringList foundFiles;
    int numberOfFiles=0;
    analyseFileNames(GlobalSettings::getSettings()->getAppPath(),foundFiles,numberOfFiles,"txt");
    if (foundFiles.contains("ScriptResult.txt")){
        timer->stop();
        QFile file(GlobalSettings::getSettings()->getAppPath()+"/ScriptResult.txt");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)){
            setLabelIcon(IconType::GREEN);
            setLabelText(tr("Counter and parameter found"));
            QTextStream in(&file);
            while (!in.atEnd()) {
                scriptResults.append(in.readLine());
            }
            ui->counterNameLE->setText(scriptResults.at(0));
            ui->counterParameterLE->setText(scriptResults.at(1));
        }
    }
}
