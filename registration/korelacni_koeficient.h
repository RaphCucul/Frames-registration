#ifndef KORELACNI_KOEFICIENT_H_INCLUDED
#define KORELACNI_KOEFICIENT_H_INCLUDED
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include "opencv2/imgproc/imgproc_c.h"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/highgui/highgui.hpp>
using std::vector;
float sum(vector<double> a);
float mean(vector<double> a);
float sqsum(vector<double> a);
float stdev(vector<double> nums);
vector<double> operator-(vector<double> a, double b);
vector<double> operator*(vector<double> a, vector<double> b);
float pearsoncoeff(vector<double> X, vector<double> Y);
float vypocet_korel_koef_puvodni(cv::Mat &obraz,cv::Mat &zfazovany,double cislo,double sirka_vyrezu);
/// vsechny vyse uvedene funkce nejsou pouzivany, byly vyuzivany pri hledani nejlepsiho zpusobu vypoctu
/// korelacniho koeficientu

double vypocet_KK(cv::Mat& referencni,cv::Mat& slicovany,cv::Rect vyrez_korelace);
/// aktualni funkce pocitajici korelacni koeficient
#endif // KORELACNI_KOEFICIENT_H_INCLUDED
