#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d/calib3d.hpp>
using namespace std;
using namespace cv;

Point2d pixel2cam(const Point2d& p, const Mat& K);
Mat R;
vector<Point2f> points1;
vector<Point2f> points2;
double min_dist = 10000, max_dist = 0;
Mat descriptors_1, descriptors_2;
Point2d principal_point(325.1, 249.7);	//相机光心, TUM dataset标定值
double focal_length = 521;			//相机焦距, TUM dataset标定值

/****************************************************
 * 本程序演示了如何使用2D-2D的特征匹配估计相机运动
 * **************************************************/

void find_feature_matches (
    const Mat& img_1, const Mat& img_2,
    std::vector<KeyPoint>& keypoints_1,
    std::vector<KeyPoint>& keypoints_2,
    std::vector< DMatch >& matches );

void pose_estimation_2d2d (
    std::vector<KeyPoint> keypoints_1,
    std::vector<KeyPoint> keypoints_2,
    std::vector< DMatch > matches,
    Mat& R, Mat& t );

// 像素坐标转相机归一化坐标
Point2d pixel2cam ( const Point2d& p, const Mat& K );


void decomposeEssentialMat_Custom(InputArray _E, OutputArray _R1, OutputArray _R2, OutputArray _t)
{
	Mat E = _E.getMat().reshape(1, 3);
	CV_Assert(E.cols == 3 && E.rows == 3);

	Mat D, U, Vt;
	SVD::compute(E, D, U, Vt);

	if (determinant(U) < 0) U *= -1.;
	if (determinant(Vt) < 0) Vt *= -1.;

	Mat W = (Mat_<double>(3, 3) << 0, 1, 0, -1, 0, 0, 0, 0, 1);
	W.convertTo(W, E.type());

	Mat R1, R2, t;
	R1 = U * W * Vt;
	R2 = U * W.t() * Vt;
	t = U.col(2) * 1.0;

	R1.copyTo(_R1);
	R2.copyTo(_R2);
	t.copyTo(_t);
}

