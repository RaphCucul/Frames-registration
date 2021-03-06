#ifndef REGISTRATETWO_H
#define REGISTRATETWO_H

#include <QWidget>
#include <QEvent>
#include <QMouseEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QSpacerItem>
#include <QGridLayout>
#include <QSignalMapper>
#include <QJsonObject>

#include "dialogs/errordialog.h"

#include <opencv2/opencv.hpp>
#include "opencv2/imgproc/imgproc_c.h"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core.hpp>

namespace Ui {
class RegistrateTwo;
}

class RegistrateTwo : public QWidget
{
    Q_OBJECT

public:
    explicit RegistrateTwo(QWidget *parent = nullptr);
    ~RegistrateTwo();

    /**
     * @brief The function clears options from Box layout connected with the combobox
     * @param layout.
     */
    void clearLayout(QGridLayout *layout);

    /**
     * @brief The function checks, if the path to the video folder exists. If so, the first video from the list of files
     * found in the folder is placed into the line edit.
     */
    void checkPaths();

public slots:
    //void GetClickCoordinates(QPointF hranice_anomalie);
    void chosenVideoPBWrapper();
    void chosenVideoPB_clicked(QWidget *W);
    void chosenReferenceImgPB_clicked(QWidget *W);
    void chosenTranslatedImgPB_clicked(QWidget *W);
    void ReferenceImgLE_textChanged(const QString &arg1);
    void TranslatedImgLE_textChanged(const QString &arg1);

signals:
    void checkRegistrationPass();
    void calculationStarted();
    void calculationStopped();

private slots:
    void Slot_VideoLE_textChanged(const QString &s);
    void ReferenceLE_textChanged(const QString &arg1);
    void TranslatedLE_textChanged(const QString &arg1);
    void on_areaMaximum_editingFinished();
    void on_rotationAngle_editingFinished();
    void on_iterationCount_editingFinished();
    void on_registrateTwo_clicked();
    void showDialog();
    void evaluateCorrectValues();
    void on_comboBox_activated(int index);

private:
    Ui::RegistrateTwo *ui;

    /**
     * @brief Functions initalize all widgets from "choice one" option
     *  which may appear in the horizontal box layout.
     */
    void initChoiceOneInnerWidgets();

    /**
     * @brief Functions initalize all widgets from "choice two" option
     *  which may appear in the horizontal box layout.
     */
    void initChoiceTwoInnerWidgets();

    /**
     * @brief Function populates the layout box with widgets used for frames registration.
     */
    void placeChoiceOneWidgets();

    /**
     * @brief Function populates the layout box with widgets used for image registration.
     */
    void placeChoiceTwoWidgets();

    /**
     * @brief Similar to analyseAndSaveFirst, this function loads images and save the name of the first one on the
     * list of found images
     * @param analysedFolder
     * @param whereToSave
     */
    void analyseAndSave(QString i_analysedFolder, QVector<QString>& i_whereToSave);

    /**
     * @brief Function checks, if input video or images can be loaded for next processing step.
     * @param path
     * @param method
     */
    bool evaluateVideoImageInput(QString i_path,QString i_method);

    /**
     * @brief Function check if the input values of area maximum, rotation angle and iteration count
     * are correct and if so, it calls the signal to check if all inputs are correct and the button of the main
     * computation algorithm can be then enabled.
     * @param input
     * @param lower
     * @param upper
     * @param editWidget
     * @param finalValue
     * @param evaluation
     */
    void checkInputNumber(double i_input,double i_lower,double i_upper,
                          QLineEdit* i_editWidget,double& i_finalValue,bool& i_evaluation);

    QVector<QString> chosenVideoAnalysis;
    QVector<QString> chosenReferencialImgAnalysis;
    QVector<QString> chosenTranslatedImgAnalysis;
    cv::VideoCapture cap;
    int referencialNumber = -1;
    int translatedNumber = -1;
    double iteration = -99.0;
    double areaMaximum = -99.0;
    double angle = -99.0;
    QString chosenVideo="";

    bool videoCorrect = false;
    bool referenceCorrect = false;
    bool referencialImgCorrect = false;
    bool translatedImgCorrect = false;
    bool translatedCorrect = false;
    bool areaMaximumCorrect = false;
    bool angleCorrect = false;
    bool iterationCorrect = false;
    bool anomalyPresent = false;
    bool scaleChanged = false;

    cv::Point3d frangiMaximumCoords;
    int formerIndex = 0;
    QLineEdit* chosenVideoLE;
    QPushButton* chosenVideoPB;
    QLineEdit* referenceNoLE;
    QLineEdit* translatedNoLE;

    QSpacerItem* horizontalSpacer1;
    QSpacerItem* horizontalSpacer2;

    QPushButton* chooseReferencialImagePB;
    QPushButton* chooseTranslatedImagePB;
    QLineEdit* referenceImgLE;
    QLineEdit* translatedImgLE;

    QHash<QWidget*,ErrorDialog*> localErrorDialogHandling;
};

#endif // REGISTRATETWO_H
