#include "sharedvariables.h"
#include "util/files_folders_operations.h"
#include "shared_staff/globalsettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QVector>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <exception>

SharedVariables* SharedVariables::g_sharedVariables = nullptr;

SharedVariables::SharedVariables()
{
    chosenActualPathes["videosPath"] = "";
    chosenActualPathes["saveVideosPath"] = "";
    chosenActualPathes["loadDatFilesPath"] = "";
    chosenActualPathes["saveDatFilesPath"] = "";
    chosenActualPathes["parametersFrangiFiltr"] = "";
    horizontalAnomalyCoords = cv::Point2d(0.0,0.0);
    verticalAnomalyCoords = cv::Point2d(0.0,0.0);
    detectedFrangiMaximum = cv::Point3d(0.0,0.0,0.0);
}

SharedVariables *SharedVariables::getSharedVariables(){
    if (g_sharedVariables == nullptr)
        g_sharedVariables = new SharedVariables();
    return g_sharedVariables;
}

QString SharedVariables::getPath(QString type) const{
    return chosenActualPathes[type];
}

void SharedVariables::setPath(QString type, QString path){
    qDebug()<<"Settings new path "<<path<<" for "<<type;
    chosenActualPathes[type] = path;
}

bool SharedVariables::processFrangiParameters(QString path){
    qDebug()<<"Processing Frangi parameters into a vector from path "<<path;
    bool processingResult = true;

    QFile file;
    file.setFileName(path+"/frangiParameters.json");
    QJsonObject FrangiParametersObject;
    FrangiParametersObject = readJson(file);
    if (FrangiParametersObject.isEmpty()){
        processingResult = false;
    }
    if (processingResult){
        size_frangi_opt(FrangiParametersList.count(),FrangiParameters);
        for (int a = 0; a < FrangiParametersList.count(); a++){
            if (!inicialization_frangi_opt(FrangiParametersObject,FrangiParametersList.at(a),FrangiParameters,a)){
                processingResult = false;
                break;
            }
        }
        for (int b = 0; b < FrangiMarginsList.count(); b++){
            frangiMargins[FrangiMarginsList.at(b)] = FrangiParametersObject[FrangiMarginsList.at(b)].toInt();
        }
        for (int b = 0; b < FrangiRatiosList.count(); b++){
            frangiRatios[FrangiRatiosList.at(b)] = FrangiParametersObject[FrangiRatiosList.at(b)].toDouble();
        }
    }    
    return processingResult;
}

QVector<double> SharedVariables::getFrangiParameters() const{
    return FrangiParameters;
}

double SharedVariables::getSpecificFrangiParameter(int parameter){
    return data_from_frangi_opt(parameter,FrangiParameters);
}

double SharedVariables::getSpecificFrangiParameter(QString parameter){
    return FrangiParametersMap[parameter];
}

void SharedVariables::size_frangi_opt(int size, QVector<double>& loadedParameters){
    loadedParameters = (QVector<double>(size));
}

bool SharedVariables::inicialization_frangi_opt(QJsonObject loadedObject, QString parameter, QVector<double>& loadedParameters,
                             int &position)
{
    try {
        loadedParameters[position] = loadedObject[parameter].toDouble();
        FrangiParametersMap[parameter] = loadedObject[parameter].toDouble();
        return true;
    } catch (std::exception& e) {
        qDebug()<<"An error occured when loading frangi parameters: "<<e.what();
        return false;
    }
}

double SharedVariables::data_from_frangi_opt(int position, QVector<double>& loadedParameters)
{
    return loadedParameters[position];
}

void SharedVariables::setSpecificFrangiParameter(int parameter, double value){
    FrangiParameters[parameter] = value;
    FrangiParametersMap[FrangiParametersList.at(parameter)] = value;
}

void SharedVariables::saveFrangiParameters(){
    QJsonDocument document;
    QJsonObject object;
    QString whereToSaveFrangi = chosenActualPathes["parametersFrangiFiltr"];
    qDebug()<<"Saving Frangi parameters to "<<whereToSaveFrangi;
    for (int indexParameter=0; indexParameter < FrangiParametersList.count(); indexParameter++)
        object[FrangiParametersList.at(indexParameter)] = FrangiParameters[indexParameter];
    for (int marginIndex = 0; marginIndex < FrangiMarginsList.count(); marginIndex++)
        object[FrangiMarginsList.at(marginIndex)] = frangiMargins[FrangiMarginsList.at(marginIndex)];
    for (int ratioIndex = 0; ratioIndex < FrangiMarginsList.count(); ratioIndex++)
        object[FrangiRatiosList.at(ratioIndex)] = frangiRatios[FrangiRatiosList.at(ratioIndex)];
    document.setObject(object);
    QString documentString = document.toJson();
    QFile writer;
    writer.setFileName(whereToSaveFrangi+"/frangiParameters.json");
    writer.open(QIODevice::WriteOnly);
    writer.write(documentString.toLocal8Bit());
    writer.close();
}

cv::Point3d SharedVariables::getFrangiMaximum(){
    return detectedFrangiMaximum;
}

void SharedVariables::setFrangiMaximum(cv::Point3d coordinates){
    detectedFrangiMaximum = coordinates;
}

cv::Point2d SharedVariables::getHorizontalAnomalyCoords(){
    return horizontalAnomalyCoords;
}

void SharedVariables::setHorizontalAnomalyCoords(QPointF coords){
    horizontalAnomalyCoords.x = double(coords.x());
    horizontalAnomalyCoords.y = double(coords.y());
}

cv::Point2d SharedVariables::getVerticalAnomalyCoords(){
    return verticalAnomalyCoords;
}

void SharedVariables::setVerticalAnomalyCoords(QPointF coords){
    verticalAnomalyCoords.x = double(coords.x());
    verticalAnomalyCoords.y = double(coords.y());
}

QMap<QString,double> SharedVariables::getFrangiRatios(){
    return frangiRatios;
}

void SharedVariables::setFrangiRatios(QMap<QString, double> i_ratios){
    frangiRatios = i_ratios;
}

QMap<QString,int> SharedVariables::getFrangiMargins(){
    return frangiMargins;
}

void SharedVariables::setFrangiMargins(QMap<QString, int> i_margins){
    frangiMargins = i_margins;
}
