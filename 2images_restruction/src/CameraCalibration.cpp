#include "CameraCalibration.h"
#include <vector>
#include <iostream>
#include <opencv2/calib3d/calib3d.hpp>
#include <Eigen/Eigen>

using namespace std;
using namespace cv;



#define DECOMPOSE_SVD

Mat GetEssentialMat(const vector<KeyPoint>& l_keypoints,
	const vector<KeyPoint>& r_keypoints,
	vector<DMatch>& matches)
{
	vector<Point2f>l_points, r_points;
	for (size_t i = 0; i < matches.size(); i++)
	{
		l_points.push_back(l_keypoints[matches[i].queryIdx].pt);
		r_points.push_back(r_keypoints[matches[i].trainIdx].pt);
	}
	Mat E = findEssentialMat(l_points, r_points, K, RANSAC, 0.999, 1.0);
	return E;
}

void TakeSVDOfE(Mat_<double>& E, Mat& svd_u, Mat& svd_vt, Mat& svd_w) {
#if 1
	//Using SVD
	SVD svd(E, SVD::MODIFY_A);
	svd_u = svd.u;
	svd_vt = svd.vt;
	svd_w = svd.w;
#else
	//Using Eigen's SVD
	cout << "Eigen3 SVD..\n";
	Eigen::Matrix3f  e = Eigen::Map<Eigen::Matrix<double, 3, 3, Eigen::RowMajor> >((double*)E.data).cast<float>();
	Eigen::JacobiSVD<Eigen::MatrixXf> svd(e, Eigen::ComputeThinU | Eigen::ComputeThinV);
	Eigen::MatrixXf Esvd_u = svd.matrixU();
	Eigen::MatrixXf Esvd_v = svd.matrixV();
	svd_u = (Mat_<double>(3, 3) << Esvd_u(0, 0), Esvd_u(0, 1), Esvd_u(0, 2),
		Esvd_u(1, 0), Esvd_u(1, 1), Esvd_u(1, 2),
		Esvd_u(2, 0), Esvd_u(2, 1), Esvd_u(2, 2));
	Mat_<double> svd_v = (Mat_<double>(3, 3) << Esvd_v(0, 0), Esvd_v(0, 1), Esvd_v(0, 2),
		Esvd_v(1, 0), Esvd_v(1, 1), Esvd_v(1, 2),
		Esvd_v(2, 0), Esvd_v(2, 1), Esvd_v(2, 2));
	svd_vt = svd_v.t();
	svd_w = (Mat_<double>(1, 3) << svd.singularValues()[0], svd.singularValues()[1], svd.singularValues()[2]);
#endif
	cout << "----------------------- SVD ------------------------\n";
	cout << "U:\n" << svd_u << "\nW:\n" << svd_w << "\nVt:\n" << svd_vt << endl;
	cout << "----------------------------------------------------\n";
}






