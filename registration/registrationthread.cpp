#include "registration/registrationthread.h"
#include "registration/multiPOC_Ai1.h"
#include "image_analysis/image_processing.h"
#include "image_analysis/correlation_coefficient.h"
#include "image_analysis/frangi_utilization.h"
#include "registration/phase_correlation_function.h"
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include "opencv2/imgproc/imgproc_c.h"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/highgui/highgui.hpp>

#include <QDebug>
#include <QTableWidgetItem>
#include "image_analysis/frangi_utilization.h"
#include <exception>
using cv::Mat;
using cv::VideoCapture;
using cv::Point3d;
using cv::Rect;

RegistrationThread::RegistrationThread(int& i_indexOfThread,
                                       QString &i_fullVideoPath,
                                       QString& i_nameOfVideo,
                                       QVector<double>& i_frangiParameters,
                                       QVector<int>& i_frameEvaluation,
                                       cv::Mat& i_referencialFrame,
                                       int& startFrame,
                                       int& stopFrame,
                                       int& i_iteration,
                                       double& i_areaMaximum,
                                       double& i_angle,
                                       int &i_horizAnomaly,
                                       int &i_vertAnomaly,
                                       bool i_scaleChange,
                                       QMap<QString, int> i_margins,
                                       QMap<QString, double> i_ratios,
                                       QObject *parent) : QThread(parent),
    referencialImage(i_referencialFrame),
    frangiParameters(i_frangiParameters),
    framesEvaluation(i_frameEvaluation),
    iteration(i_iteration),
    startingFrame(startFrame),
    stoppingFrame(stopFrame),
    maximalArea(i_areaMaximum),
    angle(i_angle),
    horizontalAnomaly(i_horizAnomaly),
    verticalAnomaly(i_vertAnomaly),
    scaling(i_scaleChange),
    threadIndex(i_indexOfThread),
    videoName(i_nameOfVideo),
    videoPath(i_fullVideoPath),
    margins(i_margins),
    ratios(i_ratios)
{
    emit setTerminationEnabled(true);
    capture = cv::VideoCapture(videoPath.toLocal8Bit().constData());
    QVector<double> pomD(static_cast<int>(capture.get(CV_CAP_PROP_FRAME_COUNT)),0.0);
    frangiX = pomD;frangiY = pomD;frangiEuklidean = pomD;finalPOCx = pomD;finalPOCy = pomD;maximalAngles = pomD;
    setTerminationEnabled(true);
    qDebug()<<"registration thread initialized";
}

QMap<QString,QVector<double>> RegistrationThread::provideResults()
{
    vectors["FrangiX"] = frangiX;
    vectors["FrangiY"] = frangiY;
    vectors["FrangiEuklid"] = frangiEuklidean;
    vectors["POCX"] = finalPOCx;
    vectors["POCY"] = finalPOCy;
    vectors["angle"] = maximalAngles;
    return vectors;
}

QVector<int> RegistrationThread::threadFrameRange(){
    QVector<int> output;
    output.push_back(startingFrame);
    output.push_back(stoppingFrame);
    return output;
}

bool registrateTheBest(cv::VideoCapture& i_cap,
                       cv::Mat& i_referencialFrame,
                       cv::Point3d i_coordsFrangiStandardReferencialReverse,
                       int i_index_translated,
                       int i_iteration,
                       double i_areaMaximum,
                       double i_angle,
                       cv::Rect& i_cutoutExtra,
                       cv::Rect& i_cutoutStandard,
                       bool i_scaleChanged,
                       QVector<double> &parametry_frangi,
                       QVector<double> &_pocX,
                       QVector<double> &_pocY,
                       QVector<double> &_frangiX,
                       QVector<double> &_frangiY,
                       QVector<double> &_frangiEucl,
                       QVector<double> &_maxAngles,
                       QMap<QString, int> i_margins);

bool fullRegistration(cv::VideoCapture& i_cap,
                      cv::Mat& i_referencialFrame,
                      int i_shiftedNo,
                      int i_iteration,
                      double i_areaMaximum,
                      double i_angle,
                      cv::Rect& i_cutoutExtra,
                      cv::Rect& i_cutoutStandard,
                      bool i_scaleChange,
                      cv::Mat& i_fullyRegistrated,
                      QVector<double> &i_pocX,
                      QVector<double> &i_pocY,
                      QVector<double> &i_maxAngles);

bool imagePreprocessing(cv::Mat &i_referencialFrame,
                        cv::Mat &i_preprocessed,
                        QVector<double> &i_frangiParameters,
                        cv::Point2f &i_verticalAnomaly,
                        cv::Point2f &i_horizontalAnomaly,
                        cv::Rect &i_anomalyArea,
                        cv::Rect &i_coutouExtra,
                        cv::Rect &i_cutoutStandard,
                        cv::VideoCapture &i_cap,
                        bool &i_scaleChange,
                        QMap<QString, int> i_margins,
                        QMap<QString, double> i_ratios);

bool registrationCorrection(cv::Mat& i_registratedFrame,
                            cv::Mat& i_frame,
                            cv::Mat& i_corrected,
                            cv::Rect& i_cutoutStandard,
                            cv::Point3d& i_correction,
                            double i_areaMaximum);

