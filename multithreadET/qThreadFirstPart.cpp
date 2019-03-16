#include "multithreadET/qThreadFirstPart.h"
#include "util/prace_s_vektory.h"
#include "util/souborove_operace.h"
#include "dialogy/multiplevideoet.h"
#include "analyza_obrazu/upravy_obrazu.h"
#include "analyza_obrazu/entropie.h"
#include "analyza_obrazu/korelacni_koeficient.h"
#include "analyza_obrazu/pouzij_frangiho.h"
#include "licovani/fazova_korelace_funkce.h"
#include "licovani/multiPOC_Ai1.h"
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include "opencv2/imgproc/imgproc_c.h"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/highgui/highgui.hpp>
#include <QDebug>
#include <QVector>
#include <QThread>

using cv::Mat;
using cv::Point3d;
qThreadFirstPart::qThreadFirstPart(QStringList _videosForAnalysis, cv::Point2d _verticalAnomalyCoords,
                                   cv::Point2d _horizontalAnomalyCoords, QVector<double> _FrangiParametersValues,
                                   QObject *parent):QThread(parent)
{
    processVideos = _videosForAnalysis;
    videoCount = _videosForAnalysis.count();
    verticalAnomaly = _verticalAnomalyCoords;
    horizontalAnomaly = _horizontalAnomalyCoords;
    FrangiParameters = _FrangiParametersValues;

    if (horizontalAnomaly.y != 0.0 || verticalAnomaly.x != 0.0)
        anomalyPresence = true;

}
void qThreadFirstPart::run()
{
    /// Firstly, entropy and tennengrad are computed for each frame of each video
    /// This task is indicated by emiting 0 in typeOfMethod
    emit typeOfMethod(0);
    emit percentageCompleted(0);
    percent = 0;
    bool errorOccured = false;
    for (int videoIndex = 0; videoIndex < processVideos.count(); videoIndex++)
    {
        QString fullPath = processVideos.at(videoIndex);
        QString slozka,jmeno,koncovka;
        processFilePath(fullPath,slozka,jmeno,koncovka);
        emit actualVideo(videoIndex);
        cv::VideoCapture cap = cv::VideoCapture(fullPath.toLocal8Bit().constData());
        if (!cap.isOpened()){
            emit unexpectedTermination("Cannot open a video for analysis",1,"hardError");
            errorOccured = true;
            break;
        }

        int frameCount = int(cap.get(CV_CAP_PROP_FRAME_COUNT));
        QVector<double> pomVecD(frameCount,0);
        QVector<int> pomVecI(frameCount,0);

        cv::Rect anomalyArea(0,0,0,0);
        QVector<double> entropyActual,tennengradActual;
        entropyActual.fill(0.0,frameCount);
        tennengradActual.fill(0.0,frameCount);

        double pocet_snimku_videa = (cap.get(CV_CAP_PROP_FRAME_COUNT));
        qDebug()<< "Video analysis: "<<videoIndex;
        for (double a = 0; a < pocet_snimku_videa; a++)
        {
            emit percentageCompleted(qRound((double(videoIndex)/double(videoCount))*100.0+1.0+((double(a)/pocet_snimku_videa)*100.0)/double(videoCount)));
            cv::Mat snimek;
            double hodnota_entropie;
            cv::Scalar hodnota_tennengrad;
            cap.set(CV_CAP_PROP_POS_FRAMES,(a));
            if (!cap.read(snimek)){
                QString errorMessage = QString("Frame number %1 could not be opened").arg(a);
                unexpectedTermination(errorMessage,1,"softError");
                continue;
            }
            else
            {
                kontrola_typu_snimku_8C3(snimek);
                calculateParametersET(snimek,hodnota_entropie,hodnota_tennengrad);
                double pom = hodnota_tennengrad[0];
                entropyActual[int(a)] = (hodnota_entropie);
                tennengradActual[int(a)] = (pom);
                snimek.release();
            }
        }

        entropyComplete.push_back(entropyActual);
        tennengradComplete.push_back(tennengradActual);
        qDebug()<<entropyComplete.size()<<tennengradComplete.size();

        /// Secondly, it is necessary to select proper maximal value for later selection of bad images
        /// after this procedure, windows for detail analysis of entropy and tennengrad vectors are computed
        /// medians of values in the windows are computed
        double correctEntropyMax = checkMaximum(entropyComplete[videoIndex]);
        double correctTennengradMax = checkMaximum(tennengradComplete[videoIndex]);
        QVector<double> windows_tennengrad,windows_entropy,windowsEntropy_medians,windowsTennengrad_medians;
        double restEntropy = 0.0,restTennengrad = 0.0;
        vectorWindows(entropyComplete[videoIndex],windows_entropy,restEntropy);
        vectorWindows(tennengradComplete[videoIndex],windows_tennengrad,restTennengrad);
        windowsEntropy_medians = mediansOfVector(entropyComplete[videoIndex],windows_entropy,restEntropy);
        windowsTennengrad_medians = mediansOfVector(tennengradComplete[videoIndex],windows_tennengrad,restTennengrad);

        /// Thirdly, values of entropy and tennengrad are evaluated and frames get mark good/bad, if they are
        /// or they are not suitable for image registration
        QVector<double> thresholdsEntropy(2,0);
        QVector<double> thresholdsTennengrad(2,0);
        thresholdsEntropy[0] = 0.01;thresholdsEntropy[1] = 0.01;
        thresholdsTennengrad[0] = 10;thresholdsTennengrad[1] = 10;
        double toleranceEntropy = 0.001;
        double toleranceTennengrad = 0.1;
        int dmin = 1;
        QVector<int> badFramesEntropy, badFramesTennengrad;
        QVector<double> nextAnalysisEntropy, nextAnalysisTennengrad;
        bool analysisOfValuesResult = analysisFunctionValues(entropyComplete[videoIndex],windowsEntropy_medians,windows_entropy,
                               correctEntropyMax,thresholdsEntropy,toleranceEntropy,dmin,restEntropy,
                               badFramesEntropy,nextAnalysisEntropy);
        if (!analysisOfValuesResult){
            emit unexpectedTermination("Entropy analysis of frames was interrupted by an error.",1,"hardError");
            errorOccured = true;
        }
        analysisOfValuesResult = analysisFunctionValues(tennengradComplete[videoIndex],windowsTennengrad_medians,windows_tennengrad,
                               correctTennengradMax,thresholdsTennengrad,toleranceTennengrad,dmin,restTennengrad,
                               badFramesTennengrad,nextAnalysisTennengrad);
        if (!analysisOfValuesResult){
            emit unexpectedTermination("Tennengrad analysis of frames was interrupted by an error.",1,"hardError");
            errorOccured = true;
        }
        int referencialImageNo = findReferencialNumber(correctEntropyMax,nextAnalysisEntropy,
                                                             entropyComplete[videoIndex]);
        referencialFrames.push_back(referencialImageNo);

        /// Fourth part - frangi filter is applied on the frame marked as the reference
        Mat reference, reference_vyrez;
        cap.set(CV_CAP_PROP_POS_FRAMES,referencialImageNo);
        cap.read(reference);

        cv::Rect cutoutExtra(0,0,0,0);
        cv::Rect cutoutStandard(0,0,0,0);
        cv::Point3d pt_temp(0.0,0.0,0.0);
        cv::Mat obraz;
        cv::Point3d frangi_bod(0,0,0);
        bool zmenaMeritka;
        bool preprocessingSuccessfull = preprocessingCompleteRegistration(
                                        reference,
                                        obraz,
                                        FrangiParameters,
                                        verticalAnomaly,
                                        horizontalAnomaly,
                                        frangi_bod,
                                        anomalyArea,
                                        cutoutStandard,
                                        cutoutExtra,
                                        cap,
                                        anomalyPresence,
                                        zmenaMeritka);
        if(!preprocessingSuccessfull){
            emit unexpectedTermination("Preprocessing failed",1,"hardError");
            errorOccured = true;
            break;
        }
        qDebug()<<"Standard "<<cutoutStandard.height<<" "<<cutoutStandard.width;
        qDebug()<<"Extra "<<cutoutExtra.height<<" "<<cutoutExtra.width;
        obtainedCutoffStandard = cutoutStandard;
        obtainedCutoffExtra = cutoutExtra;
        QVector<int> spatne_snimky_komplet = mergeVectors(badFramesEntropy,badFramesTennengrad);
        integrityCheck(spatne_snimky_komplet);
        QVector<int> hodnoceniSnimku;
        hodnoceniSnimku = pomVecI;
        if (restEntropy == 1.0)
        {
            spatne_snimky_komplet.push_back(frameCount-1);
        }
        int do_zbytku = int(restEntropy-1.0);
        if (restEntropy > 1)
        {
            while (do_zbytku >= 0)
            {
                spatne_snimky_komplet.push_back(frameCount-1-do_zbytku);
                do_zbytku -= 1;
            }
            spatne_snimky_komplet.push_back(frameCount-1);
        }
        for (int a = 0; a < spatne_snimky_komplet.size(); a++)
        {
            hodnoceniSnimku[int(spatne_snimky_komplet[a])] = 1;
        }
        framesFullCompleteDecision.append(hodnoceniSnimku);
        badFramesComplete.append(spatne_snimky_komplet);
        framesFirstFullCompleteEntropyEvaluation.append(badFramesEntropy);
        framesFirstFullCompleteTennengradEvaluation.append(badFramesTennengrad);
    }
    if (!errorOccured){
        emit percentageCompleted(100);
        emit done(1);
    }
}

