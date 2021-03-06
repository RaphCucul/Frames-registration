#include "dialogs/multiplevideoet.h"
#include "ui_multiplevideoet.h"
//#include "image_analysis/entropy.h"
#include "image_analysis/frangi_utilization.h"
#include "image_analysis/image_processing.h"
#include "main_program/frangi_detektor.h"
#include "dialogs/graphet_parent.h"
#include "dialogs/clickimageevent.h"
#include "util/files_folders_operations.h"
#include "multithreadET/qThreadFirstPart.h"
#include "util/vector_operations.h"
#include "registration/multiPOC_Ai1.h"
#include "shared_staff/sharedvariables.h"

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <QList>
#include <QUrl>
#include <QMimeData>
#include <QMimeType>
#include <QMimeDatabase>
#include <QDebug>
#include <QFileDialog>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QtConcurrent/QtConcurrent>
#include <QFuture>

MultipleVideoET::MultipleVideoET(QWidget *parent) :
    ETanalysisParent (parent),
    ui(new Ui::MultipleVideoET)
{
    ui->setupUi(this);
    setAcceptDrops(true);
    ui->showResultsPB->setEnabled(false);
    ui->showResultsPB->setText(tr("Show results"));
    ui->savePB->setEnabled(false);
    ui->savePB->setText(tr("Save computed parameters"));
    ui->horizontalAnomaly->setEnabled(false);
    ui->horizontalAnomaly->setText(tr("Top/bottom anomaly"));
    ui->verticalAnomaly->setEnabled(false);
    ui->verticalAnomaly->setText(tr("Left/right anomaly"));
    ui->areaMaximum->setPlaceholderText("0-20");
    ui->rotationAngle->setPlaceholderText("0 - 0.5");
    ui->iterationCount->setPlaceholderText("1-Inf; -1~auto");
    ui->areaSizelabel->setText(tr("Size of calculation area"));
    ui->angleTolerationlabel->setText(tr("Maximal tolerated rotation angle"));
    ui->numberIterationlabel->setText(tr("Number of iterations of algorithm"));
    ui->afewVideosPB->setText(tr("Choose few files"));
    ui->wholeFolderPB->setText(tr("Choose whole folder"));
    ui->analyzeVideosPB->setText(tr("Analyse videos"));
    ui->analyzeVideosPB->setEnabled(false);

    localErrorDialogHandling[ui->analyzeVideosPB] = new ErrorDialog(ui->analyzeVideosPB);

    QObject::connect(this,SIGNAL(checkValuesPass()),this,SLOT(evaluateCorrectValues()));
    QObject::connect(ui->verticalAnomaly,SIGNAL(stateChanged(int)),this,SLOT(showDialog()));
    QObject::connect(ui->horizontalAnomaly,SIGNAL(stateChanged(int)),this,SLOT(showDialog()));
    QObject::connect(this,SIGNAL(calculationStarted()),this,SLOT(disableWidgets()));
    QObject::connect(this,SIGNAL(calculationStopped()),this,SLOT(enableWidgets()));
}

MultipleVideoET::~MultipleVideoET()
{
    delete ui;
}

void MultipleVideoET::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
       if (!mimeData->hasUrls()) {
           return;
       }
       QList<QUrl> urls = mimeData->urls();
       foreach (QUrl url,urls){
           QMimeType mime = QMimeDatabase().mimeTypeForUrl(url);
           if (mime.inherits("video/x-msvideo")) {
                videoList.append(url.toLocalFile());
              }
       }
       //videoList = seznamVidei;
       qDebug()<<"Actual list of videos contains: "<<videoList;
       ui->selectedVideos->addItems(videoList);
}

void MultipleVideoET::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}

void MultipleVideoET::on_afewVideosPB_clicked()
{
    QStringList filenames = QFileDialog::getOpenFileNames(this,tr("Choose avi files"),
                            SharedVariables::getSharedVariables()->getPath("videosPath"),
                            tr("Video files (*.avi);;;") );
    if( !filenames.isEmpty() )
    {
        for (int i =0;i<filenames.count();i++)
        {
            ui->selectedVideos->addItem(filenames.at(i));
            videoList.append(filenames.at(i));
        }
    }
}