void RegistrationThread::run()
{
    /// define helpers for frame preprocessing
    //bool lightAnomalyPresent = false;
    //bool timeStampPresent = false;
    bool scaling=false;
    /// frame and video properties
    double width = capture.get(CV_CAP_PROP_FRAME_WIDTH);
    double height = capture.get(CV_CAP_PROP_FRAME_HEIGHT);

    if (verticalAnomaly >0.0f && verticalAnomaly < float(width))
    {
        obtainedVerticalAnomalyCoords.x = verticalAnomaly;
        obtainedVerticalAnomalyCoords.y = 0;
        //lightAnomalyPresent=true;
        scaling = true;
    }
    else
    {
        obtainedVerticalAnomalyCoords.x = 0.0f;
        obtainedVerticalAnomalyCoords.y = 0.0f;
    }
    if (horizontalAnomaly > 0.0f && horizontalAnomaly < float(height))
    {
        obtainedHorizontalAnomalyCoords.y = horizontalAnomaly;
        obtainedHorizontalAnomalyCoords.x = 0;
        //timeStampPresent=true;
        scaling=true;
    }
    else
    {
        obtainedHorizontalAnomalyCoords.y = 0.0f;
        obtainedHorizontalAnomalyCoords.x = 0.0f;
    }
    Rect pom(0,0,0,0);
    correl_standard = pom;
    correl_extra = pom;
    anomalyCutoff = pom;
    Point3d pt_temp(0.0,0.0,0.0);
    Mat preprocessed,preprocessed_vyrez;
    if(!imagePreprocessing(referencialImage,
                           preprocessed,
                           frangiParameters,
                           obtainedVerticalAnomalyCoords,
                           obtainedHorizontalAnomalyCoords,
                           anomalyCutoff,
                           correl_extra,
                           correl_standard,
                           capture,
                           scaling,
                           margins,
                           ratios)){
        emit errorDetected(threadIndex,QString(tr("Frame preprocessing failed for %1.")).arg(videoName));
        return;
    }
    preprocessed(correl_standard).copyTo(preprocessed_vyrez);
    Point3d frangiMaxReversal = frangi_analysis(preprocessed,2,2,0,"",1,pt_temp,frangiParameters,margins);
    if (frangiMaxReversal.z == 0.0){
        emit errorDetected(threadIndex,QString(tr("Frangi filter for referencial image failed for %1.")).arg(videoName));
        return;
    }
    //qDebug()<<"Frangi reversal: "<<frangiMaxReversal.x<<" "<<frangiMaxReversal.y;
    for (int indexFrame = startingFrame; indexFrame <= stoppingFrame; indexFrame++)
    {
        bool errorOccured = false;
        if (framesEvaluation[indexFrame] == 0){
            if (!registrateTheBest(capture,referencialImage,frangiMaxReversal,indexFrame,iteration,maximalArea,
                                   totalAngle,correl_extra,correl_standard,scaling,frangiParameters,
                                   finalPOCx,finalPOCy,frangiX,frangiY,frangiEuklidean,maximalAngles,margins)){
                errorOccured = true;
                continue;
            }
        }
        else if (framesEvaluation[indexFrame] == 2)
        {
            Mat shifted_temp;
            capture.set(CV_CAP_PROP_POS_FRAMES,indexFrame);
            if (capture.read(shifted_temp)!=1)
            {
                qWarning()<<"Frame "<<indexFrame<<" could not be read!";
                errorOccured = true;
                continue;
            }
            else
            {
                if (scaling == true)
                {
                    Mat shifted;
                    shifted_temp(correl_extra).copyTo(shifted);
                    Point3d frangiCoords_preprocessed_reverse = frangi_analysis(shifted,2,2,0,"",1,pt_temp,
                                                                                frangiParameters,margins);
                    if (frangiCoords_preprocessed_reverse.z == 0.0){
                        errorOccured = true;
                        continue;
                    }
                    frangiX[indexFrame] = frangiCoords_preprocessed_reverse.x;
                    frangiY[indexFrame] = frangiCoords_preprocessed_reverse.y;
                    frangiEuklidean[indexFrame] = 0.0;
                    //qDebug() << "Referencial frame "<<indexFrame<<" analysed.";
                    shifted_temp.release();
                    shifted.release();
                }
                else
                {
                    Point3d frangiCoords_preprocessed_reverse = frangi_analysis(shifted_temp,2,2,0,"",1,pt_temp,
                                                                                frangiParameters,margins);
                    if (frangiCoords_preprocessed_reverse.z == 0.0){
                        errorOccured = true;
                        continue;
                    }
                    //qDebug() << "Referencial frame "<<indexFrame<<" analysed.";
                    shifted_temp.release();
                    frangiX[indexFrame] = frangiCoords_preprocessed_reverse.x;
                    frangiY[indexFrame] = frangiCoords_preprocessed_reverse.y;
                    frangiEuklidean[indexFrame] = 0.0;
                }
                maximalAngles[indexFrame] = 0.0;
                finalPOCx[indexFrame] = 0.0;
                finalPOCy[indexFrame] = 0.0;
            }
        }
        else if (framesEvaluation[indexFrame] == 5)
        {
            Mat shifted;
            capture.set(CV_CAP_PROP_POS_FRAMES,indexFrame);
            if (capture.read(shifted)!=1)
            {
                qWarning()<<"Frame "<<indexFrame<<" could not be read!";
                errorOccured = true;
                continue;
            }
            else
            {
                //qDebug() << "Frame "<<indexFrame<<" written without any changes.";
                shifted.release();
                frangiX[indexFrame] = 999.0;
                frangiY[indexFrame] = 999.0;
                frangiEuklidean[indexFrame] = 999.0;
                maximalAngles[indexFrame] = 999;
                finalPOCx[indexFrame] = 999.0;
                finalPOCy[indexFrame] = 999.0;
            }
        }
        else if (framesEvaluation[indexFrame] == 1 || framesEvaluation[indexFrame] == 4)
        {
            Mat registratedFrame;
            if (!fullRegistration(capture,
                                  referencialImage,
                                  indexFrame,
                                  iteration,
                                  maximalArea,
                                  angle,
                                  correl_extra,
                                  correl_standard,
                                  scaling,
                                  registratedFrame,
                                  finalPOCx,
                                  finalPOCy,
                                  maximalAngles)){
                errorOccured = true;
                continue;
            }
            Mat shifted_temp;
            capture.set(CV_CAP_PROP_POS_FRAMES,indexFrame);
            if (capture.read(shifted_temp)!=1)
            {
                qWarning()<<"Frame "<<indexFrame<<" could not be read!";
                continue;
            }
            else
            {
                Point3d translationCorrection(0.0,0.0,0.0);
                cv::Mat fully_registratedFrame;
                if (!registrationCorrection(registratedFrame,preprocessed,fully_registratedFrame,
                                            correl_standard,
                                            translationCorrection,
                                            maximalArea)){
                    errorOccured = true;
                    continue;
                }

                if (translationCorrection.x != 0.0 || translationCorrection.y != 0.0)
                {
                    finalPOCx[indexFrame] += translationCorrection.x;
                    finalPOCy[indexFrame] += translationCorrection.y;
                    Point3d pt6 = pc_translation_hann(preprocessed,fully_registratedFrame,maximalArea);
                    if (std::abs(pt6.x)>=290 || std::abs(pt6.y)>=290)
                    {
                        pt6 = pc_translation(preprocessed,fully_registratedFrame,maximalArea);
                    }
                    //qDebug() << "After correction of translation Y: " << pt6.y <<" X: "<<pt6.x;
                }
                Point3d _tempTranslation = Point3d(finalPOCx[indexFrame],finalPOCy[indexFrame],0.0);
                //qDebug()<<" Translation: "<<finalPOCx[indexFrame]<<" "<<finalPOCy[indexFrame]<<" "<<maximalAngles[indexFrame];
                if (scaling == true)
                {
                    int rows = shifted_temp.rows;
                    int cols = shifted_temp.cols;
                    Mat shifted_original = frameTranslation(shifted_temp,_tempTranslation,rows,cols);
                    Mat finalFrame = frameRotation(shifted_original,maximalAngles[indexFrame]);
                    fully_registratedFrame.release();
                    finalFrame.release();
                    shifted_original.release();
                    shifted_temp.release();
                }
                else
                {
                    fully_registratedFrame.release();
                    shifted_temp.release();
                }
            }
        }
        else {
            emit x_coordInfo(indexFrame,0,QString::number(999.0));
            emit y_coordInfo(indexFrame,1,QString::number(999.0));
            emit angleInfo(indexFrame,2,QString::number(999.0));
            emit statusInfo(indexFrame,3,QString("error"));
        }
        if (errorOccured){
            emit x_coordInfo(indexFrame,0,QString::number(999.0));
            emit y_coordInfo(indexFrame,1,QString::number(999.0));
            emit angleInfo(indexFrame,2,QString::number(999.0));
            emit statusInfo(indexFrame,3,QString("error"));
        }
        else if (!errorOccured){
            /*qDebug()<<"Sending: frame: "<<indexFrame<<" "<<QString::number(finalPOCx[indexFrame])<<" "<<
                      QString::number(finalPOCy[indexFrame])<<" "<<QString::number(maximalAngles[indexFrame]);*/

            //QTableWidgetItem* newItem = new QTableWidgetItem(QString::number(finalPOCx[indexFrame]));
            //emit x_coordInfo(indexFrame,0,newItem);
            emit x_coordInfo(indexFrame,0,QString::number(finalPOCx[indexFrame]));
            //newItem = new QTableWidgetItem(QString::number(finalPOCy[indexFrame]));
            //emit y_coordInfo(indexFrame,1,newItem);
            emit y_coordInfo(indexFrame,1,QString::number(finalPOCy[indexFrame]));
            //newItem = new QTableWidgetItem(QString::number(maximalAngles[indexFrame]));
            //emit angleInfo(indexFrame,2,newItem);
            emit angleInfo(indexFrame,2,QString::number(maximalAngles[indexFrame]));
            emit statusInfo(indexFrame,3,QString("done"));
        }
        //qDebug()<<"####################################################";
    }
    if (framesEvaluation[startingFrame] == 0 || framesEvaluation[startingFrame] == 1 ||
            framesEvaluation[startingFrame] == 4){
        //qDebug()<<"Recalculating starting frame "<<startingFrame;
        if (registrateTheBest(capture,referencialImage,frangiMaxReversal,startingFrame,iteration,maximalArea,
                               totalAngle,correl_extra,correl_standard,scaling,frangiParameters,
                               finalPOCx,finalPOCy,frangiX,frangiY,frangiEuklidean,maximalAngles,margins)){
            emit x_coordInfo(startingFrame,0,QString::number(finalPOCx[startingFrame]));
            emit y_coordInfo(startingFrame,1,QString::number(finalPOCy[startingFrame]));
            emit angleInfo(startingFrame,2,QString::number(maximalAngles[startingFrame]));
            emit statusInfo(startingFrame,3,QString("done"));
            qDebug()<<QString::number(frangiX[startingFrame])<<QString::number(frangiY[startingFrame]);
        }
    }
    emit allWorkDone(threadIndex);
}