QVector<QVector<double> > qThreadFirstPart::computedEntropy()
{
    return entropyComplete;
}

QVector<QVector<double> > qThreadFirstPart::computedTennengrad()
{
    return tennengradComplete;
}
QVector<int> qThreadFirstPart::estimatedReferencialFrames()
{
    return referencialFrames;
}
QVector<QVector<int> > qThreadFirstPart::computedFirstEntropyEvaluation()
{
    return framesFirstFullCompleteEntropyEvaluation;
}
QVector<QVector<int> > qThreadFirstPart::computedFirstTennengradEvalueation()
{
    return framesFirstFullCompleteTennengradEvaluation;
}
QVector<QVector<int>> qThreadFirstPart::computedBadFrames()
{
    return badFramesComplete;
}
cv::Rect qThreadFirstPart::computedCOstandard()
{
    return obtainedCutoffStandard;
}
QVector<QVector<int> > qThreadFirstPart::computedCompleteEvaluation()
{
    return framesFullCompleteDecision;
}
cv::Rect qThreadFirstPart::computedCOextra()
{
    return obtainedCutoffExtra;
}
/*QVector<QVector<double> > VicevlaknoveZpracovani::vypocitanePrvniRozhodnuti()
{
    return snimkyPrvniRozhodovaniKomplet;
}
QVector<QVector<double> > VicevlaknoveZpracovani::vypocitaneDruheRozhodnuti()
{
    return snimkyDruheRozhodovaniKomplet;
}
QVector<QVector<double> > VicevlaknoveZpracovani::vypocitaneFrangiX()
{
    return snimkyFrangiX;
}
QVector<QVector<double> > VicevlaknoveZpracovani::vypocitaneFrangiY()
{
    return snimkyFrangiY;
}
QVector<QVector<double> > VicevlaknoveZpracovani::vypocitaneFrangiEuklid()
{
    return snimkyFrangiEuklid;
}
QVector<QVector<double> > VicevlaknoveZpracovani::vypocitanePOCX()
{
    return snimkyPOCX;
}
QVector<QVector<double> > VicevlaknoveZpracovani::vypocitanePOCY()
{
    return snimkyPOCY;
}
QVector<QVector<double> > VicevlaknoveZpracovani::vypocitanyUhel()
{
    return snimkyUhel;
}
QVector<QVector<double>> VicevlaknoveZpracovani::vypocitaneOhodnoceni()
{
    return snimkyOhodnoceniKomplet;
}*/
/*void VicevlaknoveZpracovani::analyzaFWHM(cv::VideoCapture& capture,
                                           int referencni_snimek_cislo,
                                           bool zmena_meritka, double &vypocteneR, double &vypocteneFWHM,
                                           cv::Rect& vyrez_oblasti_standardni,
                                           cv::Rect& vyrez_oblasti_navic, QVector<double> &snimkyProSigma)
{

}*/

