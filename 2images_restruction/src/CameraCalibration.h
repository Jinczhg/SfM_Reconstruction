#pragma once
#include "opencv2/core.hpp"
#include "opencv2/calib3d.hpp"
#include <iostream>
#include "dataStruct.h"
using namespace std;
using namespace cv;


const double focal = 5.0, u0 = 320, v0 = 240, dx = 5.312 / 640, dy = 3.984 / 480, fu = focal / dx, fv = focal / dy;
const double fx = focal * 180 / 25.4, fy = focal * 180 / 25.4;

const Matx33d K = Matx33d(fx, 0, u0,
						  0, fy, v0,
                       	  0, 0, 1);
const Point2d pp(u0, v0);

Mat GetEssentialMat(const vector<KeyPoint>& l_keypoints,
	const vector<KeyPoint>& r_keypoints,
	vector<DMatch>& matches);
bool CheckCoherentRotation(Mat_<double>& R);
bool DecomposeEtoRandT(
	Mat_<double>& E,
	Mat_<double>& R1,
	Mat_<double>& R2,
	Mat_<double>& t1,
	Mat_<double>& t2);
bool FindCameraMatrices(const Mat& K,
	const Mat& Kinv,
	const Mat& distcoeff,
	const vector<KeyPoint>& l_keypoints,
	const vector<KeyPoint>& r_keypoints,
	vector<KeyPoint>& imgpts1_good,
	vector<KeyPoint>& imgpts2_good,
	Matx34d& P,
	Matx34d& P1,
	vector<DMatch>& matches,
	vector<CloudPoint>& outCloud
);







bool TestTriangulation(const vector<CloudPoint>& pcloud, const Matx34d& P, vector<uchar>& status);