//测试到了这里，E经过SVD分解成R，t已完成
//
//bool FindCameraMatrices(const Mat& K,
//	const Mat& Kinv,
//	const Mat& distcoeff,
//	const vector<KeyPoint>& l_keypoints,
//	const vector<KeyPoint>& r_keypoints,
//	vector<KeyPoint>& imgpts1_good,
//	vector<KeyPoint>& imgpts2_good,
//	Matx34d& P,
//	Matx34d& P1,
//	vector<DMatch>& matches,
//	vector<CloudPoint>& outCloud
//)
//{
//	//Find camera matrices
//	{
//		cout << "Find camera matrices...";
//		double t = getTickCount();
//
//		Mat_<double> E = GetEssentialMat(l_keypoints, r_keypoints, matches);
//		if (matches.size() < 100) { // || ((double)imgpts1_good.size() / (double)imgpts1.size()) < 0.25
//			cerr << "not enough inliers after E matrix" << endl;
//			return false;
//		}
//		
//		if (fabs(determinant(E)) > 1e-07) {
//			cout << "det(E) != 0 : " << determinant(E) << "\n";
//			P1 = 0;
//			return false;
//		}
//		Mat_<double> R1(3, 3);
//		Mat_<double> R2(3, 3);
//		Mat_<double> t1(1, 3);
//		Mat_<double> t2(1, 3);
//
//		//decompose E to P' , HZ (9.19)
//		{
//			if (!DecomposeEtoRandT(E, R1, R2, t1, t2)) return false;
//
//			if (determinant(R1) + 1.0 < 1e-09) {
//				//according to http://en.wikipedia.org/wiki/Essential_matrix#Showing_that_it_is_valid
//				cout << "det(R) == -1 [" << determinant(R1) << "]: flip E's sign" << endl;
//				E = -E;
//				DecomposeEtoRandT(E, R1, R2, t1, t2);
//			}
//			if (!CheckCoherentRotation(R1)) {
//				cout << "resulting rotation is not coherent\n";
//				P1 = 0;
//				return false;
//			}
//
//			P1 = Matx34d(R1(0, 0), R1(0, 1), R1(0, 2), t1(0),
//				R1(1, 0), R1(1, 1), R1(1, 2), t1(1),
//				R1(2, 0), R1(2, 1), R1(2, 2), t1(2));
//			cout << "Testing P1 " << endl << Mat(P1) << endl;
//
//			vector<CloudPoint> pcloud, pcloud1; vector<KeyPoint> corresp;
//			double reproj_error1 = TriangulatePoints(imgpts1_good, imgpts2_good, K, Kinv, distcoeff, P, P1, pcloud, corresp);
//			double reproj_error2 = TriangulatePoints(imgpts2_good, imgpts1_good, K, Kinv, distcoeff, P1, P, pcloud1, corresp);
//			vector<uchar> tmp_status;
//			//check if pointa are triangulated --in front-- of cameras for all 4 ambiguations
//			if (!TestTriangulation(pcloud, P1, tmp_status) || !TestTriangulation(pcloud1, P, tmp_status) || reproj_error1 > 100.0 || reproj_error2 > 100.0) {
//				P1 = Matx34d(R1(0, 0), R1(0, 1), R1(0, 2), t2(0),
//					R1(1, 0), R1(1, 1), R1(1, 2), t2(1),
//					R1(2, 0), R1(2, 1), R1(2, 2), t2(2));
//				cout << "Testing P1 " << endl << Mat(P1) << endl;
//
//				pcloud.clear(); pcloud1.clear(); corresp.clear();
//				reproj_error1 = TriangulatePoints(imgpts1_good, imgpts2_good, K, Kinv, distcoeff, P, P1, pcloud, corresp);
//				reproj_error2 = TriangulatePoints(imgpts2_good, imgpts1_good, K, Kinv, distcoeff, P1, P, pcloud1, corresp);
//
//				if (!TestTriangulation(pcloud, P1, tmp_status) || !TestTriangulation(pcloud1, P, tmp_status) || reproj_error1 > 100.0 || reproj_error2 > 100.0) {
//					if (!CheckCoherentRotation(R2)) {
//						cout << "resulting rotation is not coherent\n";
//						P1 = 0;
//						return false;
//					}
//
//					P1 = Matx34d(R2(0, 0), R2(0, 1), R2(0, 2), t1(0),
//						R2(1, 0), R2(1, 1), R2(1, 2), t1(1),
//						R2(2, 0), R2(2, 1), R2(2, 2), t1(2));
//					cout << "Testing P1 " << endl << Mat(P1) << endl;
//
//					pcloud.clear(); pcloud1.clear(); corresp.clear();
//					reproj_error1 = TriangulatePoints(imgpts1_good, imgpts2_good, K, Kinv, distcoeff, P, P1, pcloud, corresp);
//					reproj_error2 = TriangulatePoints(imgpts2_good, imgpts1_good, K, Kinv, distcoeff, P1, P, pcloud1, corresp);
//
//					if (!TestTriangulation(pcloud, P1, tmp_status) || !TestTriangulation(pcloud1, P, tmp_status) || reproj_error1 > 100.0 || reproj_error2 > 100.0) {
//						P1 = Matx34d(R2(0, 0), R2(0, 1), R2(0, 2), t2(0),
//							R2(1, 0), R2(1, 1), R2(1, 2), t2(1),
//							R2(2, 0), R2(2, 1), R2(2, 2), t2(2));
//						cout << "Testing P1 " << endl << Mat(P1) << endl;
//
//						pcloud.clear(); pcloud1.clear(); corresp.clear();
//						reproj_error1 = TriangulatePoints(imgpts1_good, imgpts2_good, K, Kinv, distcoeff, P, P1, pcloud, corresp);
//						reproj_error2 = TriangulatePoints(imgpts2_good, imgpts1_good, K, Kinv, distcoeff, P1, P, pcloud1, corresp);
//
//						if (!TestTriangulation(pcloud, P1, tmp_status) || !TestTriangulation(pcloud1, P, tmp_status) || reproj_error1 > 100.0 || reproj_error2 > 100.0) {
//							cout << "Shit." << endl;
//							return false;
//						}
//					}
//				}
//			}
//			for (unsigned int i = 0; i<pcloud.size(); i++) {
//				outCloud.push_back(pcloud[i]);
//			}
//		}
//
//		t = ((double)getTickCount() - t) / getTickFrequency();
//		cout << "Done. (" << t << "s)" << endl;
//	}
//	return true;
//}