void RegistrationThread::dataObtained(){
    emit readyForFinish(threadIndex);
}

bool registrateTheBest(cv::VideoCapture& i_cap,
                       cv::Mat& i_referencialFrame,
                       cv::Point3d i_coordsFrangiStandardReferencialReverse,
                       int i_index_translated,
                       int i_iteration,
                       double i_areaMaximum,
                       double i_angle,
                       cv::Rect& i_cutoutExtra,
                       cv::Rect& i_cutoutStandard,
                       bool i_scaleChanged,
                       QVector<double> &parametry_frangi,
                       QVector<double> &_pocX,
                       QVector<double> &_pocY,
                       QVector<double> &_frangiX,
                       QVector<double> &_frangiY,
                       QVector<double> &_frangiEucl,
                       QVector<double> &_maxAngles,
                       QMap<QString,int> i_margins)
{
    Mat fully_registratedFrame = cv::Mat::zeros(cv::Size(i_referencialFrame.cols,i_referencialFrame.rows), CV_32FC3);

    if (!fullRegistration(i_cap,
                          i_referencialFrame,
                          i_index_translated,
                          i_iteration,
                          i_areaMaximum,
                          i_angle,
                          i_cutoutExtra,
                          i_cutoutStandard,
                          i_scaleChanged,
                          fully_registratedFrame,
                          _pocX,_pocY,_maxAngles)){
        qWarning()<< "Registration error. Frame "<<i_index_translated<<" was not registrated.";
        _frangiX[i_index_translated]=999.0;
        _frangiY[i_index_translated]=999.0;
        _frangiEucl[i_index_translated]=999.0;
        fully_registratedFrame.release();
        return false;
    }
    else
    {
        //qDebug()<<"Full registration correct.";
        Mat refSnimek_vyrez;
        i_referencialFrame(i_cutoutStandard).copyTo(refSnimek_vyrez);
        int rows = i_referencialFrame.rows;
        int cols = i_referencialFrame.cols;
        if (std::abs(_pocX[i_index_translated]) == 999.0)
        {
            qWarning()<< "Frame "<<i_index_translated<<" written without changes.";
            fully_registratedFrame.release();
            _frangiX[i_index_translated]=999.0;
            _frangiY[i_index_translated]=999.0;
            _frangiEucl[i_index_translated]=999.0;
            return false;
        }
        else
        {
            Mat shifted_temp;
            i_cap.set(CV_CAP_PROP_POS_FRAMES,i_index_translated);
            if (i_cap.read(shifted_temp)!=1)
            {
                qWarning()<<"Frame "<<i_index_translated<<" could not be read!";
            }
            Mat interresult32f,interresult32f_vyrez,shifted;
            if (i_scaleChanged == true)
            {
                shifted_temp(i_cutoutExtra).copyTo(shifted);
            }
            else
            {
                shifted_temp.copyTo(shifted);
            }

            Point3d translationCorrection(0.0,0.0,0.0);
            Mat registratedCorrectedFrame = cv::Mat::zeros(cv::Size(i_referencialFrame.cols,i_referencialFrame.rows), CV_32FC3);
            if (!registrationCorrection(fully_registratedFrame,i_referencialFrame,registratedCorrectedFrame,
                                        i_cutoutStandard,translationCorrection,i_areaMaximum)){
                qWarning()<<"Frame "<<i_index_translated<<" - registration correction failed";
                return false;
            }
            else{
                if (translationCorrection.x > 0.0 || translationCorrection.y > 0.0)
                {
                    //qDebug()<<"Correction: "<<translationCorrection.x<<" "<<translationCorrection.y;
                    _pocX[i_index_translated] += translationCorrection.x;
                    _pocY[i_index_translated] += translationCorrection.y;
                    Point3d pt6 = pc_translation_hann(i_referencialFrame,registratedCorrectedFrame,i_areaMaximum);
                    if (std::abs(pt6.x)>=290 || std::abs(pt6.y)>=290)
                    {
                        pt6 = pc_translation(i_referencialFrame,registratedCorrectedFrame,i_areaMaximum);
                    }
                    //qDebug()<<"Checking translation after correction: "<<pt6.x<<" "<<pt6.y;
                    _pocX[i_index_translated] += pt6.x;
                    _pocY[i_index_translated] += pt6.y;
                }

                registratedCorrectedFrame.copyTo(interresult32f);
                transformMatTypeTo32C1(interresult32f);
                interresult32f(i_cutoutStandard).copyTo(interresult32f_vyrez);
                double R1 = calculateCorrCoef(i_referencialFrame,registratedCorrectedFrame,i_cutoutStandard);
                Point3d _tempTranslation = Point3d(_pocX[i_index_translated],_pocY[i_index_translated],0.0);

                Point3d frangiCoords_registrated_reverse = frangi_analysis(registratedCorrectedFrame,2,2,0,"",2,
                                                                           _tempTranslation,parametry_frangi,i_margins);

                _frangiX[i_index_translated] = frangiCoords_registrated_reverse.x;
                _frangiY[i_index_translated] = frangiCoords_registrated_reverse.y;
                double yydef = i_coordsFrangiStandardReferencialReverse.x - frangiCoords_registrated_reverse.x;
                double xxdef = i_coordsFrangiStandardReferencialReverse.y - frangiCoords_registrated_reverse.y;
                double diffSum = std::pow(xxdef,2.0) + std::pow(yydef,2.0);
                double euklid = std::sqrt(diffSum);
                _frangiEucl[i_index_translated] = euklid;

                Point3d finalTranslation(0.0,0.0,0.0);
                finalTranslation.y = _pocY[i_index_translated] - yydef;
                finalTranslation.x = _pocX[i_index_translated] - xxdef;
                finalTranslation.z = 0;
                xxdef = 0.0;
                yydef = 0.0;
                diffSum = 0.0;
                euklid = 0.0;

                Mat shifted2 = frameTranslation(shifted,finalTranslation,rows,cols);
                Mat finalFrame = frameRotation(shifted2,_maxAngles[i_index_translated]);
                //Mat finalFrame_32f,finalFrame_32f_vyrez;
                /*finalFrame.copyTo(finalFrame_32f);
                transformMatTypeTo32C1(finalFrame_32f);
                finalFrame_32f(i_cutoutStandard).copyTo(finalFrame_32f_vyrez);
                finalFrame_32f.release();*/
                shifted2.release();
                double R2 = calculateCorrCoef(i_referencialFrame,finalFrame,i_cutoutStandard);
                // finalFrame_32f_vyrez.release();
                if (R1 >= R2)
                {
                    //qDebug()<< "Frame "<<i_index_translated<<" written after standard registration.";
                    //qDebug()<<"R1: "<<R1<<" R2: "<<R2;
                    //qDebug()<<"Translation: "<<_pocX[i_index_translated]<<" "<<_pocY[i_index_translated]<<" "<<_maxAngles[i_index_translated];
                    if (i_scaleChanged == true)
                    {
                        int rows = shifted_temp.rows;
                        int cols = shifted_temp.cols;
                        Mat shifted_original = frameTranslation(shifted_temp,_tempTranslation,rows,cols);
                        Mat finalFrame2 = frameRotation(shifted_original,_maxAngles[i_index_translated]);
                        fully_registratedFrame.release();
                        registratedCorrectedFrame.release();
                        finalFrame2.release();
                        finalFrame.release();
                        shifted_original.release();
                        shifted_temp.release();
                    }
                    else
                    {
                        fully_registratedFrame.release();
                        finalFrame.release();
                    }
                }
                else
                {
                    qDebug()<< "Frame "<<i_index_translated<<" written after vein analysis";
                    qDebug()<<" Translation: "<<finalTranslation.x<<" "<<finalTranslation.y<<" "<<_maxAngles[i_index_translated];
                    if (i_scaleChanged == true)
                    {
                        int rows = shifted_temp.rows;
                        int cols = shifted_temp.cols;
                        Mat shifted_original = frameTranslation(shifted_temp,finalTranslation,rows,cols);
                        Mat finalFrame2 = frameRotation(shifted_original,_maxAngles[i_index_translated]);
                        fully_registratedFrame.release();
                        registratedCorrectedFrame.release();
                        finalFrame2.release();
                        finalFrame.release();
                        shifted_original.release();
                        shifted_temp.release();
                    }
                    else
                    {
                        fully_registratedFrame.release();
                        finalFrame.release();
                    }
                    _pocX[i_index_translated] = finalTranslation.x;
                    _pocY[i_index_translated] = finalTranslation.y;
                }
                interresult32f.release();
                interresult32f_vyrez.release();
                return true;
            }
        }
    }
};