int recoverPose_Custom(InputArray E, InputArray _points1, InputArray _points2, OutputArray _R,
	OutputArray _t, double focal, Point2d pp, InputOutputArray _mask)
{
	Mat points1, points2, cameraMatrix;
	cameraMatrix = (Mat_<double>(3, 3) << focal, 0, pp.x, 0, focal, pp.y, 0, 0, 1);
	_points1.getMat().convertTo(points1, CV_64F);
	_points2.getMat().convertTo(points2, CV_64F);

	int npoints = points1.checkVector(2);
	CV_Assert(npoints >= 0 && points2.checkVector(2) == npoints &&
		points1.type() == points2.type());

	CV_Assert(cameraMatrix.rows == 3 && cameraMatrix.cols == 3 && cameraMatrix.channels() == 1);

	if (points1.channels() > 1)
	{
		points1 = points1.reshape(1, npoints);
		points2 = points2.reshape(1, npoints);
	}

	double fx = cameraMatrix.at<double>(0, 0);
	double fy = cameraMatrix.at<double>(1, 1);
	double cx = cameraMatrix.at<double>(0, 2);
	double cy = cameraMatrix.at<double>(1, 2);

	points1.col(0) = (points1.col(0) - cx) / fx;
	points2.col(0) = (points2.col(0) - cx) / fx;
	points1.col(1) = (points1.col(1) - cy) / fy;
	points2.col(1) = (points2.col(1) - cy) / fy;

	points1 = points1.t();
	points2 = points2.t();

	Mat R1, R2, t;
	decomposeEssentialMat_Custom(E, R1, R2, t);
	Mat P0 = Mat::eye(3, 4, R1.type());
	Mat P1(3, 4, R1.type()), P2(3, 4, R1.type()), P3(3, 4, R1.type()), P4(3, 4, R1.type());
	P1(Range::all(), Range(0, 3)) = R1 * 1.0; P1.col(3) = t * 1.0;
	P2(Range::all(), Range(0, 3)) = R2 * 1.0; P2.col(3) = t * 1.0;
	P3(Range::all(), Range(0, 3)) = R1 * 1.0; P3.col(3) = -t * 1.0;
	P4(Range::all(), Range(0, 3)) = R2 * 1.0; P4.col(3) = -t * 1.0;

	// Do the cheirality check.
	// Notice here a threshold dist is used to filter
	// out far away points (i.e. infinite points) since
	// there depth may vary between postive and negtive.
	double dist = 50.0;
	Mat Q;
	triangulatePoints(P0, P1, points1, points2, Q);
	Mat mask1 = Q.row(2).mul(Q.row(3)) > 0;
	Q.row(0) /= Q.row(3);
	Q.row(1) /= Q.row(3);
	Q.row(2) /= Q.row(3);
	Q.row(3) /= Q.row(3);
	mask1 = (Q.row(2) < dist) & mask1;
	Q = P1 * Q;
	mask1 = (Q.row(2) > 0) & mask1;
	mask1 = (Q.row(2) < dist) & mask1;

	triangulatePoints(P0, P2, points1, points2, Q);
	Mat mask2 = Q.row(2).mul(Q.row(3)) > 0;
	Q.row(0) /= Q.row(3);
	Q.row(1) /= Q.row(3);
	Q.row(2) /= Q.row(3);
	Q.row(3) /= Q.row(3);
	mask2 = (Q.row(2) < dist) & mask2;
	Q = P2 * Q;
	mask2 = (Q.row(2) > 0) & mask2;
	mask2 = (Q.row(2) < dist) & mask2;

	triangulatePoints(P0, P3, points1, points2, Q);
	Mat mask3 = Q.row(2).mul(Q.row(3)) > 0;
	Q.row(0) /= Q.row(3);
	Q.row(1) /= Q.row(3);
	Q.row(2) /= Q.row(3);
	Q.row(3) /= Q.row(3);
	mask3 = (Q.row(2) < dist) & mask3;
	Q = P3 * Q;
	mask3 = (Q.row(2) > 0) & mask3;
	mask3 = (Q.row(2) < dist) & mask3;

	triangulatePoints(P0, P4, points1, points2, Q);
	Mat mask4 = Q.row(2).mul(Q.row(3)) > 0;
	Q.row(0) /= Q.row(3);
	Q.row(1) /= Q.row(3);
	Q.row(2) /= Q.row(3);
	Q.row(3) /= Q.row(3);
	mask4 = (Q.row(2) < dist) & mask4;
	Q = P4 * Q;
	mask4 = (Q.row(2) > 0) & mask4;
	mask4 = (Q.row(2) < dist) & mask4;

	mask1 = mask1.t();
	mask2 = mask2.t();
	mask3 = mask3.t();
	mask4 = mask4.t();

	// If _mask is given, then use it to filter outliers.
	if (!_mask.empty())
	{
		Mat mask = _mask.getMat();
		CV_Assert(mask.size() == mask1.size());
		bitwise_and(mask, mask1, mask1);
		bitwise_and(mask, mask2, mask2);
		bitwise_and(mask, mask3, mask3);
		bitwise_and(mask, mask4, mask4);
	}
	if (_mask.empty() && _mask.needed())
	{
		_mask.create(mask1.size(), CV_8U);
	}

	CV_Assert(_R.needed() && _t.needed());
	_R.create(3, 3, R1.type());
	_t.create(3, 1, t.type());

	int good1 = countNonZero(mask1);
	int good2 = countNonZero(mask2);
	int good3 = countNonZero(mask3);
	int good4 = countNonZero(mask4);

	if (good1 >= good2 && good1 >= good3 && good1 >= good4)
	{
		R1.copyTo(_R);
		t.copyTo(_t);
		if (_mask.needed()) mask1.copyTo(_mask);
		return good1;
	}
	else if (good2 >= good1 && good2 >= good3 && good2 >= good4)
	{
		R2.copyTo(_R);
		t.copyTo(_t);
		if (_mask.needed()) mask2.copyTo(_mask);
		return good2;
	}
	else if (good3 >= good1 && good3 >= good2 && good3 >= good4)
	{
		t = -t;
		R1.copyTo(_R);
		t.copyTo(_t);
		if (_mask.needed()) mask3.copyTo(_mask);
		return good3;
	}
	else
	{
		t = -t;
		R2.copyTo(_R);
		t.copyTo(_t);
		if (_mask.needed()) mask4.copyTo(_mask);
		return good4;
	}
}