bool TestTriangulation(const vector<CloudPoint>& pcloud, const Matx34d& P, vector<uchar>& status) {
	vector<Point3d> pcloud_pt3d = CloudPointsToPoints(pcloud);
	vector<Point3d> pcloud_pt3d_projected(pcloud_pt3d.size());

	Matx44d P4x4 = Matx44d::eye();
	for (int i = 0;i<12;i++) P4x4.val[i] = P.val[i];

	perspectiveTransform(pcloud_pt3d, pcloud_pt3d_projected, P4x4);

	status.resize(pcloud.size(), 0);
	for (int i = 0; i<pcloud.size(); i++) {
		status[i] = (pcloud_pt3d_projected[i].z > 0) ? 1 : 0;
	}
	int count = countNonZero(status);

	double percentage = ((double)count / (double)pcloud.size());
	cout << count << "/" << pcloud.size() << " = " << percentage*100.0 << "% are in front of camera" << endl;
	if (percentage < 0.75)
		return false; //less than 75% of the points are in front of the camera

					  //check for coplanarity of points
	if (false) //not
	{
		cv::Mat_<double> cldm(pcloud.size(), 3);
		for (unsigned int i = 0;i<pcloud.size();i++) {
			cldm.row(i)(0) = pcloud[i].pt.x;
			cldm.row(i)(1) = pcloud[i].pt.y;
			cldm.row(i)(2) = pcloud[i].pt.z;
		}
		cv::Mat_<double> mean;
		cv::PCA pca(cldm, mean, CV_PCA_DATA_AS_ROW);

		int num_inliers = 0;
		cv::Vec3d nrm = pca.eigenvectors.row(2); nrm = nrm / norm(nrm);
		cv::Vec3d x0 = pca.mean;
		double p_to_plane_thresh = sqrt(pca.eigenvalues.at<double>(2));

		for (int i = 0; i<pcloud.size(); i++) {
			Vec3d w = Vec3d(pcloud[i].pt) - x0;
			double D = fabs(nrm.dot(w));
			if (D < p_to_plane_thresh) num_inliers++;
		}

		cout << num_inliers << "/" << pcloud.size() << " are coplanar" << endl;
		if ((double)num_inliers / (double)(pcloud.size()) > 0.85)
			return false;
	}

	return true;
}


bool DecomposeEtoRandT(
	Mat_<double>& E,
	Mat_<double>& R1,
	Mat_<double>& R2,
	Mat_<double>& t1,
	Mat_<double>& t2)
{
#ifdef DECOMPOSE_SVD
	//Using HZ E decomposition
	Mat svd_u, svd_vt, svd_w;
	TakeSVDOfE(E, svd_u, svd_vt, svd_w);

	//check if first and second singular values are the same (as they should be)
	double singular_values_ratio = fabsf(svd_w.at<double>(0) / svd_w.at<double>(1));
	if (singular_values_ratio>1.0) singular_values_ratio = 1.0 / singular_values_ratio; // flip ratio to keep it [0,1]
	if (singular_values_ratio < 0.7) {
		cout << "singular values are too far apart\n";
		return false;
	}

	Matx33d W(0, -1, 0,	//HZ 9.13
		1, 0, 0,
		0, 0, 1);
	Matx33d Wt(0, 1, 0,
		-1, 0, 0,
		0, 0, 1);
	R1 = svd_u * Mat(W) * svd_vt; //HZ 9.19
	R2 = svd_u * Mat(Wt) * svd_vt; //HZ 9.19
	t1 = svd_u.col(2); //u3
	t2 = -svd_u.col(2); //u3
#else
	//Using Horn E decomposition
	DecomposeEssentialUsingHorn90(E[0], R1[0], R2[0], t1[0], t2[0]);
#endif
	return true;
}

