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





void DecomposeEssentialUsingHorn90(double _E[9], double _R1[9], double _R2[9], double _t1[3], double _t2[3]) {
	//from : http://people.csail.mit.edu/bkph/articles/Essential.pdf
	using namespace Eigen;
	Matrix3d E = Map<Matrix<double, 3, 3, RowMajor> >(_E);
	Matrix3d EEt = E * E.transpose();
	Vector3d e0e1 = E.col(0).cross(E.col(1)), e1e2 = E.col(1).cross(E.col(2)), e2e0 = E.col(2).cross(E.col(2));
	Vector3d b1, b2;
#if 1
	//Method 1
	Matrix3d bbt = 0.5 * EEt.trace() * Matrix3d::Identity() - EEt; //Horn90 (12)
	Vector3d bbt_diag = bbt.diagonal();
	if (bbt_diag(0) > bbt_diag(1) && bbt_diag(0) > bbt_diag(2)) {
		b1 = bbt.row(0) / sqrt(bbt_diag(0));
		b2 = -b1;
	}
	else if (bbt_diag(1) > bbt_diag(0) && bbt_diag(1) > bbt_diag(2)) {
		b1 = bbt.row(1) / sqrt(bbt_diag(1));
		b2 = -b1;
	}
	else {
		b1 = bbt.row(2) / sqrt(bbt_diag(2));
		b2 = -b1;
	}
#else
	//Method 2
	if (e0e1.norm() > e1e2.norm() && e0e1.norm() > e2e0.norm()) {
		b1 = e0e1.normalized() * sqrt(0.5 * EEt.trace()); //Horn90 (18)
		b2 = -b1;
	}
	else if (e1e2.norm() > e0e1.norm() && e1e2.norm() > e2e0.norm()) {
		b1 = e1e2.normalized() * sqrt(0.5 * EEt.trace()); //Horn90 (18)
		b2 = -b1;
	}
	else {
		b1 = e2e0.normalized() * sqrt(0.5 * EEt.trace()); //Horn90 (18)
		b2 = -b1;
	}
#endif

	//Horn90 (19)
	Matrix3d cofactors; cofactors.col(0) = e1e2; cofactors.col(1) = e2e0; cofactors.col(2) = e0e1;
	cofactors.transposeInPlace();

	//B = [b]_x , see Horn90 (6) and http://en.wikipedia.org/wiki/Cross_product#Conversion_to_matrix_multiplication
	Matrix3d B1; B1 << 0, -b1(2), b1(1),
		b1(2), 0, -b1(0),
		-b1(1), b1(0), 0;
	Matrix3d B2; B2 << 0, -b2(2), b2(1),
		b2(2), 0, -b2(0),
		-b2(1), b2(0), 0;

	Map<Matrix<double, 3, 3, RowMajor> > R1(_R1), R2(_R2);

	//Horn90 (24)
	R1 = (cofactors.transpose() - B1*E) / b1.dot(b1);
	R2 = (cofactors.transpose() - B2*E) / b2.dot(b2);
	Map<Vector3d> t1(_t1), t2(_t2);
	t1 = b1; t2 = b2;
	cout << "Horn90 provided " << endl << R1 << endl << "and" << endl << R2 << endl;
}
bool CheckCoherentRotation(cv::Mat_<double>& R) {
	if (fabs(determinant(R)) - 1.0 > 1e-07) {
		cerr << "det(R) != +-1.0, this is not a rotation matrix" << endl;
		return false;
	}
	else
		return true;
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

