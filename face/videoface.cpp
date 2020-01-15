
#include "stdafx.h"

//этот блок включений из dlib должен быть здесь, иначе много warning-ов
#include <dlib/dnn.h>
#include <dlib/gui_widgets.h>
#include <dlib/clustering.h>
#include <dlib/string.h>
#include <dlib/image_io.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/opencv.h>

#include <chrono>

#include "videoface.h"

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
frontal_face_detector VideoFace_detector;
shape_predictor VideoFace_sp;
//объект распознавания лиц
anet_type VideoFace_net;

#define DETECTOR VideoFace_detector
#define SP VideoFace_sp
#define NET VideoFace_net

string VideoFace::s_logfile = "";
string VideoFace::s_imagepath = "./";
string VideoFace::s_personpath = "./";
bool VideoFace::s_show = false;
bool VideoFace::s_kalman = true;
int VideoFace::s_bufsize = 5;
int VideoFace::s_frameskip = 5;
bool VideoFace::s_interpolation = false;
int VideoFace::s_missedframes = 50;
int VideoFace::s_neededframes = 10;
int VideoFace::s_maxfaceid = 0;
bool VideoFace::s_update = false;
PersonsFace VideoFace::s_persons;

SimpleKalmanFilter::SimpleKalmanFilter(float mea_e, float est_e, float q)
{
	err_measure = mea_e;
	err_estimate = est_e;
	this->q = q;
	is_first = 1;
}

float SimpleKalmanFilter::updateEstimate(float mea)
{
	if (is_first)
	{
		last_estimate = mea;
		is_first = 0;
	}

	kalman_gain = err_estimate / (err_estimate + err_measure);
	current_estimate = last_estimate + kalman_gain * (mea - last_estimate);
	err_estimate = (1.0f - kalman_gain) * err_estimate + fabs(last_estimate - current_estimate) * q;
	last_estimate = current_estimate;

	return current_estimate;
}

void SimpleKalmanFilter::setMeasurementError(float mea_e)
{
	err_measure = mea_e;
}

void SimpleKalmanFilter::setEstimateError(float est_e)
{
	err_estimate = est_e;
}

void SimpleKalmanFilter::setProcessNoise(float q)
{
	this->q = q;
}

float SimpleKalmanFilter::getKalmanGain() {
	return kalman_gain;
}

float SimpleKalmanFilter::getEstimateError() {
	return err_estimate;
}

string VideoFace::tostring(size_t i)
{
	char buf[64];
	sprintf(buf, "%zd", i);
	return string(buf);
}

string VideoFace::timecode(size_t framecount, int fps)
{
	double duration = (double)framecount / fps;

	int hours = int(duration / 3600);
	int minutes = int((duration - hours * 3600) / 60);
	int seconds = int(duration - hours * 3600 - minutes * 60);

	char buf[64];
	sprintf(buf, "%d:%02d:%02d", hours, minutes, seconds);

	return buf;
}

void VideoFace::log(const char* format, ...)
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

int VideoFace::restoremaxfaceid()
{
	int id = 0;
	string fn = s_imagepath + "/" + "maxfaceid";
	FILE *f = fopen(fn.c_str(), "r");
	if (f != nullptr)
	{
		char data[32];
		fread(data, 32, 1, f);
		id = atoi(data);
		fclose(f);
	}
	return id;
}

void VideoFace::savemaxfaceid(int id)
{
	string fn = s_imagepath + "/" + "maxfaceid";
	FILE *f = fopen(fn.c_str(), "w");
	if (f != nullptr)
	{
		fprintf(f, "%d", id);
		fclose(f);
	}
}

void VideoFace::copy(const dlib::matrix<float, 0, 1>& facedescriptorsrc, std::vector<float>& facedescriptordst)
{
	facedescriptordst.clear();

	for (int i = 0; i < 128; i++)
	{
		facedescriptordst.push_back(facedescriptorsrc(i, 0));
	}
}

bool VideoFace::init(const string &path)
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

	s_persons.init(s_personpath + "/" + "persons.csv");

	return true;
}