void MultipleVideoET::on_wholeFolderPB_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                  SharedVariables::getSharedVariables()->getPath("videosPath"),
                  QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    QDir chosenDirectory(dir);
    QStringList videosInDirectory = chosenDirectory.entryList(QStringList() << "*.avi" << "*.AVI",QDir::Files);
    if (!videosInDirectory.isEmpty())
    {
        for (int a = 0; a < videosInDirectory.count();a++)
        {
            videoList.append(videosInDirectory.at(a));
        }
        ui->selectedVideos->addItems(videosInDirectory);
    }
    qDebug()<<"videoList contains "<<videoList.count()<<" videos.";
}

void MultipleVideoET::on_analyzeVideosPB_clicked()
{
    if (runStatus){
        if (checkVideos()){
            First[1] = new qThreadFirstPart(analysedVideos,
                                            SharedVariables::getSharedVariables()->getVerticalAnomalyCoords(),
                                            SharedVariables::getSharedVariables()->getHorizontalAnomalyCoords(),
                                            SharedVariables::getSharedVariables()->getFrangiParameters(),
                                            SharedVariables::getSharedVariables()->getFrangiMargins(),
                                            SharedVariables::getSharedVariables()->getFrangiRatios());
            QObject::connect(First[1],SIGNAL(percentageCompleted(int)),ui->computationProgress,SLOT(setValue(int)));
            QObject::connect(First[1],SIGNAL(done(int)),this,SLOT(onDone(int)));
            QObject::connect(First[1],SIGNAL(typeOfMethod(int)),this,SLOT(movedToMethod(int)));
            QObject::connect(First[1],SIGNAL(actualVideo(int)),this,SLOT(newVideoProcessed(int)));
            QObject::connect(First[1],SIGNAL(unexpectedTermination(int,QString)),this,SLOT(onUnexpectedTermination(int,QString)));
            QObject::connect(this,SIGNAL(dataObtained_first()),First[1],SLOT(onDataObtained()));
            QObject::connect(First[1],SIGNAL(readyForFinish()),First[1],SLOT(deleteLater()));
            First[1]->start();

            initMaps();
            canProceed = true;
            emit calculationStarted();
            ui->analyzeVideosPB->setText(tr("Cancel"));
            runStatus = false;
        }
    }
    else{
        cancelAllCalculations();
        ui->analyzeVideosPB->setText(tr("Analyse videos"));
        ui->computationProgress->setValue(0);
        ui->actualMethod_label->setText("");
        ui->actualVideo_label->setText("");
        canProceed = false;
        runStatus = true;
    }
}

void MultipleVideoET::on_showResultsPB_clicked()
{
    QStringList inputVector;
    for (int a=0; a<videoList.count(); a++)
    {
        QString fullPath = videoList.at(a);
        QString slozka,jmeno,koncovka;
        processFilePath(fullPath,slozka,jmeno,koncovka);
        inputVector.append(jmeno);
    }
    GraphET_parent* graph = new GraphET_parent(inputVector,
                                               mapDouble["entropy"],
                                               mapDouble["tennengrad"],
                                               mapInt["firstEvalEntropy"],
                                               mapInt["firstEvalTennengrad"],
                                               mapInt["firstEval"],
                                               mapInt["secondEval"],
                                               mapInt["evaluation"],
                                               this);
    graph->setModal(true);
    graph->show();
}

void MultipleVideoET::keyPressEvent(QKeyEvent *event){
    switch (event->key()) {
        case Qt::Key_Delete:
            deleteSelectedFiles();
            break;
         default:
            QWidget::keyPressEvent(event);
    }
}

void MultipleVideoET::deleteSelectedFiles(){
    QList<QListWidgetItem*> selectedVideos = ui->selectedVideos->selectedItems();
    //qDebug()<<"Selected videos will be deleted: "<<selectedVideos;
    foreach (QListWidgetItem* item,selectedVideos)
    {
        int index = ui->selectedVideos->row(item);
        QString actuallyDeleted = item->text();
        if (actuallyDeleted == "")
            continue;
        videoList.removeAt(index);
        qDebug()<<"video "<<item->text()<<" deleted";
        delete ui->selectedVideos->takeItem(ui->selectedVideos->row(item));
    }
}

