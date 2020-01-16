#ifndef __VIDEOFACE_H__
#define __VIDEOFACE_H__

#include "stdlib.h"
#include "stdio.h"

#include <dlib/pixel.h>
#include <dlib/matrix.h>

#include "opencv2/opencv.hpp"

#include "personsface.h"

using namespace std;
using namespace cv;

class SimpleKalmanFilter
{
public:

	SimpleKalmanFilter(float mea_e, float est_e, float q);
	float updateEstimate(float mea);
	void setMeasurementError(float mea_e);
	void setEstimateError(float est_e);
	void setProcessNoise(float q);
	float getKalmanGain();
	float getEstimateError();

private:
	float err_measure;
	float err_estimate;
	float q;
	float current_estimate;
	float last_estimate;
	float kalman_gain;
	int is_first;
};

struct StatusFace
{
	Mat image;//для startframe
	size_t startframe;//включительно
	size_t lastframe;//включительно
	Rect startrect;
	Rect lastrect;
	size_t missedframes;
	PersonFace *person;
	std::vector<SimpleKalmanFilter> filters;
};

struct FrameFace
{
	Rect rect;
	dlib::matrix<dlib::rgb_pixel> face;
	std::vector<float> facedescriptor;
};

struct VideoFace
{
	static string tostring(size_t i);
	static string timecode(size_t framecount, int fps);
	static void log(const char* format, ...);

	static int restoremaxfaceid();
	static void savemaxfaceid(int id);

	static void copy(const dlib::matrix<float, 0, 1>& facedescriptorsrc, std::vector<float>& facedescriptordst);

	static bool init(const string &path);
	static void process(const string &videosource);
	static void processbuffer(const string &name, int fps, size_t start, size_t step, std::vector<Mat> &frames, std::vector<std::vector<FrameFace> > &vectframefaces, std::map<int, StatusFace> &status);

	VideoFace();
	VideoFace(const VideoFace& videoface);
	VideoFace& operator=(const VideoFace& videoface);

	virtual ~VideoFace();

	static string s_logfile;
	static string s_imagepath;
	static string s_personpath;
	static bool s_show;
	static bool s_kalman;
	static int s_bufsize;
	static int s_frameskip;
	static bool s_interpolation;
	static int s_missedframes;
	static int s_neededframes;
	static int s_maxfaceid;
	static bool s_update;
	static PersonsFace s_persons;
};

#endif // __VIDEOFACE_H__