void VideoFace::process(const string &videosource)
{
	VideoCapture cap;

	log("video: %s\n", videosource.c_str());

	if (videosource.substr(0, 4) == "cam:")
	{
		cap.open(atoi(videosource.substr(4).c_str()));
	}
	else
	{
		cap.open(videosource);
	}

	if (!cap.isOpened())
	{
		log("error: not opened\n");
		return;
	}

	int fps = (int)cap.get(CAP_PROP_FPS);

	log("fps: %d\n", fps);

	Mat frame;

	std::vector<Mat> frames;
	std::vector<std::vector<FrameFace> > vectframefaces;

	std::map<int, StatusFace> status;

	size_t framecount = 0;
	size_t frameskip = s_frameskip < 0 ? 1 : s_frameskip + 1;//шаг для распознания
	size_t framestep = s_interpolation ? 1 : frameskip;//шаг для формирования кадров вывода

	while (cap.read(frame))
	{
		if (s_interpolation || framecount % framestep == 0)
		{
			std::vector<FrameFace> framefaces;

			if (!s_interpolation || framecount % frameskip == 0)
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

			Mat frame_;
			frame.copyTo(frame_);
			frames.push_back(frame_);
			vectframefaces.push_back(framefaces);

			if (frames.size() == s_bufsize)
			{
				processbuffer(videosource, fps, 1 + framecount - vectframefaces.size(), framestep, frames, vectframefaces, status);
			}
		}

		framecount++;
	}

	processbuffer(videosource, fps, framecount - vectframefaces.size(), framestep, frames, vectframefaces, status);

	if (s_show)
	{
		destroyAllWindows();
	}

	for (std::map<int, StatusFace>::iterator it = status.begin(); it != status.end(); it++)
	{
		if ((*it).second.lastframe - (*it).second.startframe + 1 >= s_neededframes)
		{
			if (s_update)
			{
				s_persons.update();
			}

			log("%d;%d;%d,%d;%d;%d;%zd[%s];%zd[%s];%s;%s\n",
				0,//personid
				(*it).first,
				(*it).second.startrect.x,
				(*it).second.startrect.y, 
				(*it).second.startrect.width,
				(*it).second.startrect.height, 
				(*it).second.startframe, timecode((*it).second.startframe, fps).c_str(),
				(*it).second.lastframe, timecode((*it).second.lastframe, fps).c_str(),
				PersonFace::tostring((*it).second.facedescriptor).c_str(),
				"");//personname

			char filename[128];

			sprintf(filename, "%d-%d-%d-%d-%d-%d.jpg",
				0,//personid
				(*it).first,
				(*it).second.startrect.x,
				(*it).second.startrect.y,
				(*it).second.startrect.width,
				(*it).second.startrect.height);

			{
				Mat image_;
				Scalar red = Scalar(0, 0, 255);
				(*it).second.image.copyTo(image_);
				cv::rectangle(image_, (*it).second.startrect, red);
				imwrite(s_imagepath + "/" + filename, image_);
			}
			//imwrite(s_imagepath + "/" + filename, (*it).second.image);
		}
	}

	log("frames: %zd\n", framecount);
}