bool fullRegistration(cv::VideoCapture& i_cap,
                      cv::Mat& i_referencialFrame,
                      int i_shiftedNo,
                      int i_iteration,
                      double i_areaMaximum,
                      double i_angle,
                      cv::Rect& i_cutoutExtra,
                      cv::Rect& i_cutoutStandard,
                      bool i_scaleChange,
                      cv::Mat& i_fullyRegistrated,
                      QVector<double> &i_pocX,
                      QVector<double> &i_pocY,
                      QVector<double> &i_maxAngles)
{
    try {
        Mat shifted_temp;
        i_cap.set(CV_CAP_PROP_POS_FRAMES,i_shiftedNo);
        double totalAngle = 0.0;
        if(!i_cap.read(shifted_temp))
            return false;

        transformMatTypeTo8C3(i_referencialFrame);
        transformMatTypeTo8C3(shifted_temp);
        int rows = i_referencialFrame.rows;
        int cols = i_referencialFrame.cols;
        Mat hann;
        createHanningWindow(hann, i_referencialFrame.size(), CV_32FC1);
        Mat referencialFrame_32f,referencialFrame_vyrez;
        i_referencialFrame.copyTo(referencialFrame_32f);
        transformMatTypeTo32C1(referencialFrame_32f);
        referencialFrame_32f(i_cutoutStandard).copyTo(referencialFrame_vyrez);
        Mat shifted, shifted_vyrez;
        if (i_scaleChange == true)
        {
            shifted_temp(i_cutoutExtra).copyTo(shifted);
            shifted(i_cutoutStandard).copyTo(shifted_vyrez);
            shifted_temp.release();
        }
        else
        {
            shifted_temp.copyTo(shifted);
            shifted(i_cutoutStandard).copyTo(shifted_vyrez);
            shifted_temp.release();
        }
        Mat shifted_32f;
        shifted.copyTo(shifted_32f);
        transformMatTypeTo32C1(shifted_32f);

        Mat registrated1;
        Point3d pt1(0.0,0.0,0.0);
        if (i_scaleChange == true)
        {
            pt1 = pc_translation_hann(referencialFrame_32f,shifted_32f,i_areaMaximum);
            if (std::abs(pt1.x)>=290 || std::abs(pt1.y)>=290)
                pt1 = pc_translation(referencialFrame_32f,shifted_32f,i_areaMaximum);

            if (std::abs(pt1.x)>=290 || std::abs(pt1.y)>=290)
                pt1 = pc_translation(referencialFrame_vyrez,shifted_vyrez,i_areaMaximum);
        }
        if (i_scaleChange == false)
        {
            pt1 = pc_translation_hann(referencialFrame_32f,shifted_32f,i_areaMaximum);
        }

        if (pt1.x>=55 || pt1.y>=55)
        {
            i_pocX[i_shiftedNo] = 999.0;
            i_pocY[i_shiftedNo] = 999.0;
            i_maxAngles[i_shiftedNo] = 999.0;
            return false;
        }
        else
        {
            //qDebug()<<"Filling pt1";
            i_pocX[i_shiftedNo] = pt1.x;
            i_pocY[i_shiftedNo] = pt1.y;
            //qDebug()<<"pt1 filled.";
            //if (cislo_shifted == 0)
                //qDebug()<<"PT1: "<<pt1.x<<" "<<pt1.y;
            registrated1 = frameTranslation(shifted,pt1,rows,cols);
            cv::Mat registrated1_32f_rotace,registrated1_32f,registrated1_vyrez;
            registrated1.copyTo(registrated1_32f);
            transformMatTypeTo32C1(registrated1_32f);
            Point3d rotation_result = pc_rotation(referencialFrame_32f,registrated1_32f,i_angle,pt1.z,pt1);
            if (std::abs(rotation_result.y) > i_angle)
                rotation_result.y=0;

            i_maxAngles[i_shiftedNo] = rotation_result.y;
            registrated1_32f_rotace = frameRotation(registrated1_32f,rotation_result.y);
            registrated1_32f_rotace(i_cutoutStandard).copyTo(registrated1_vyrez);

            Point3d pt2(0.0,0.0,0.0);
            pt2 = pc_translation(referencialFrame_vyrez,registrated1_vyrez,i_areaMaximum);
            if (pt2.x >= 55 || pt2.y >= 55)
            {
                i_pocX[i_shiftedNo] = 999.0;
                i_pocY[i_shiftedNo] = 999.0;
                i_maxAngles[i_shiftedNo] = 999.0;
                registrated1.copyTo(i_fullyRegistrated);
                registrated1.release();
                return false;
            }
            else
            {
                double sigma_gauss = 1/(std::sqrt(2*CV_PI)*pt2.z);
                double FWHM = 2*std::sqrt(2*std::log(2)) * sigma_gauss;
                //qDebug()<<"FWHM for "<<cislo_shifted<<" = "<<FWHM;
                registrated1.release();
                registrated1_32f.release();
                registrated1_vyrez.release();
                //if (cislo_shifted == 0)
                    //qDebug()<<"PT2: "<<pt2.x<<" "<<pt2.y;

                Point3d pt3(0.0,0.0,0.0);
                pt3.x = pt1.x+pt2.x;
                pt3.y = pt1.y+pt2.y;
                pt3.z = pt2.z;
                i_pocX[i_shiftedNo] = pt3.x;
                i_pocY[i_shiftedNo] = pt3.y;
                Mat registrated2 = frameTranslation(shifted,pt3,rows,cols);
                Mat registrated2_32f,registrated2_vyrez;
                registrated2.copyTo(registrated2_32f);
                transformMatTypeTo32C1(registrated2_32f);
                Mat registrated2_rotace = frameRotation(registrated2_32f,rotation_result.y);
                registrated2_rotace(i_cutoutStandard).copyTo(registrated2_vyrez);
                Mat interresult_vyrez,interresult;
                registrated2_rotace.copyTo(interresult);
                registrated2_vyrez.copyTo(interresult_vyrez);
                //registrated2.release();
                registrated2_vyrez.release();
                registrated2_32f.release();
                registrated2_rotace.release();
                totalAngle+=rotation_result.y;
                rotation_result.y = 0;
                int maxIterationCount = 0;
                if (i_iteration == -1)
                {
                    if (FWHM <= 20){maxIterationCount = 2;}
                    else if (FWHM > 20 && FWHM <= 30){maxIterationCount = 4;}
                    else if (FWHM > 30 && FWHM <= 35){maxIterationCount = 6;}
                    else if (FWHM > 35 && FWHM <= 40){maxIterationCount = 8;}
                    else if (FWHM > 40 && FWHM <= 45){maxIterationCount = 10;}
                    else if (FWHM > 45){maxIterationCount = 5;};
                }
                if (i_iteration >= 1)
                {
                    maxIterationCount = i_iteration;
                }
                for (int i = 0; i < maxIterationCount; i++)
                {
                    Point3d rotace_ForLoop(0.0,0.0,0.0);
                    rotace_ForLoop = pc_rotation(i_referencialFrame,interresult,i_angle,pt3.z,pt3);
                    if (std::abs(rotace_ForLoop.y) > i_angle)
                        rotace_ForLoop.y = 0.0;
                    else if (std::abs(totalAngle+rotace_ForLoop.y)>i_angle)
                        rotace_ForLoop.y=0.0;
                    else
                        totalAngle+=rotace_ForLoop.y;

                    Mat rotatedFrame;
                    if (rotace_ForLoop.y != 0.0)
                        rotatedFrame = frameRotation(interresult,rotace_ForLoop.y);
                    else
                        rotatedFrame = interresult;

                    rotace_ForLoop.y = 0.0;
                    Mat rotatedFrame_vyrez;
                    rotatedFrame(i_cutoutStandard).copyTo(rotatedFrame_vyrez);
                    rotatedFrame.release();
                    Point3d pt4(0.0,0.0,0.0);
                    pt4 = pc_translation(referencialFrame_vyrez,rotatedFrame_vyrez,i_areaMaximum);
                    rotatedFrame_vyrez.release();
                    if (pt4.x >= 55 || pt4.y >= 55)
                    {
                        registrated2.copyTo(i_fullyRegistrated);
                        //qDebug()<<"Frame "<<cislo_shifted<<" terminated because \"condition 55\" reached.";
                        break;
                    }
                    else
                    {
                        pt3.x += pt4.x;
                        pt3.y += pt4.y;
                        pt3.z = pt4.z;
                        //if (cislo_shifted == 0)
                            //qDebug()<<"PT3 loop: "<<pt3.x<<" "<<pt3.y;
                        i_pocX[i_shiftedNo] = pt3.x;
                        i_pocY[i_shiftedNo] = pt3.y;
                        i_maxAngles[i_shiftedNo] = totalAngle;
                        Mat shifted_temp = frameTranslation(shifted,pt3,rows,cols);
                        Mat rotatedFrame_temp = frameRotation(shifted_temp,totalAngle);
                        shifted_temp.release();
                        rotatedFrame_temp.copyTo(interresult);
                        transformMatTypeTo32C1(rotatedFrame_temp);
                        rotatedFrame_temp.release();
                    }
                }
                interresult.copyTo(i_fullyRegistrated);
                return true;
            }
        }
    } catch (std::exception &e) {
        qWarning()<<"Full registration error: "<<e.what();
        i_pocX[i_shiftedNo] = 999.0;
        i_pocY[i_shiftedNo] = 999.0;
        i_maxAngles[i_shiftedNo] = 999.0;
        return false;
    }
};