/*QVector<double> VicevlaknoveZpracovani::vectorForFWHM(QVector<double>& spatne_snimky_komplet, int pocet_snimku_videa)
{

}*/

/*void VicevlaknoveZpracovani::firstDecisionAlgorithm(QVector<double> &spatne_snimky_prvotni_ohodnoceni,
                       QVector<double> &hodnoceni_vsech_snimku_videa,
                       QVector<double> &POC_x,
                       QVector<double> &POC_y,
                       QVector<double> &uhel,
                       QVector<double> &frangi_x,
                       QVector<double> &frangi_y,
                       QVector<double> &frangi_euklid,
                       double prumerny_korelacni_koeficient,
                       double prumerne_FWHM,
                       cv::VideoCapture& cap,
                       cv::Mat& referencni_snimek,
                       cv::Rect& cutoutStandardni,
                       cv::Rect& vyrez_korelace_navic,
                       bool zmena_velikosti_obrazu,
                       QVector<double> &snimky_k_provereni_prvni,
                       QVector<double> &vypoctene_hodnoty_R,
                       QVector<double> &vypoctene_hodnoty_FWHM)
{
    for (int i = 0; i < spatne_snimky_prvotni_ohodnoceni.length(); i++)
    {
        Mat posunuty_temp,posunuty,posunuty_vyrez,slicovany,slicovany_vyrez;
        Mat obraz, obraz_vyrez;
        referencni_snimek.copyTo(obraz);
        obraz(cutoutStandardni).copyTo(obraz_vyrez);
        int rows = obraz.rows;
        int cols = obraz.cols;
        cap.set(CV_CAP_PROP_POS_FRAMES,spatne_snimky_prvotni_ohodnoceni[i]);
        if (cap.read(posunuty_temp) != 1)
        {
            qWarning()<<"Snimek "<<i<<" nelze slicovat!";
            POC_x[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999.0;
            POC_y[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999.0;
            uhel[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999.0;
            frangi_x[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999.0;
            frangi_y[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999.0;
            frangi_euklid[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999.0;
            continue;
        }
        if (zmena_velikosti_obrazu == true)
        {
            posunuty_temp(vyrez_korelace_navic).copyTo(posunuty);
            posunuty(cutoutStandardni).copyTo(posunuty_vyrez);
            posunuty_temp.release();
        }
        else
        {
            posunuty_temp.copyTo(posunuty);
            posunuty(vyrez_korelace_standardni).copyTo(posunuty_vyrez);
            posunuty_temp.release();
        }
        cv::Point3d pt(0,0,0);
        if (zmena_velikosti_obrazu == true)
        {
            pt = fk_translace_hann(obraz,posunuty);
            if (std::abs(pt.x)>=290 || std::abs(pt.y)>=290)
            {
                pt = fk_translace(obraz,posunuty);
            }
        }
        if (zmena_velikosti_obrazu == false)
        {
            pt = fk_translace_hann(obraz,posunuty);
        }
        if (pt.x >= 55 || pt.y >= 55)
        {
            qDebug()<< "Snimek "<< spatne_snimky_prvotni_ohodnoceni[i]<< " nepripusten k analyze.";
            hodnoceni_vsech_snimku_videa[int(spatne_snimky_prvotni_ohodnoceni[i])] = 5;
            POC_x[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
            POC_y[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
            uhel[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
            frangi_x[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
            frangi_y[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
            frangi_euklid[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
            posunuty.release();
        }
        else
        {
            double sigma_gauss = 0.0;
            sigma_gauss = 1/(std::sqrt(2*CV_PI)*pt.z);
            double FWHM = 0.0;
            FWHM = 2*std::sqrt(2*std::log(2)) * sigma_gauss;
            slicovany = translace_snimku(posunuty,pt,rows,cols);
            slicovany(vyrez_korelace_standardni).copyTo(slicovany_vyrez);
            double R = vypocet_KK(obraz,slicovany,vyrez_korelace_standardni);
            qDebug() <<spatne_snimky_prvotni_ohodnoceni[i]<< "R " << R <<" a FWHM " << FWHM;
            slicovany.release();
            slicovany_vyrez.release();
            posunuty.release();
            double rozdilnostKK = prumerny_korelacni_koeficient-R;
            double rozdilnostFWHM = prumerne_FWHM-FWHM;
            if ((std::abs(rozdilnostKK) < 0.02) && (FWHM < prumerne_FWHM)) //1.
            {
                qDebug()<< "Snimek "<< spatne_snimky_prvotni_ohodnoceni[i]<< " je vhodny ke slicovani.";
                hodnoceni_vsech_snimku_videa[int(spatne_snimky_prvotni_ohodnoceni[i])] = 0.0;
                continue;
            }
            else if (R > prumerny_korelacni_koeficient && (std::abs(rozdilnostFWHM)<=2||(FWHM < prumerne_FWHM))) //5.
            {
                qDebug()<< "Snimek "<< spatne_snimky_prvotni_ohodnoceni[i]<< " je vhodny ke slicovani.";
                hodnoceni_vsech_snimku_videa[int(spatne_snimky_prvotni_ohodnoceni[i])] = 0.0;
                continue;
            }
            else if (R >= prumerny_korelacni_koeficient && FWHM > prumerne_FWHM) //4.
            {
                qDebug()<< "Snimek "<< spatne_snimky_prvotni_ohodnoceni[i]<< " je vhodny ke slicovani.";
                hodnoceni_vsech_snimku_videa[int(spatne_snimky_prvotni_ohodnoceni[i])] = 0.0;
                continue;
            }
            else if ((std::abs(rozdilnostKK) <= 0.02) && (FWHM > prumerne_FWHM)) //2.
            {
                qDebug()<< "Snimek "<< spatne_snimky_prvotni_ohodnoceni[i]<< " bude proveren.";
                snimky_k_provereni_prvni.push_back(int(spatne_snimky_prvotni_ohodnoceni[i]));
                vypoctene_hodnoty_FWHM.push_back(FWHM);
                vypoctene_hodnoty_R.push_back(R);
                continue;
            }
            else if ((rozdilnostKK > 0.02) && (rozdilnostKK < 0.18)) //3.
            {
                qDebug()<< "Snimek "<< spatne_snimky_prvotni_ohodnoceni[i]<< " bude proveren.";
                snimky_k_provereni_prvni.push_back(int(spatne_snimky_prvotni_ohodnoceni[i]));
                vypoctene_hodnoty_FWHM.push_back(FWHM);
                vypoctene_hodnoty_R.push_back(R);
                continue;
            }
            else if ((rozdilnostKK >= 0.05 && rozdilnostKK < 0.18) && ((FWHM < prumerne_FWHM) || prumerne_FWHM > 35.0)) //6.
            {
                qDebug()<< "Snimek "<< spatne_snimky_prvotni_ohodnoceni[i]<< " bude proveren.";
                snimky_k_provereni_prvni.push_back(int(spatne_snimky_prvotni_ohodnoceni[i]));
                vypoctene_hodnoty_FWHM.push_back(FWHM);
                vypoctene_hodnoty_R.push_back(R);
                continue;
            }
            else if ((rozdilnostKK >= 0.05 && rozdilnostKK < 0.18) && (FWHM <= (prumerne_FWHM+10))) //8.
            {
                qDebug()<< "Snimek "<< spatne_snimky_prvotni_ohodnoceni[i]<< " bude proveren.";
                snimky_k_provereni_prvni.push_back(int(spatne_snimky_prvotni_ohodnoceni[i]));
                vypoctene_hodnoty_FWHM.push_back(FWHM);
                vypoctene_hodnoty_R.push_back(R);
                continue;
            }

            else if ((rozdilnostKK >= 0.2) && (FWHM > (prumerne_FWHM+10))) //7.
            {
                qDebug()<< "Snimek "<< spatne_snimky_prvotni_ohodnoceni[i]<< " nepripusten k analyze.";
                hodnoceni_vsech_snimku_videa[int(spatne_snimky_prvotni_ohodnoceni[i])] = 5;
                POC_x[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
                POC_y[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
                uhel[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
                frangi_x[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
                frangi_y[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
                frangi_euklid[int(spatne_snimky_prvotni_ohodnoceni[i])] = 999;
                continue;
            }
            else
            {
                qDebug() << "Snimek "<< spatne_snimky_prvotni_ohodnoceni[i]<< " bude proveren - nevyhovel nikde.";
                hodnoceni_vsech_snimku_videa[spatne_snimky_prvotni_ohodnoceni[i]] = 5;
                POC_x[spatne_snimky_prvotni_ohodnoceni[i]] = 999;
                POC_y[spatne_snimky_prvotni_ohodnoceni[i]] = 999;
                uhel[spatne_snimky_prvotni_ohodnoceni[i]] = 999;
                frangi_x[spatne_snimky_prvotni_ohodnoceni[i]] = 999;
                frangi_y[spatne_snimky_prvotni_ohodnoceni[i]] = 999;
                frangi_euklid[spatne_snimky_prvotni_ohodnoceni[i]] = 999;
                qDebug()<< "Snimek "<< spatne_snimky_prvotni_ohodnoceni[i]<< " bude proveren.";
                snimky_k_provereni_prvni.push_back(int(spatne_snimky_prvotni_ohodnoceni[i]));
                vypoctene_hodnoty_FWHM.push_back(FWHM);
                vypoctene_hodnoty_R.push_back(R);

            }
        }
    }
}

void VicevlaknoveZpracovani::rozhodovani_druhe(QVector<double> &snimky_k_provereni_prvni,
                       QVector<double> &hodnoceni_vsech_snimku_videa,
                       QVector<double> &vypoctene_hodnoty_R,
                       QVector<double> &vypoctene_hodnoty_FWHM,
                       QVector<double> &POC_x,
                       QVector<double> &POC_y,
                       QVector<double> &uhel,
                       QVector<double> &frangi_x,
                       QVector<double> &frangi_y,
                       QVector<double> &frangi_euklid,
                       double prumerny_korelacni_koeficient,
                       double prumerne_FWHM,
                       QVector<double> &snimky_k_provereni_druhy)
{
    for (int b = 0; b < snimky_k_provereni_prvni.length(); b++)
    {
        if ((prumerny_korelacni_koeficient - vypoctene_hodnoty_R[b]) <= 0.02)
        {
            if (vypoctene_hodnoty_FWHM[b] < (prumerne_FWHM + 30))
            {
                qDebug()<< "Snimek "<< snimky_k_provereni_prvni[b]<< " je vhodny ke slicovani.";
                hodnoceni_vsech_snimku_videa[int(snimky_k_provereni_prvni[b])] = 0;
            }
            else
            {
                qDebug()<< "Snimek "<< snimky_k_provereni_prvni[b]<< " bude proveren.";
                snimky_k_provereni_druhy.push_back(snimky_k_provereni_prvni[b]);
            }
        }
        else if ((prumerny_korelacni_koeficient - vypoctene_hodnoty_R[b]) >0.02 && (prumerny_korelacni_koeficient - vypoctene_hodnoty_R[b]) < 0.05)
        {
            if (vypoctene_hodnoty_FWHM[b] < (prumerne_FWHM + 15))
            {
                qDebug()<< "Snimek "<< snimky_k_provereni_prvni[b]<< " je vhodny ke slicovani.";
                hodnoceni_vsech_snimku_videa[int(snimky_k_provereni_prvni[b])] = 0;
            }
            else
            {
                qDebug()<< "Snimek "<< snimky_k_provereni_prvni[b]<< " bude proveren.";
                snimky_k_provereni_druhy.push_back(snimky_k_provereni_prvni[b]);
            }
        }
        else if ((prumerny_korelacni_koeficient - vypoctene_hodnoty_R[b]) >= 0.05)
        {
            if (vypoctene_hodnoty_FWHM[b] <= (prumerne_FWHM + 5))
            {
                qDebug()<< "Snimek "<< snimky_k_provereni_prvni[b]<< " je vhodny ke slicovani.";
                hodnoceni_vsech_snimku_videa[int(snimky_k_provereni_prvni[b])] = 0;
            }
            else if (vypoctene_hodnoty_FWHM[b] > (prumerne_FWHM + 10))
            {
                qDebug()<< "Snimek "<< snimky_k_provereni_prvni[b]<< " nepripusten k analyze.";
                hodnoceni_vsech_snimku_videa[int(snimky_k_provereni_prvni[b])] = 5.0;
                POC_x[int(snimky_k_provereni_prvni[b])] = 999.0;
                POC_y[int(snimky_k_provereni_prvni[b])] = 999.0;
                uhel[int(snimky_k_provereni_prvni[b])] = 999.0;
                frangi_x[int(snimky_k_provereni_prvni[b])] = 999.0;
                frangi_y[int(snimky_k_provereni_prvni[b])] = 999.0;
                frangi_euklid[int(snimky_k_provereni_prvni[b])] = 999.0;
            }
            else
            {
                qDebug()<< "Snimek "<< snimky_k_provereni_prvni[b]<< " bude proveren.";
                snimky_k_provereni_druhy.push_back(int(snimky_k_provereni_prvni[b]));
            }
        }
    }
}

void VicevlaknoveZpracovani::rozhodovani_treti(cv::Mat& obraz,
                       cv::Rect& vyrez_korelace_navic,
                       cv::Rect& vyrez_korelace_standardni,
                       QVector<double> &frangi_x,
                       QVector<double> &frangi_y,
                       QVector<double> &frangi_euklid,
                       QVector<double> &POC_x,
                       QVector<double> &POC_y,
                       QVector<double> &uhel,
                       bool zmena_velikosti_snimku,
                       bool casova_znamka,
                       cv::VideoCapture& cap,
                       QVector<double> &hodnoceni_vsech_snimku_videa,
                       QVector<double> &snimky_k_provereni_druhy,
                       QVector<double> &parametryFrangianalyzy)
{
    cv::Point3d pt_temp(0,0,0);
    Point3d obraz_frangi_reverse = frangi_analyza(obraz,2,2,0,"",1,false,pt_temp,parametryFrangianalyzy);//!
    Mat obraz_vyrez;
    obraz(vyrez_korelace_standardni).copyTo(obraz_vyrez);
    int rows = obraz.rows;
    int cols = obraz.cols;
    //for (unsigned int i = 0; i < 1; i++)
    for (int i = 0; i < snimky_k_provereni_druhy.length(); i++) //snimky_k_provereni2.size()
    {

        Mat slicovan_kompletne = cv::Mat::zeros(obraz.size(), CV_32FC3);
        Point3d mira_translace;
        double celkovy_uhel = 0;
        int iterace = -1;double oblastMaxima = 5.0;double uhelMaximalni = 0.1;
        int uspech_licovani = kompletni_slicovani(cap,obraz,
                                                  snimky_k_provereni_druhy[i],
                                                  iterace,
                                                  oblastMaxima,
                                                  uhelMaximalni,
                                                  vyrez_korelace_navic,
                                                  vyrez_korelace_standardni,
                                                  zmena_velikosti_snimku,
                                                  slicovan_kompletne,
                                                  mira_translace,celkovy_uhel);
        qDebug() << snimky_k_provereni_druhy[i] <<" -> ";
        if (uspech_licovani == 0)
        {

            qDebug()  << "nelze slicovat, ohodnocen�: 5";
            hodnoceni_vsech_snimku_videa[int(snimky_k_provereni_druhy[i])] = 5.0;
            POC_x[int(snimky_k_provereni_druhy[i])] = 999.0;
            POC_y[int(snimky_k_provereni_druhy[i])] = 999.0;
            uhel[int(snimky_k_provereni_druhy[i])] = 999.0;
            frangi_x[int(snimky_k_provereni_druhy[i])] = 999.0;
            frangi_y[int(snimky_k_provereni_druhy[i])] = 999.0;
            frangi_euklid[int(snimky_k_provereni_druhy[i])] = 999.0;
            continue;
        }
        else
        {
            Mat mezivysledek32f,mezivysledek32f_vyrez;
            slicovan_kompletne.copyTo(mezivysledek32f);
            kontrola_typu_snimku_32C1(mezivysledek32f);
            mezivysledek32f(vyrez_korelace_standardni).copyTo(mezivysledek32f_vyrez);
            double R1 = vypocet_KK(obraz,slicovan_kompletne,vyrez_korelace_standardni);
            mezivysledek32f.release();
            mezivysledek32f_vyrez.release();
            Point3d korekce_bod(0,0,0);
            if (zmena_velikosti_snimku == true)
            {
                korekce_bod = fk_translace(obraz,slicovan_kompletne);
                if (std::abs(korekce_bod.x)>=290 || std::abs(korekce_bod.y)>=290)
                {
                    korekce_bod = fk_translace_hann(obraz,slicovan_kompletne);
                }
            }
            else
            {
                korekce_bod = fk_translace_hann(obraz,slicovan_kompletne);
            }
            Mat korekce = translace_snimku(slicovan_kompletne,korekce_bod,rows,cols);
            korekce.copyTo(mezivysledek32f);
            kontrola_typu_snimku_32C1(mezivysledek32f);
            mezivysledek32f(vyrez_korelace_standardni).copyTo(mezivysledek32f_vyrez);
            double R2 = vypocet_KK(obraz,korekce,vyrez_korelace_standardni);
            Point3d slicovany_frangi_reverse(0,0,0);
            double rozdil = R2-R1;
            if (rozdil>0.015)
            {
                cv::Point3d extra_translace(0,0,0);
                extra_translace.x = mira_translace.x+korekce_bod.x;
                extra_translace.y = mira_translace.y+korekce_bod.y;
                extra_translace.z = mira_translace.z;
                qDebug()<< "Provedena korekce posunuti pro objektivnejsi analyzu skrze cevy.";
                slicovany_frangi_reverse = frangi_analyza(korekce,2,2,0,"",2,false,extra_translace,parametryFrangianalyzy);//!
            }
            else
            {
                slicovany_frangi_reverse = frangi_analyza(slicovan_kompletne,2,2,0,"",2,false,mira_translace,parametryFrangianalyzy);//!
            }
            slicovan_kompletne.release();
            if (slicovany_frangi_reverse.z == 0.0)
            {

                qDebug()<< "Nelze zjistit maximum Frangiho funkce, ohodnoceni: 5";
                hodnoceni_vsech_snimku_videa[int(snimky_k_provereni_druhy[i])] = 5.0;
                POC_x[int(snimky_k_provereni_druhy[i])] = 999.0;
                POC_y[int(snimky_k_provereni_druhy[i])] = 999.0;
                uhel[int(snimky_k_provereni_druhy[i])] = 999.0;
                frangi_x[int(snimky_k_provereni_druhy[i])] = 999.0;
                frangi_y[int(snimky_k_provereni_druhy[i])] = 999.0;
                frangi_euklid[int(snimky_k_provereni_druhy[i])] = 999.0;
                continue;
            }
            else
            {
                double rozdil_x = obraz_frangi_reverse.x - slicovany_frangi_reverse.x;
                double rozdil_y = obraz_frangi_reverse.y - slicovany_frangi_reverse.y;
                double suma_rozdilu = std::pow(rozdil_x,2.0) + std::pow(rozdil_y,2.0);
                double euklid = std::sqrt(suma_rozdilu);
                if (euklid <= 1.2)
                {
                    hodnoceni_vsech_snimku_videa[int(snimky_k_provereni_druhy[i])] = 0;
                    qDebug()<< "euklid. vzdal. je "<<euklid<<", ohodnoceni: 0";
                }
                else if (euklid > 1.2 && euklid < 10)
                {
                    hodnoceni_vsech_snimku_videa[int(snimky_k_provereni_druhy[i])] = 1;
                    qDebug()<< "euklid. vzdal. je "<<euklid<<", ohodnoceni: 1";
                    frangi_x[int(snimky_k_provereni_druhy[i])] = slicovany_frangi_reverse.x;
                    frangi_y[int(snimky_k_provereni_druhy[i])] = slicovany_frangi_reverse.y;
                    frangi_euklid[int(snimky_k_provereni_druhy[i])] = euklid;
                }
                else if (euklid >=10)
                {
                    hodnoceni_vsech_snimku_videa[int(snimky_k_provereni_druhy[i])] = 4;
                    qDebug()<< "euklid. vzdal. je "<<euklid<<", ohodnoceni: 4";
                    frangi_x[int(snimky_k_provereni_druhy[i])] = slicovany_frangi_reverse.x;
                    frangi_y[int(snimky_k_provereni_druhy[i])] = slicovany_frangi_reverse.y;
                    frangi_euklid[int(snimky_k_provereni_druhy[i])] = euklid;
                }
                rozdil_x = 0;
                rozdil_y = 0;
                suma_rozdilu = 0;
                euklid = 0;
            }

        }
    }
}*/