void VideoFace::processbuffer(const string &name, int fps, size_t start, size_t step, std::vector<Mat> &frames, std::vector<std::vector<FrameFace> > &vectframefaces, std::map<int, StatusFace> &status)
{
	// распознаём - заполняем дескрипторы
	{
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

	float framewidth = 0;
	float frameheight = 0;

	if (frames.size())
	{
		framewidth = (float)frames[0].cols;
		frameheight = (float)frames[0].rows;
	}

	std::vector<std::map<int, FrameFace> > vectfaces;

	//цикл по кадрам
	for (int i = 0; i < frames.size(); i++)
	{
		std::vector<FrameFace> &framefaces = vectframefaces[i];
		std::map<int, FrameFace> faces;

		//цикл по обнаруженным лицам в кадре
		//if (i % 5 == 0)//для отладки симулируем пропуски обнаружения лиц в кадре
		for (int j = 0; j < framefaces.size(); j++)
		{
			FrameFace &face = framefaces[j];

			//ищем лицо в предыдущих
			int faceid = 0;
			//double best_iou;
			double best_dis;
			double best_len;
			//double threshold_iou = 0.01;
			double threshold_dis = 0.3;
			double threshold_len = 0.6;
			for (std::map<int, StatusFace>::iterator it = status.begin(); it != status.end(); it++)
			{
				//если запомненное лицо из предыдущих ещё не было распределёно
				if ((*it).second.lastframe < start + i * step)
				{
					//double iou = (double)((*it).second.lastrect & face.rect).area() / (double)((*it).second.lastrect | face.rect).area();
					double dis = sqrt(pow(((*it).second.lastrect.x + (double)(*it).second.lastrect.width / 2 - face.rect.x - (double)face.rect.width / 2) / framewidth, 2)
						+ pow(((*it).second.lastrect.y + (double)(*it).second.lastrect.height / 2 - face.rect.y - (double)face.rect.height / 2) / frameheight, 2));
					double len = PersonFace::distance((*it).second.facedescriptor, face.facedescriptor);

					if (len < threshold_len && dis < threshold_dis/* && iou < threshold_iou*/)
					{
						//приоритет либо для дистанции либо для отличия дескрипторов
						if (faceid == 0 || len < best_len )//dis < best_dis
						{
							faceid = (*it).first;
							//best_iou = iou;
							best_dis = dis;
							best_len = len;
						}
					}
				}
			}

			if (faceid == 0)
			{
				//не нашли, значит новый ид
				faceid = ++s_maxfaceid;
				savemaxfaceid(s_maxfaceid);
				status[faceid] = StatusFace();
				frames[i].copyTo(status[faceid].image);
				status[faceid].startframe = start + i * step;
				status[faceid].startrect = face.rect;
				status[faceid].counter = 0;
				status[faceid].deviation = 0;

				if (s_kalman)
				{
					//инициализируем фильтры калмана для rect
					status[faceid].filters.push_back(SimpleKalmanFilter(framewidth / 10, framewidth / 10, 1.0));//x
					status[faceid].filters.push_back(SimpleKalmanFilter(frameheight / 10, frameheight / 10, 1.0));//y
					status[faceid].filters.push_back(SimpleKalmanFilter(framewidth / 10, framewidth / 10, 1.0));//width
					status[faceid].filters.push_back(SimpleKalmanFilter(frameheight / 10, frameheight / 10, 1.0));//height
				}
			}
			else if (status[faceid].missedframes != 0)
			{
				//нашли лицо, которое было пропущено на нескольких кадрах, теперь на пропущенных кадрах его нужно восстановить - интерполировать
				//номер кадра, начиная с которого не было обнаружения
				size_t k = status[faceid].missedframes < i ? i - status[faceid].missedframes : 0;
				for (; k < i; k++)
				{
					vectfaces[k][faceid] = FrameFace();
					vectfaces[k][faceid].rect = status[faceid].lastrect;
					vectfaces[k][faceid].facedescriptor = status[faceid].facedescriptor;
				}
			}

			if (s_kalman)
			{
				//обрабатываем фильтрами калмана
				face.rect.x = (int)round(status[faceid].filters[0].updateEstimate((float)face.rect.x));
				face.rect.y = (int)round(status[faceid].filters[1].updateEstimate((float)face.rect.y));
				face.rect.width = (int)round(status[faceid].filters[2].updateEstimate((float)face.rect.width));
				face.rect.height = (int)round(status[faceid].filters[3].updateEstimate((float)face.rect.height));
			}

			//обновляем инфу в состоянии
			status[faceid].lastframe = start + i * step;
			status[faceid].lastrect = face.rect;
			status[faceid].missedframes = 0;
			status[faceid].append(face.facedescriptor);

			//инфа об ограничивающих боксах лиц для кадра
			if (status[faceid].lastframe - status[faceid].startframe + 1 >= s_neededframes)
			{
				faces[faceid] = FrameFace();
				faces[faceid].rect = face.rect;
				faces[faceid].face = face.face;
				faces[faceid].facedescriptor = status[faceid].facedescriptor;
			}
		}

		//ищем не обнаруженные в кадре лица, если устарели - удаляем
		for (std::map<int, StatusFace>::iterator it = status.begin(); it != status.end(); )
		{
			if ((*it).second.lastframe < start + i * step)
			{
				(*it).second.missedframes += step;
			}

			if ((*it).second.missedframes >= s_missedframes)
			{
				if ((*it).second.lastframe - (*it).second.startframe + 1 >= s_neededframes)
				{
					if (s_update)
					{
						s_persons.update();
					}

					log("%d;%d;%d,%d;%d;%d;%zd[%s];%zd[%s];%s;%s\n",
						0,//personid
						(*it).first,
						(*it).second.startrect.x,
						(*it).second.startrect.y,
						(*it).second.startrect.width,
						(*it).second.startrect.height,
						(*it).second.startframe, timecode( (*it).second.startframe, fps ).c_str(),
						(*it).second.lastframe, timecode( (*it).second.lastframe, fps ).c_str(),
						PersonFace::tostring( (*it).second.facedescriptor ).c_str(),
						"");//personname

					char filename[128];

					sprintf(filename, "%d-%d-%d-%d-%d-%d.jpg",
						0,//personid
						(*it).first,
						(*it).second.startrect.x,
						(*it).second.startrect.y,
						(*it).second.startrect.width,
						(*it).second.startrect.height);

					{
						Mat image_;
						Scalar red = Scalar(0, 0, 255);
						(*it).second.image.copyTo(image_);
						cv::rectangle(image_, (*it).second.startrect, red);
						imwrite(s_imagepath + "/" + filename, image_);
					}
					//imwrite(s_imagepath + "/" + filename, (*it).second.image);
				}
				it = status.erase(it);
			}
			else
			{
				it++;
			}
		}

		vectfaces.push_back(faces);
	}

	if (s_show)
	{
		chrono::high_resolution_clock::time_point time = chrono::high_resolution_clock::now();

		for (int i = 0; i < frames.size(); i++)
		{
			std::map<int, FrameFace> &faces = vectfaces[i];

			for (std::map<int, FrameFace>::iterator it = faces.begin(); it != faces.end(); it++)
			{
				Scalar red = Scalar(0, 0, 255);
				cv::rectangle(frames[i], (*it).second.rect, red);
				putText(frames[i], tostring((*it).first), (*it).second.rect.tl(), FONT_HERSHEY_DUPLEX, 0.8, red);
			}

			imshow(name, frames[i]);

			int delay = 1000 / fps;
			int duration = (int)chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - time).count();

			if (duration < delay)
			{
				waitKey(delay - duration);
			}

			time = chrono::high_resolution_clock::now();
		}
	}

	frames.clear();
	vectframefaces.clear();
}


VideoFace::VideoFace()
{
}

VideoFace::VideoFace(const VideoFace& videoface)
{
}

VideoFace& VideoFace::operator=(const VideoFace& videoface)
{
	return *this;
}

VideoFace::~VideoFace()
{
}
