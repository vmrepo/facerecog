
#include "stdafx.h"

//этот блок включений из dlib должен быть здесь, иначе много warning-ов
#include <dlib/dnn.h>
#include <dlib/gui_widgets.h>
#include <dlib/clustering.h>
#include <dlib/string.h>
#include <dlib/image_io.h>
#include <dlib/image_processing/frontal_face_detector.h>

#include <fstream>

#include "imageface.h"

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
frontal_face_detector ImageFace_detector;
shape_predictor ImageFace_sp;
//объект распознавания лиц
anet_type ImageFace_net;

#define DETECTOR ImageFace_detector
#define SP ImageFace_sp
#define NET ImageFace_net

string ImageFace::s_logfile = "";
string ImageFace::s_imagepath = "./";
int ImageFace::s_maxfaceid = 0;
bool ImageFace::s_update = false;
string ImageFace::s_personname = "";

string ImageFace::tostring(size_t i)
{
	char buf[64];
	sprintf(buf, "%zd", i);
	return string(buf);
}

void ImageFace::log(const char* format, ...)
{
	if (s_logfile != "")
	{
		FILE *f = fopen(s_logfile.c_str(), "a");
		if (f != nullptr)
		{
			va_list arglist;
			va_start(arglist, format);
			vfprintf(f, format, arglist);
			va_end(arglist);
			fclose(f);
		}
		else
		{
			va_list arglist;
			va_start(arglist, format);
			vprintf(format, arglist);
			va_end(arglist);
		}
	}
	else
	{
		va_list arglist;
		va_start(arglist, format);
		vprintf(format, arglist);
		va_end(arglist);
	}
}

int ImageFace::restoremaxfaceid()
{
	int id = 0;
	string fn = s_imagepath + "/" + "maxfaceid";
	FILE *f = fopen(fn.c_str(), "r");
	if( f != nullptr )
	{
		char data[32];
		fread(data, 32, 1, f);
		id = atoi(data);
		fclose(f);
	}
	return id;
}

void ImageFace::savemaxfaceid(int id)
{
	string fn = s_imagepath + "/" + "maxfaceid";
	FILE *f = fopen(fn.c_str(), "w");
	if (f != nullptr)
	{
		fprintf(f, "%d", id);
		fclose(f);
	}
}

string ImageFace::tostring(const std::vector<float>& facedescriptor)
{
	char buf[64];

	string res;

	for (int i = 0; i < 128; i++)
	{
		sprintf(buf, "%f", facedescriptor[i]);
		if (i != 0)
		{
			res += " ";
		}
		res += buf;
	}

	return res;
}

double ImageFace::distance(const std::vector<float>& facedescriptor1, const std::vector<float>& facedescriptor2)
{
	double q = 0;
	for (int i = 0; i < 128; i++)
	{
		double r = facedescriptor1[i] - facedescriptor2[i];
		q += r * r;
	}
	return sqrt(q);
}

void ImageFace::copy(const dlib::matrix<float, 0, 1>& facedescriptorsrc, std::vector<float>& facedescriptordst)
{
	facedescriptordst.clear();

	for (int i = 0; i < 128; i++)
	{
		facedescriptordst.push_back(facedescriptorsrc(i, 0));
	}
}

bool ImageFace::init(const string &path)
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

void ImageFace::process(const std::vector<string> &filenames, const std::vector<string> &outnames)
{
	for (int i = 0; i < filenames.size(); i++)
	{
		log("image: %s\n", filenames[i].c_str());

		ifstream ifile(filenames[i].c_str());

		if (!(bool)ifile)
		{
			log("error: not opened\n");
			continue;
		}

		std::vector<int> face_ids;
		std::vector<string> face_names;
		std::vector<Rect> face_rects;

		matrix<rgb_pixel> img;
		load_image(img, filenames[i]);

		if (!img.size() || !img.nr() || !img.nc())
		{
			log("error: bad image\n");
			continue;
		}

		int nr = img.nr();

		pyramid_up(img);

		float k = (float)nr / img.nr();

		std::vector<matrix<rgb_pixel> > faces;
		for (auto face : DETECTOR(img))
		{
			face_rects.push_back(Rect(k * face.left(), k * face.top(), k * (face.right() - face.left()), k * (face.bottom() - face.top())));
			auto shape = SP(img, face);
			matrix<rgb_pixel> face_chip;
			extract_image_chip(img, get_face_chip_details(shape, 150, 0.25), face_chip);
			faces.push_back(move(face_chip));
		}

		if (faces.size() == 0)
		{
			continue;
		}

		std::vector<matrix<float, 0, 1> > facedescriptors = NET(faces);
		std::vector<string> titles;

		Mat image = imread(filenames[i]);

		for (int j = 0; j < facedescriptors.size(); j++)
		{
			int faceid = ++s_maxfaceid;
			savemaxfaceid(s_maxfaceid);

			std::vector<float> facedescriptor;
			copy(facedescriptors[j], facedescriptor);

			PersonFace* person = PersonsFace::get(facedescriptor);
			if (!person)
			{
				person = new PersonFace();
				person->counter = 0;
				PersonsFace::add(person);
			}
			person->update(facedescriptor, 0, 1, s_personname);

			if (s_update)
			{
				PersonsFace::update();
			}

			titles.push_back(tostring(person->id) + " " + person->name);

			log("%d;%d;%d,%d;%d;%d;0[0];0[0]%s;%s\n",
				person->id,
				faceid,
				face_rects[j].x,
				face_rects[j].y,
				face_rects[j].width,
				face_rects[j].height,
				tostring(facedescriptor).c_str(),
				person->name.c_str());

			char filename[128];

			sprintf(filename, "%d-%d-%d-%d-%d-%d.jpg",
				person->id,
				faceid,
				face_rects[j].x,
				face_rects[j].y,
				face_rects[j].width,
				face_rects[j].height);

			{
				Mat image_;
				Scalar red = Scalar(0, 0, 255);
				image.copyTo(image_);
				cv::rectangle(image_, face_rects[j], red);
				imwrite(s_imagepath + "/" + filename, image_);
			}
			//imwrite(s_imagepath + "/" + filename, image);
		}

		if (i < outnames.size())
		{
			for (int j = 0; j < face_rects.size(); j++)
			{
				Scalar red = Scalar(0, 0, 255);
				cv::rectangle(image, face_rects[j], red);
				putText(image, titles[j], face_rects[j].tl(), FONT_HERSHEY_DUPLEX, 0.8, red);
			}

			imwrite(outnames[i], image);
		}
	}
}

ImageFace::ImageFace()
{
}

ImageFace::ImageFace(const ImageFace& imageface)
{
}

ImageFace& ImageFace::operator=(const ImageFace& imageface)
{
	return *this;
}

ImageFace::~ImageFace()
{
}
