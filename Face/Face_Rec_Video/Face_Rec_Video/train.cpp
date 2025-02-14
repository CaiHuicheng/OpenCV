#include<opencv2\face\facerec.hpp> //opencv3需要
#include<opencv2\core.hpp>
#include<opencv2\face.hpp>
#include<opencv2\highgui.hpp>
#include<opencv2\imgproc.hpp>

#include <math.h>  
//使用void read_csv()这个函数必须的三个头文件
#include <iostream>  
#include <fstream>  
#include <sstream>  


using namespace cv;
using namespace cv::face;
using namespace std;

static Mat norm_0_255(InputArray _src) {
	Mat src = _src.getMat();
	// 创建和返回一个归一化后的图像矩阵:  
	Mat dst;
	switch (src.channels()) {
	case 1:
		cv::normalize(_src, dst, 0, 255, NORM_MINMAX, CV_8UC1);
		break;
	case 3:
		cv::normalize(_src, dst, 0, 255, NORM_MINMAX, CV_8UC3);
		break;
	default:
		src.copyTo(dst);
		break;
	}
	return dst;
}

//使用CSV文件去读图像和标签，主要使用stringstream和getline方法  
static void read_csv(const string& filename, vector<Mat>& images, vector<int>& labels, char separator = ';') {
	std::ifstream file(filename.c_str(), ifstream::in);//c_str()函数可用可不用，无需返回一个标准C类型的字符串
	if (!file)
	{
		string error_message = "No valid input file was given, please check the given filename.";
		CV_Error(CV_StsBadArg, error_message);
	}
	string line, path, classlabel;
	while (getline(file, line)) //从文本文件中读取一行字符，未指定限定符默认限定符为“/n”
	{
		stringstream liness(line);//这里采用stringstream主要作用是做字符串的分割
		getline(liness, path, separator);//读入图片文件路径以分好作为限定符
		getline(liness, classlabel);//读入图片标签，默认限定符
		if (!path.empty() && !classlabel.empty()) //如果读取成功，则将图片和对应标签压入对应容器中
		{
			images.push_back(imread(path, 0));
			labels.push_back(atoi(classlabel.c_str()));
		}
	}
}

int main()
{
	//读取你的CSV文件路径.  
	//string fn_csv = string(argv[1]);  
	string fn_csv = "face3.txt";

	// 2个容器来存放图像数据和对应的标签  
	vector<Mat> images;
	vector<int> labels;
	// 读取数据. 如果文件不合法就会出错  
	// 输入的文件名已经有了.  
	try
	{
		read_csv(fn_csv, images, labels); //从csv文件中批量读取训练数据
	}
	catch (cv::Exception& e)
	{
		cerr << "Error opening file \"" << fn_csv << "\". Reason: " << e.msg << endl;
		// 文件有问题，我们啥也做不了了，退出了  
		exit(1);
	}
	// 如果没有读取到足够图片，也退出.  
	if (images.size() <= 1) {
		string error_message = "This demo needs at least 2 images to work. Please add more images to your data set!";
		CV_Error(CV_StsError, error_message);
	}

	for (int i = 0; i < images.size(); i++)
	{
		if (images[i].size() != Size(92, 112))
		{
			cout << i << endl;
			cout << images[i].size() << endl;
		}
	}


	// 下面的几行代码仅仅是从你的数据集中移除最后一张图片，作为测试图片  
	//[gm:自然这里需要根据自己的需要修改，他这里简化了很多问题]  
	Mat testSample = images[images.size() - 1];
	int testLabel = labels[labels.size() - 1];
	images.pop_back();//删除最后一张照片，此照片作为测试图片
	labels.pop_back();//删除最有一张照片的labels
					  // 下面几行创建了一个特征脸模型用于人脸识别，  
					  // 通过CSV文件读取的图像和标签训练它。  
					  // T这里是一个完整的PCA变换  
					  //如果你只想保留10个主成分，使用如下代码  
					  //      cv::createEigenFaceRecognizer(10);  
					  //  
					  // 如果你还希望使用置信度阈值来初始化，使用以下语句：  
					  //      cv::createEigenFaceRecognizer(10, 123.0);  
					  //  
					  // 如果你使用所有特征并且使用一个阈值，使用以下语句：  
					  //      cv::createEigenFaceRecognizer(0, 123.0);  

					  //创建一个PCA人脸分类器，暂时命名为model吧，创建完成后
					  //调用其中的成员函数train()来完成分类器的训练
	Ptr<BasicFaceRecognizer> model = EigenFaceRecognizer::create();
	model->train(images, labels);
	model->save("MyFacePCAModel.xml");//保存路径可自己设置，但注意用“\\”

	Ptr<BasicFaceRecognizer> model1 = FisherFaceRecognizer::create();
	model1->train(images, labels);
	model1->save("MyFaceFisherModel.xml");

	Ptr<LBPHFaceRecognizer> model2 = LBPHFaceRecognizer::create();
	model2->train(images, labels);
	model2->save("MyFaceLBPHModel.xml");

	// 下面对测试图像进行预测，predictedLabel是预测标签结果  
	//注意predict()入口参数必须为单通道灰度图像，如果图像类型不符，需要先进行转换
	//predict()函数返回一个整形变量作为识别标签
	int predictedLabel = model->predict(testSample);//加载分类器
	int predictedLabel1 = model1->predict(testSample);
	int predictedLabel2 = model2->predict(testSample);

	// 还有一种调用方式，可以获取结果同时得到阈值:  
	// int predictedLabel = -1;  
	// double confidence = 0.0;  
	//  model->predict(testSample, predictedLabel, confidence);  

	string result_message = format("Predicted class = %d / Actual class = %d.", predictedLabel, testLabel);
	string result_message1 = format("Predicted class = %d / Actual class = %d.", predictedLabel1, testLabel);
	string result_message2 = format("Predicted class = %d / Actual class = %d.", predictedLabel2, testLabel);
	cout << result_message << endl;
	cout << result_message1 << endl;
	cout << result_message2 << endl;

	getchar();
	//waitKey(0);
	return 0;
}