void MultipleVideoET::on_savePB_clicked()
{    
    for (int indexVideo=0; indexVideo<mapDouble["entropy"].length(); indexVideo++){
        QJsonDocument document;
        QJsonObject object;
        QString folder,filename,suffix;
        QString fullPath = videoList.at(indexVideo);
        processFilePath(fullPath,folder,filename,suffix);
        QString path = SharedVariables::getSharedVariables()->getPath("saveDatFilesPath")+"/"+filename+".dat";

        for (int parameter = 0; parameter < videoParameters.count(); parameter++){
            qDebug()<<videoParameters.at(parameter);
            if (parameter < 8){
                QVector<double> pomDouble = mapDouble[videoParameters.at(parameter)][indexVideo];
                QJsonArray pomArray = vector2array(pomDouble);
                object[videoParameters.at(parameter)] = pomArray;
            }
            else if (parameter >= 8 && parameter <= 12){
                QVector<int> pomInt = mapInt[videoParameters.at(parameter)][indexVideo];
                if (videoParameters.at(parameter) == "evaluation")
                    pomInt[framesReferencial[indexVideo]]=2;

                QJsonArray pomArray = vector2array(pomInt);
                object[videoParameters.at(parameter)] = pomArray;
            }
            else{
                if (videoParameters.at(parameter) == "VerticalAnomaly")
                    object[videoParameters.at(parameter)] = double(SharedVariables::getSharedVariables()->getHorizontalAnomalyCoords().y);
                else
                    object[videoParameters.at(parameter)] = double(SharedVariables::getSharedVariables()->getVerticalAnomalyCoords().x);
            }
        }
        document.setObject(object);
        QString documentString = document.toJson();
        QFile writer;
        writer.setFileName(path);
        writer.open(QIODevice::WriteOnly);
        writer.write(documentString.toLocal8Bit());
        writer.close();
    }
}