bool imagePreprocessing(cv::Mat &i_referencialFrame,
                        cv::Mat &i_preprocessed,
                        QVector<double> &i_frangiParameters,
                        cv::Point2f &i_verticalAnomaly,
                        cv::Point2f &i_horizontalAnomaly,
                        cv::Rect &i_anomalyArea,
                        cv::Rect &i_coutouExtra,
                        cv::Rect &i_cutoutStandard,
                        cv::VideoCapture &i_cap,
                        bool &i_scaleChange,
                        QMap<QString,int> i_margins,
                        QMap<QString,double> i_ratios)
{
    try {
        cv::Point3d pt_temp(0.0,0.0,0.0);
        cv::Point3d frangiCoords(0.0,0.0,0.0);
        double width = i_cap.get(CV_CAP_PROP_FRAME_WIDTH);
        double height = i_cap.get(CV_CAP_PROP_FRAME_HEIGHT);
        bool verticalAnomalyPresence = false;
        bool horizontalAnomalyPresence = false;
        if (i_verticalAnomaly.y != 0.0f)
        {
            if (i_verticalAnomaly.y < float(width/2))
            {
                i_anomalyArea.y = int(i_verticalAnomaly.y);
                i_anomalyArea.height = int(height-int(i_verticalAnomaly.y));
                i_anomalyArea.width = int(i_cap.get(CV_CAP_PROP_FRAME_WIDTH));
            }
            if (i_verticalAnomaly.y > float(width/2))
            {
                i_anomalyArea.height = int(i_verticalAnomaly.y);
                i_anomalyArea.width = int(i_cap.get(CV_CAP_PROP_FRAME_WIDTH));
            }
            verticalAnomalyPresence = true;
        }
        if (i_horizontalAnomaly.x != 0.0f)
        {
            if (i_horizontalAnomaly.x < float(height/2))
            {
                i_anomalyArea.x = int(i_horizontalAnomaly.x);
                if (!verticalAnomalyPresence)
                    i_anomalyArea.height = int(i_cap.get(CV_CAP_PROP_FRAME_HEIGHT));
                i_anomalyArea.width = int(width-int(i_horizontalAnomaly.x));
            }
            if (i_horizontalAnomaly.x > float(width/2))
            {
                if (!verticalAnomalyPresence)
                    i_anomalyArea.height = int(i_cap.get(CV_CAP_PROP_FRAME_HEIGHT));
                i_anomalyArea.width = int(i_horizontalAnomaly.x);
            }
            horizontalAnomalyPresence = true;
        }
        if (verticalAnomalyPresence == true || horizontalAnomalyPresence == true)
            frangiCoords = frangi_analysis(i_referencialFrame(i_anomalyArea),1,1,0,"",3,pt_temp,
                                           i_frangiParameters,i_margins);
        else
            frangiCoords = frangi_analysis(i_referencialFrame,1,1,0,"",1,pt_temp,i_frangiParameters,i_margins);

        if (frangiCoords.z == 0.0)
        {
            return false;
        }
        else
        {
            bool needToChangeScale = false;
            int rows = i_referencialFrame.rows;
            int cols = i_referencialFrame.cols;
            int rowFrom = int(round(frangiCoords.y-i_ratios["top_r"]*frangiCoords.y));
            int rowTo = int(round(frangiCoords.y+i_ratios["bottom_r"]*(rows - frangiCoords.y)));
            int columnFrom = 0;
            int columnTo = 0;

            if (verticalAnomalyPresence == true && i_verticalAnomaly.y != 0.0f && int(i_verticalAnomaly.y)<(cols/2))
            {
                columnFrom = int(i_verticalAnomaly.y);
                needToChangeScale = true;
            }
            else
                columnFrom = int(round(frangiCoords.x-i_ratios["left_r"]*(frangiCoords.x)));

            if (verticalAnomalyPresence == true && i_verticalAnomaly.y != 0.0f &&  int(i_verticalAnomaly.y)>(cols/2))
            {
                columnTo = int(i_verticalAnomaly.y);
                needToChangeScale = true;
            }
            else
                columnTo = int(round(frangiCoords.x+i_ratios["right_r"]*(cols - frangiCoords.x)));

            int cutout_width = columnTo-columnFrom;
            int cutout_height = rowTo - rowFrom;

            if (needToChangeScale)
            {
                i_coutouExtra.x = columnFrom;
                i_coutouExtra.y = rowFrom;
                i_coutouExtra.width = cutout_width;
                i_coutouExtra.height = cutout_height;

                i_referencialFrame(i_coutouExtra).copyTo(i_preprocessed);

                frangiCoords = frangi_analysis(i_preprocessed,1,1,0,"",1,pt_temp,i_frangiParameters,i_margins);
                rows = i_preprocessed.rows;
                cols = i_preprocessed.cols;
                rowFrom = int(round(frangiCoords.y-0.9*frangiCoords.y));
                rowTo = int(round(frangiCoords.y+0.9*(rows - frangiCoords.y)));
                columnFrom = int(round(frangiCoords.x-0.9*(frangiCoords.x)));
                columnTo = int(round(frangiCoords.x+0.9*(cols - frangiCoords.x)));
                cutout_width = columnTo-columnFrom;
                cutout_height = rowTo - rowFrom;
                i_cutoutStandard.x = columnFrom;
                i_cutoutStandard.y = rowFrom;
                i_cutoutStandard.width = cutout_width;
                i_cutoutStandard.height = cutout_height;
                i_scaleChange = true;
            }
            else
            {
                i_cutoutStandard.x = int(round(frangiCoords.x-i_ratios["left_r"]*(frangiCoords.x)));
                i_cutoutStandard.y = int(round(frangiCoords.y-i_ratios["top_r"]*frangiCoords.y));
                rowTo = int(round(frangiCoords.y+i_ratios["bottom_r"]*(rows - frangiCoords.y)));
                columnTo = int(round(frangiCoords.x+i_ratios["right_r"]*(cols - frangiCoords.x)));
                i_cutoutStandard.width = columnTo-i_cutoutStandard.x;
                i_cutoutStandard.height = rowTo - i_cutoutStandard.y;

                i_referencialFrame.copyTo(i_preprocessed);
            }
            return true;
        }
    } catch (std::exception &e) {
        qWarning()<<"Image preprocessing error: "<<e.what();
        return false;
    }
};