cv::Mat findEssentialMat_Custom(InputArray _points1, InputArray _points2, double focal, Point2d pp)
{
	Mat cameraMatrix = (Mat_<double>(3, 3) << focal, 0, pp.x, 0, focal, pp.y, 0, 0, 1);

	Mat points1, points2;
	_points1.getMat().convertTo(points1, CV_64F);
	_points2.getMat().convertTo(points2, CV_64F);

	Mat fundamental_matrix;
	fundamental_matrix = findFundamentalMat(points1, points2, CV_FM_8POINT);
	Mat E;
	E = cameraMatrix.t()* fundamental_matrix * cameraMatrix;

	return E;
}




int main ( int argc, char** argv )
{
    if ( argc != 3 )
    {
        cout<<"usage: pose_estimation_2d2d img1 img2"<<endl;
        return 1;
    }
    //-- 读取图像
    Mat img_1 = imread ( argv[1], CV_LOAD_IMAGE_COLOR );
    Mat img_2 = imread ( argv[2], CV_LOAD_IMAGE_COLOR );

    vector<KeyPoint> keypoints_1, keypoints_2;
    vector<DMatch> matches;
    find_feature_matches ( img_1, img_2, keypoints_1, keypoints_2, matches );
    cout<<"一共找到了"<<matches.size() <<"组匹配点"<<endl;

    //-- 估计两张图像间运动
	Mat R;
	Mat t;
    pose_estimation_2d2d ( keypoints_1, keypoints_2, matches, R, t );

    //-- 验证E=t^R*scale
    Mat t_x = ( Mat_<double> ( 3,3 ) <<
                0,                      -t.at<double> ( 2,0 ),     t.at<double> ( 1,0 ),
                t.at<double> ( 2,0 ),      0,                      -t.at<double> ( 0,0 ),
                -t.at<double> ( 1,0 ),     t.at<double> ( 0,0 ),      0 );

    cout<<"t^R="<<endl<<t_x*R<<endl;

    //-- 验证对极约束
    Mat K = ( Mat_<double> ( 3,3 ) << 520.9, 0, 325.1, 0, 521.0, 249.7, 0, 0, 1 );
    for ( DMatch m: matches )
    {
        Point2d pt1 = pixel2cam ( keypoints_1[ m.queryIdx ].pt, K );
        Mat y1 = ( Mat_<double> ( 3,1 ) << pt1.x, pt1.y, 1 );
        Point2d pt2 = pixel2cam ( keypoints_2[ m.trainIdx ].pt, K );
        Mat y2 = ( Mat_<double> ( 3,1 ) << pt2.x, pt2.y, 1 );
        Mat d = y2.t() * t_x * R * y1;
        cout << "epipolar constraint = " << d << endl;
    }
    return 0;
}