void MultipleVideoET::onDone(int thread){
    done(thread);
    if (thread == 1){
        qDebug()<<"First done, starting second...";

        Second[2] = new qThreadSecondPart(analysedVideos,
                                          badVideos,
                                          obtainedCutoffStandard,
                                          obtainedCutoffExtra,
                                          badFramesComplete,
                                          framesReferencial,false,areaMaximum);
        QObject::connect(Second[2],SIGNAL(done(int)),this,SLOT(onDone(int)));
        QObject::connect(Second[2],SIGNAL(percentageCompleted(int)),ui->computationProgress,SLOT(setValue(int)));
        QObject::connect(Second[2],SIGNAL(typeOfMethod(int)),this,SLOT(movedToMethod(int)));
        QObject::connect(Second[2],SIGNAL(actualVideo(int)),this,SLOT(newVideoProcessed(int)));
        QObject::connect(Second[2],SIGNAL(unexpectedTermination(int,QString)),this,SLOT(onUnexpectedTermination(int,QString)));
        QObject::connect(this,SIGNAL(dataObtained_second()),Second[2],SLOT(onDataObtained()));
        QObject::connect(Second[2],SIGNAL(readyForFinish()),Second[2],SLOT(deleteLater()));

        Second[2]->start();
        qDebug()<<"Started";
    }
    else if (thread == 2){
        qDebug()<<"Second done, starting third...";

        Third[3] = new qThreadThirdPart(analysedVideos,
                                        badVideos,
                                        badFramesComplete,
                                        mapInt["evaluation"],
                                        framesReferencial,
                                        averageCCcomplete,
                                        averageFWHMcomplete,
                                        obtainedCutoffStandard,
                                        obtainedCutoffExtra,false,areaMaximum);
        QObject::connect(Third[3],SIGNAL(done(int)),this,SLOT(onDone(int)));
        QObject::connect(Third[3],SIGNAL(percentageCompleted(int)),ui->computationProgress,SLOT(setValue(int)));
        QObject::connect(Third[3],SIGNAL(typeOfMethod(int)),this,SLOT(movedToMethod(int)));
        QObject::connect(Third[3],SIGNAL(actualVideo(int)),this,SLOT(newVideoProcessed(int)));
        QObject::connect(Third[3],SIGNAL(unexpectedTermination(int,QString)),this,SLOT(onUnexpectedTermination(int,QString)));
        QObject::connect(this,SIGNAL(dataObtained_third()),Third[3],SLOT(onDataObtained()));
        QObject::connect(Third[3],SIGNAL(readyForFinish()),Third[3],SLOT(deleteLater()));

        Third[3]->start();
        qDebug()<<"Started";
    }
    else if (thread == 3){
        qDebug()<<"Third done, starting fourth...";

        Fourth[4] = new qThreadFourthPart(analysedVideos,
                                          badVideos,
                                          mapInt["firstEval"],
                                          mapInt["evaluation"],
                                          CC_problematicFrames,
                                          FWHM_problematicFrames,
                                          mapDouble["POCX"],
                                          mapDouble["POCY"],
                                          mapDouble["angle"],
                                          mapDouble["FrangiX"],
                                          mapDouble["FrangiY"],
                                          mapDouble["FrangiEuklid"],
                                          averageCCcomplete,
                                          averageFWHMcomplete);
        QObject::connect(Fourth[4],SIGNAL(done(int)),this,SLOT(onDone(int)));
        QObject::connect(Fourth[4],SIGNAL(percentageCompleted(int)),ui->computationProgress,SLOT(setValue(int)));
        QObject::connect(Fourth[4],SIGNAL(typeOfMethod(int)),this,SLOT(movedToMethod(int)));
        QObject::connect(Fourth[4],SIGNAL(actualVideo(int)),this,SLOT(newVideoProcessed(int)));
        QObject::connect(this,SIGNAL(dataObtained_fourth()),Fourth[4],SLOT(onDataObtained()));
        QObject::connect(Fourth[4],SIGNAL(readyForFinish()),Fourth[4],SLOT(deleteLater()));

        Fourth[4]->start();
        qDebug()<<"Started";
    }
    else if (thread == 4){
        qDebug()<<"Fourth done, starting fifth";

        Fifth[5] = new qThreadFifthPart(analysedVideos,
                                        badVideos,
                                       obtainedCutoffStandard,
                                       obtainedCutoffExtra,
                                       mapDouble["POCX"],
                                       mapDouble["POCY"],
                                       mapDouble["angle"],
                                       mapDouble["FrangiX"],
                                       mapDouble["FrangiY"],
                                       mapDouble["FrangiEuklid"],
                                       false,
                                       mapInt["evaluation"],
                                       mapInt["secondEval"],
                                       framesReferencial,
                                       SharedVariables::getSharedVariables()->getFrangiParameters(),
                                        int(iterationCount),
                                        areaMaximum,
                                        rotationAngle,
                                        SharedVariables::getSharedVariables()->getFrangiMargins());
        QObject::connect(Fifth[5],SIGNAL(done(int)),this,SLOT(onDone(int)));
        QObject::connect(Fifth[5],SIGNAL(percentageCompleted(int)),ui->computationProgress,SLOT(setValue(int)));
        QObject::connect(Fifth[5],SIGNAL(typeOfMethod(int)),this,SLOT(movedToMethod(int)));
        QObject::connect(Fifth[5],SIGNAL(actualVideo(int)),this,SLOT(newVideoProcessed(int)));
        QObject::connect(Fifth[5],SIGNAL(unexpectedTermination(int,QString)),this,SLOT(onUnexpectedTermination(int,QString)));
        QObject::connect(this,SIGNAL(dataObtained_fifth()),Fifth[5],SLOT(onDataObtained()));
        QObject::connect(Fifth[5],SIGNAL(readyForFinish()),Fifth[5],SLOT(deleteLater()));

        Fifth[5]->start();
        qDebug()<<"Started";
    }
    else if (thread == 5){
        ui->showResultsPB->setEnabled(true);
        ui->savePB->setEnabled(true);
        ui->actualMethod_label->setText(tr("Fifth part done. Analysis completed"));
        qDebug()<<"Fifth done.";
        ui->analyzeVideosPB->setText(tr("Analyse videos"));
        emit calculationStopped();
        runStatus = true;
        First.clear();
        Second.clear();
        Third.clear();
        Fourth.clear();
        Fifth.clear();
    }
}

void MultipleVideoET::onUnexpectedTermination(int videoIndex, QString errorType){
    Q_UNUSED(videoIndex);
    localErrorDialogHandling[ui->analyzeVideosPB]->evaluate("left",errorType,"Video could not be analysed.");
    localErrorDialogHandling[ui->analyzeVideosPB]->show(false);
    if (errorType == "hardError"){
        cancelAllCalculations();
        emit calculationStopped();
    }
}

void MultipleVideoET::newVideoProcessed(int index)
{
    ui->actualVideo_label->setText("Analysing: "+videoList.at(index)+" ("+QString::number(index+1)+"/"+QString::number(videoList.size())+")");
}

