
#include "stdafx.h"

#include <chrono>

#include "videoface.h"

string VideoFace::s_logfile = "";
string VideoFace::s_imagepath = "./";
bool VideoFace::s_show = false;
bool VideoFace::s_kalman = true;
int VideoFace::s_bufsize = 10;
int VideoFace::s_frameskip = 5;
bool VideoFace::s_interpolation = false;
int VideoFace::s_missedframes = 50;
int VideoFace::s_neededframes = 10;
int VideoFace::s_neededfaces = 10;
int VideoFace::s_maxfaceid = 0;
bool VideoFace::s_update = false;

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
				RecogFace::detect(frame, framefaces);
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
		if ((*it).second.lastframe - (*it).second.startframe + 1 >= s_neededframes && (*it).second.person->id != 0)
		{
			log("%d;%d;%d,%d;%d;%d;%zd[%s];%zd[%s];%s;%s\n",
				(*it).second.person->id,
				(*it).first,
				(*it).second.startrect.x,
				(*it).second.startrect.y, 
				(*it).second.startrect.width,
				(*it).second.startrect.height, 
				(*it).second.startframe, timecode((*it).second.startframe, fps).c_str(),
				(*it).second.lastframe, timecode((*it).second.lastframe, fps).c_str(),
				PersonFace::tostring((*it).second.person->facedescriptor).c_str(),
				(*it).second.person->name.c_str());

			char filename[128];

			sprintf(filename, "%d-%d-%d-%d-%d-%d.jpg",
				(*it).second.person->id,
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

		if ((*it).second.person->id == 0)
		{
			delete (*it).second.person;
		}
	}

	if( s_update )
	{
		PersonsFace::update();
	}

	log("frames: %zd\n", framecount);
}

void VideoFace::processbuffer(const string &name, int fps, size_t start, size_t step, std::vector<Mat> &frames, std::vector<std::vector<FrameFace> > &vectframefaces, std::map<int, StatusFace> &status)
{
	RecogFace::recog(vectframefaces);

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
			double threshold_dis = 0.2;
			double threshold_len = 0.6;
			for (std::map<int, StatusFace>::iterator it = status.begin(); it != status.end(); it++)
			{
				//если запомненное лицо из предыдущих ещё не было распределёно
				if ((*it).second.lastframe < start + i * step)
				{
					//double iou = (double)((*it).second.lastrect & face.rect).area() / (double)((*it).second.lastrect | face.rect).area();
					double dis = sqrt(pow(((*it).second.lastrect.x + (double)(*it).second.lastrect.width / 2 - face.rect.x - (double)face.rect.width / 2) / framewidth, 2)
						+ pow(((*it).second.lastrect.y + (double)(*it).second.lastrect.height / 2 - face.rect.y - (double)face.rect.height / 2) / frameheight, 2));
					double len = PersonFace::distance((*it).second.person->facedescriptor, face.facedescriptor);

					if (len < threshold_len && dis < threshold_dis/* && iou < threshold_iou*/)
					{
						//приоритет либо для дистанции либо для отличия дескрипторов
						if (faceid == 0 || dis < best_dis)//len < best_len
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
				status[faceid].searchfailed = false;
				status[faceid].person = new PersonFace();
				status[faceid].person->id = 0;
				status[faceid].person->next = nullptr;
				status[faceid].person->counter = 0;
				status[faceid].person->deviation = 0;

				if (s_kalman)
				{
					//инициализируем фильтры калмана для rect
					status[faceid].filters.push_back(SimpleKalmanFilter(framewidth / 10, framewidth / 10, 1.0));//x
					status[faceid].filters.push_back(SimpleKalmanFilter(frameheight / 10, frameheight / 10, 1.0));//y
					status[faceid].filters.push_back(SimpleKalmanFilter(framewidth / 10, framewidth / 10, 1.0));//width
					status[faceid].filters.push_back(SimpleKalmanFilter(frameheight / 10, frameheight / 10, 1.0));//height
				}
			}
			else if (status[faceid].missedframes != 0 && status[faceid].person->id != 0)
			{
				//нашли лицо, которое было пропущено на нескольких кадрах, теперь на пропущенных кадрах его нужно восстановить - интерполировать
				//номер кадра, начиная с которого не было обнаружения
				size_t k = status[faceid].missedframes < i ? i - status[faceid].missedframes : 0;
				for (; k < i; k++)
				{
					vectfaces[k][faceid] = FrameFace();
					vectfaces[k][faceid].rect = status[faceid].lastrect;
					vectfaces[k][faceid].facedescriptor = status[faceid].person->facedescriptor;
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

			//игнорируем на очередном кадре скорее всего фото или статически-неподвижное лицо (т.к. для статистки это вредно)
			if (status[faceid].lastdescriptor.size() == 0 || PersonFace::distance(status[faceid].lastdescriptor, face.facedescriptor) > 0.1)
			{
				status[faceid].lastdescriptor = face.facedescriptor;
				status[faceid].person->update(face.facedescriptor, 0, 1);
			}

			//инфа об ограничивающих боксах лиц для кадра
			if (status[faceid].lastframe - status[faceid].startframe + 1 >= s_neededframes)
			{
				if (status[faceid].person->id == 0)
				{
					PersonFace* person = nullptr;

					//если пробовали найти и не нашли, значит больше не стоит искать
					if (status[faceid].searchfailed == false)
					{
						person = PersonsFace::get(status[faceid].person->facedescriptor);
						status[faceid].searchfailed = !person;
					}

					if (person)
					{
						person->update(status[faceid].person->facedescriptor, status[faceid].person->deviation, status[faceid].person->counter);
						delete status[faceid].person;
						status[faceid].person = person;
					}
					else if (status[faceid].person->counter >= s_neededfaces && s_update)
					{
						PersonsFace::add(status[faceid].person);
					}
				}

				faces[faceid] = FrameFace();
				faces[faceid].rect = face.rect;
				faces[faceid].face = face.face;
				faces[faceid].facedescriptor = status[faceid].person->facedescriptor;
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
				if ((*it).second.lastframe - (*it).second.startframe + 1 >= s_neededframes && (*it).second.person->id != 0)
				{
					log("%d;%d;%d,%d;%d;%d;%zd[%s];%zd[%s];%s;%s\n",
						(*it).second.person->id,
						(*it).first,
						(*it).second.startrect.x,
						(*it).second.startrect.y,
						(*it).second.startrect.width,
						(*it).second.startrect.height,
						(*it).second.startframe, timecode((*it).second.startframe, fps).c_str(),
						(*it).second.lastframe, timecode((*it).second.lastframe, fps).c_str(),
						PersonFace::tostring((*it).second.person->facedescriptor).c_str(),
						(*it).second.person->name.c_str());

					char filename[128];

					sprintf(filename, "%d-%d-%d-%d-%d-%d.jpg",
						(*it).second.person->id,
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

				if ((*it).second.person->id == 0)
				{
					delete (*it).second.person;
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
				if (status[(*it).first].person->id)
				{
					string title = tostring(status[(*it).first].person->id);
					putText(frames[i], title, (*it).second.rect.tl(), FONT_HERSHEY_DUPLEX, 0.8, red);
				}
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