void find_feature_matches ( const Mat& img_1, const Mat& img_2,
                            std::vector<KeyPoint>& keypoints_1,
                            std::vector<KeyPoint>& keypoints_2,
                            std::vector< DMatch >& matches )
{
    //-- 初始化
    //Mat descriptors_1, descriptors_2;
    // used in OpenCV3 
    Ptr<FeatureDetector> detector = ORB::create();
    Ptr<DescriptorExtractor> descriptor = ORB::create();
    // use this if you are in OpenCV2 
    // Ptr<FeatureDetector> detector = FeatureDetector::create ( "ORB" );
    // Ptr<DescriptorExtractor> descriptor = DescriptorExtractor::create ( "ORB" );
    Ptr<DescriptorMatcher> matcher  = DescriptorMatcher::create ( "BruteForce-Hamming" );
    //-- 第一步:检测 Oriented FAST 角点位置
    detector->detect ( img_1,keypoints_1 );
    detector->detect ( img_2,keypoints_2 );

    //-- 第二步:根据角点位置计算 BRIEF 描述子
    descriptor->compute ( img_1, keypoints_1, descriptors_1 );
    descriptor->compute ( img_2, keypoints_2, descriptors_2 );

    //-- 第三步:对两幅图像中的BRIEF描述子进行匹配，使用 Hamming 距离
    vector<DMatch> match;
    //BFMatcher matcher ( NORM_HAMMING );
    matcher->match ( descriptors_1, descriptors_2, match );

    //-- 第四步:匹配点对筛选
    //double min_dist=10000, max_dist=0;

    //找出所有匹配之间的最小距离和最大距离, 即是最相似的和最不相似的两组点之间的距离
    for ( int i = 0; i < descriptors_1.rows; i++ )
    {
        double dist = match[i].distance;
        if ( dist < min_dist ) min_dist = dist;
        if ( dist > max_dist ) max_dist = dist;
    }

    printf ( "-- Max dist : %f \n", max_dist );
    printf ( "-- Min dist : %f \n", min_dist );

    //当描述子之间的距离大于两倍的最小距离时,即认为匹配有误.但有时候最小距离会非常小,设置一个经验值30作为下限.
    for ( int i = 0; i < descriptors_1.rows; i++ )
    {
        if ( match[i].distance <= max ( 2*min_dist, 30.0 ) )
        {
            matches.push_back ( match[i] );
        }
    }
}


Point2d pixel2cam ( const Point2d& p, const Mat& K )
{
    return Point2d
           (
               ( p.x - K.at<double> ( 0,2 ) ) / K.at<double> ( 0,0 ),
               ( p.y - K.at<double> ( 1,2 ) ) / K.at<double> ( 1,1 )
           );
}


void pose_estimation_2d2d ( std::vector<KeyPoint> keypoints_1,
                            std::vector<KeyPoint> keypoints_2,
                            std::vector< DMatch > matches,
                            Mat& R, Mat& t )
{
    //相机内参,TUM Freiburg2
    Mat K = ( Mat_<double>(3,3) << 520.9, 0, 325.1, 0, 521.0 , 249.7, 0, 0, 1); 

    //-- 把匹配点转换为vector<Point2f>的形式
    vector<Point2f> points1;
    vector<Point2f> points2;
	double focal_length = 521;//相机焦距, TUM dataset标定值
    for ( int i = 0; i < ( int ) matches.size(); i++ )
    {
        points1.push_back ( keypoints_1[matches[i].queryIdx].pt );
        points2.push_back ( keypoints_2[matches[i].trainIdx].pt );
    }

    //-- 计算基础矩阵
    Mat fundamental_matrix;
    fundamental_matrix = findFundamentalMat ( points1, points2, CV_FM_8POINT );
    cout<<"fundamental_matrix is "<<endl<< fundamental_matrix<<endl;

    //-- 计算本质矩阵
    Mat essential_matrix;
    essential_matrix = findEssentialMat_Custom(points1,points2,focal_length,principal_point);
    cout<<"essential_matrix is "<<endl<< essential_matrix<<endl;

    //-- 计算单应矩阵
    Mat homography_matrix;
    homography_matrix = findHomography ( points1, points2, RANSAC, 3 );
    cout<<"homography_matrix is "<<endl<<homography_matrix<<endl;

    //-- 从本质矩阵中恢复旋转和平移信息.
    cv::recoverPose( essential_matrix, points1, points2, R, t, focal_length, principal_point);
    cout<<"R is "<<endl<<R<<endl;
    cout<<"t is "<<endl<<t<<endl;
    
}