bool registrationCorrection(cv::Mat& i_registratedFrame,
                            cv::Mat& i_frame,
                            cv::Mat &i_corrected,
                            cv::Rect& i_cutoutStandard,
                            cv::Point3d& i_correction,
                            double i_areaMaximum)
{
    try {
        Mat interresult,interresult32f,interresult32f_vyrez,obraz_vyrez;
        i_registratedFrame.copyTo(interresult);
        int rows = i_registratedFrame.rows;
        int cols = i_registratedFrame.cols;
        //interresult.copyTo(interresult32f);
        //transformMatTypeTo32C1(interresult32f);
        //interresult32f(vyrez_korelace_standard).copyTo(interresult32f_vyrez);
        i_frame(i_cutoutStandard).copyTo(obraz_vyrez);

        double R1 = calculateCorrCoef(i_frame,interresult,i_cutoutStandard);

        //interresult32f.release();
        //interresult32f_vyrez.release();

        Point3d shiftCorrection(0.0,0.0,0.0);
        shiftCorrection = pc_translation_hann(i_frame,interresult,i_areaMaximum);
        if (std::abs(shiftCorrection.x) > 290 || std::abs(shiftCorrection.y) > 290)
        {
            shiftCorrection = pc_translation(i_frame,interresult,i_areaMaximum);
        }
        if (std::abs(shiftCorrection.x) > 290 || std::abs(shiftCorrection.y) > 290)
        {
            shiftCorrection = pc_translation(obraz_vyrez,interresult32f_vyrez,i_areaMaximum);
        }
        i_corrected = frameTranslation(interresult,shiftCorrection,rows,cols);
        double R2 = calculateCorrCoef(i_frame,i_corrected,i_cutoutStandard);
        if ((R2 > R1) && ((std::abs(shiftCorrection.x) > 0.3) || (std::abs(shiftCorrection.y) > 0.3)))
        {
            i_correction = shiftCorrection;
            return true;
        }
        else
            return true;

    } catch (std::exception &e) {
        qWarning()<<"Registration correction error: "<<e.what();
        return false;
    }
}