void MultipleVideoET::movedToMethod(int metoda)
{
    if (metoda == 0)
        ui->actualMethod_label->setText(tr("1/5 Entropy and tennengrad computation"));
    if (metoda == 1)
        ui->actualMethod_label->setText(tr("2/5 Average correlation and FWHM"));
    if (metoda == 2)
        ui->actualMethod_label->setText(tr("3/5 First decision algorithm started"));
    if (metoda == 3)
        ui->actualMethod_label->setText(tr("4/5 Second decision algorithm started"));
    if (metoda == 4)
        ui->actualMethod_label->setText(tr("5/5 Third decision algorithm started"));
}

void MultipleVideoET::evaluateCorrectValues(){
    if (areaMaximumCorrect && rotationAngleCorrect && iterationCountCorrect){
        ui->analyzeVideosPB->setEnabled(true);
    }
    else
        ui->analyzeVideosPB->setEnabled(false);
}

void MultipleVideoET::on_areaMaximum_editingFinished()
{
    bool ok;
    double input = ui->areaMaximum->text().toDouble(&ok);
    if (ok){
        checkInputNumber(input,0.0,20.0,ui->areaMaximum,areaMaximum,areaMaximumCorrect);
        checkValuesPass();
    }
}

void MultipleVideoET::on_rotationAngle_editingFinished()
{
    bool ok;
    double input = ui->rotationAngle->text().toDouble(&ok);
    if (ok){
        checkInputNumber(input,0.0,0.5,ui->rotationAngle,rotationAngle,rotationAngleCorrect);
        checkValuesPass();
    }
}

void MultipleVideoET::on_iterationCount_editingFinished()
{
    bool ok;
    double input = ui->iterationCount->text().toDouble(&ok);
    if (ok){
        checkInputNumber(input,-1.0,0.0,ui->iterationCount,iterationCount,iterationCountCorrect);
        checkValuesPass();
    }
}

bool MultipleVideoET::checkVideos(){
    QVector<int> _badVideos;
    QStringList _temp;
    for (int var = 0; var < videoList.count(); var++) {
        cv::VideoCapture cap = cv::VideoCapture(videoList.at(var).toLocal8Bit().constData());
        if (!cap.isOpened()){
            _badVideos.push_back(var);
            ui->selectedVideos->item(var)->setBackgroundColor(Qt::red);
        }
        else{
            _temp.append(videoList.at(var));
            ui->selectedVideos->item(var)->setBackgroundColor(Qt::blue);
        }
    }
    if (_badVideos.length() == videoList.count()){
            localErrorDialogHandling[ui->analyzeVideosPB]->evaluate("center","softError",3);
            localErrorDialogHandling[ui->analyzeVideosPB]->show(false);
            return false;
    }
    else{
        analysedVideos = _temp;
        return true;
    }
}

void MultipleVideoET::showDialog(){
    if (ui->verticalAnomaly->isChecked())
    {
        QString fullPath = chosenVideoETSingle[0]+"/"+chosenVideoETSingle[1]+"."+chosenVideoETSingle[2];
        ClickImageEvent* markAnomaly = new ClickImageEvent(fullPath,1);
        markAnomaly->setModal(true);
        markAnomaly->show();
    }
    if (ui->horizontalAnomaly->isChecked())
    {
        QString fullPath = chosenVideoETSingle[0]+"/"+chosenVideoETSingle[1]+"."+chosenVideoETSingle[2];
        ClickImageEvent* markAnomaly = new ClickImageEvent(fullPath,2);
        markAnomaly->setModal(true);
        markAnomaly->show();
    }
}

void MultipleVideoET::disableWidgets(){
    ui->wholeFolderPB->setEnabled(false);
    ui->afewVideosPB->setEnabled(false);
    ui->verticalAnomaly->setEnabled(false);
    ui->horizontalAnomaly->setEnabled(false);
    ui->savePB->setEnabled(false);
    ui->showResultsPB->setEnabled(false);
}

void MultipleVideoET::enableWidgets(){
    ui->wholeFolderPB->setEnabled(true);
    ui->afewVideosPB->setEnabled(true);
    ui->verticalAnomaly->setEnabled(true);
    ui->horizontalAnomaly->setEnabled(true);
    ui->savePB->setEnabled(true);
    ui->showResultsPB->setEnabled(true);
}
