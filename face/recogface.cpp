
#include "stdafx.h"

//этот блок включений из dlib должен быть здесь, иначе много warning-ов
#include <dlib/dnn.h>
#include <dlib/gui_widgets.h>
#include <dlib/clustering.h>
#include <dlib/string.h>
#include <dlib/image_io.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/opencv.h>

#include "recogface.h"

using namespace dlib;

// ----------------------------------------------------------------------------------------

// The next bit of code defines a ResNet network.  It's basically copied
// and pasted from the dnn_imagenet_ex.cpp example, except we replaced the loss
// layer with loss_metric and made the network somewhat smaller.  Go read the introductory
// dlib DNN examples to learn what all this stuff means.
//
// Also, the dnn_metric_learning_on_images_ex.cpp example shows how to train this network.
// The dlib_face_recognition_resnet_model_v1 model used by this example was trained using
// essentially the code shown in dnn_metric_learning_on_images_ex.cpp except the
// mini-batches were made larger (35x15 instead of 5x5), the iterations without progress
// was set to 10000, the jittering you can see below in jitter_image() was used during
// training, and the training dataset consisted of about 3 million images instead of 55.
// Also, the input layer was locked to images of size 150.
template <template <int, template<typename>class, int, typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual = add_prev1<block<N, BN, 1, tag1<SUBNET>>>;

template <template <int, template<typename>class, int, typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual_down = add_prev2<avg_pool<2, 2, 2, 2, skip1<tag2<block<N, BN, 2, tag1<SUBNET>>>>>>;

template <int N, template <typename> class BN, int stride, typename SUBNET>
using block = BN<con<N, 3, 3, 1, 1, relu<BN<con<N, 3, 3, stride, stride, SUBNET>>>>>;

template <int N, typename SUBNET> using ares = relu<residual<block, N, affine, SUBNET>>;
template <int N, typename SUBNET> using ares_down = relu<residual_down<block, N, affine, SUBNET>>;

template <typename SUBNET> using alevel0 = ares_down<256, SUBNET>;
template <typename SUBNET> using alevel1 = ares<256, ares<256, ares_down<256, SUBNET>>>;
template <typename SUBNET> using alevel2 = ares<128, ares<128, ares_down<128, SUBNET>>>;
template <typename SUBNET> using alevel3 = ares<64, ares<64, ares<64, ares_down<64, SUBNET>>>>;
template <typename SUBNET> using alevel4 = ares<32, ares<32, ares<32, SUBNET>>>;

using anet_type = loss_metric<fc_no_bias<128, avg_pool_everything<
	alevel0<
	alevel1<
	alevel2<
	alevel3<
	alevel4<
	max_pool<3, 3, 2, 2, relu<affine<con<32, 7, 7, 2, 2,
	input_rgb_image_sized<150>
	>>>>>>>>>>>>;

// ----------------------------------------------------------------------------------------

//объекты детектирования  лиц
frontal_face_detector detector;
shape_predictor sp;
//объект распознавания лиц
anet_type net;

#define DETECTOR detector
#define SP sp
#define NET net

void RecogFace::copy(const dlib::matrix<float, 0, 1>& facedescriptorsrc, std::vector<float>& facedescriptordst)
{
	facedescriptordst.clear();

	for (int i = 0; i < 128; i++)
	{
		facedescriptordst.push_back(facedescriptorsrc(i, 0));
	}
}

bool RecogFace::init(const string &path)
{
	string shapefile = path + "shape_predictor_68_face_landmarks.dat";
	string resnetfile = path + "dlib_face_recognition_resnet_model_v1.dat";

	ifstream ifile0(shapefile.c_str());
	ifstream ifile1(resnetfile.c_str());

	if (!(bool)ifile0)
	{
		printf("not found shape_predictor_68_face_landmarks.dat\n");
		return false;
	}

	if (!(bool)ifile1)
	{
		printf("not found dlib_face_recognition_resnet_model_v1.dat\n");
		return false;
	}

	DETECTOR = get_frontal_face_detector();
	deserialize(shapefile) >> SP;
	deserialize(resnetfile) >> NET;

	return true;
}

void RecogFace::detect(const Mat& frame, std::vector<FrameFace>& framefaces)
{
	cv_image<bgr_pixel> img_(frame);
	matrix<rgb_pixel> img;
	assign_image(img, img_);// memory copy happens here and only here

	int nr = img.nr();

	pyramid_up(img);

        float k = (float)nr / img.nr();

	for (auto face : DETECTOR(img))
	{
		FrameFace frameface;
		frameface.rect = Rect(k * face.left(), k * face.top(), k * (face.right() - face.left()), k * (face.bottom() - face.top()));
		auto shape = SP(img, face);
		extract_image_chip(img, get_face_chip_details(shape, 150, 0.25), frameface.face);
		framefaces.push_back(frameface);
	}
}

void RecogFace::recog(std::vector<std::vector<FrameFace> > &vectframefaces)
{
	// распознаём - заполняем дескрипторы

	bool isfaces = false;

	std::vector<matrix<rgb_pixel> > faces;

	for (int i = 0; i < vectframefaces.size(); i++)
	{
		for (int j = 0; j < vectframefaces[i].size(); j++)
		{
			isfaces = true;
			faces.push_back(vectframefaces[i][j].face);
		}
	}

	if (isfaces)
	{
		std::vector<matrix<float, 0, 1> > facedescriptors = NET(faces);

		int k = 0;

		for (int i = 0; i < vectframefaces.size(); i++)
		{
			for (int j = 0; j < vectframefaces[i].size(); j++)
			{
				copy(facedescriptors[k++], vectframefaces[i][j].facedescriptor);
			}
		}
	}
}

RecogFace::RecogFace()
{
}

RecogFace::RecogFace(const RecogFace& recogface)
{
}

RecogFace& RecogFace::operator=(const RecogFace& recogface)
{
	return *this;
}

RecogFace::~RecogFace()
{
}
