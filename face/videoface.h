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
	std::vector<float> facedescriptor;
	size_t counter;
	std::vector<SimpleKalmanFilter> filters;
	std::vector<std::vector<float> > descriptors;
	//std::vector<float> sum1;
	//std::vector<float> sum2;
	float distance( const std::vector<float>& facedescriptor1, const std::vector<float>& facedescriptor2 )
	{
		double q = 0;
		for( int i = 0; i < 128; i++ )
		{
			double r = facedescriptor1[i] - facedescriptor2[i];
			q += r * r;
		}
		return sqrt( q );
	}
	void append(const std::vector<float>& facedescriptor_)
	{
		descriptors.push_back(facedescriptor_);

		if ( counter == 0)
		{
			facedescriptor = facedescriptor_;
		}
		else
		{
			for (int i = 0; i < 128; i++)
			{
				facedescriptor[i] = (counter * facedescriptor[i] + facedescriptor_[i]) / (counter + 1);
			}
		}

		counter++;
	}
	/*float spread()
	{
		if (descriptors.size() == 0)
		{
			return -1;
		}
		double sum = 0;
		for (int i = 0; i < descriptors.size(); i++)
		{
			sum += distance(descriptors[i], facedescriptor);
		}
		return sum / descriptors.size();
	}*/
	float spread()
	{
		return descriptors.size();

		if( descriptors.size() == 0 )
		{
			return -1;
		}
		double min = 1;

		for( int i = 0; i < descriptors.size(); i++ )
		{
			double d = distance( descriptors[i], facedescriptor );
			if( d < min )
			{
				min = d;
			}
		}

		return min;
	}
	/*void append( const std::vector<float>& facedescriptor_ )
	{
		if( counter == 0 )
		{
			facedescriptor = facedescriptor_;
			sum1 = facedescriptor_;
			for( int i = 0; i < 128; i++ )
			{
				sum2.push_back( facedescriptor_[i] * facedescriptor_[i] );
			}
		}
		else
		{
			for( int i = 0; i < 128; i++ )
			{
				facedescriptor[i] = (counter * facedescriptor[i] + facedescriptor_[i]) / (counter + 1);
				sum1[i] += facedescriptor_[i];
				sum2[i] += facedescriptor_[i] * facedescriptor_[i];
			}
		}

		counter++;
	}
	float spread()
	{
		double q = 0;

		for( int i = 0; i < 128; i++ )
		{
			q += (sum2[i] - sum1[i] * sum1[i] / counter) / counter;
		}

		return sqrt( q );
	}*/
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

	static string tostring(const std::vector<float>& facedescriptor);
	static double distance(const std::vector<float>& facedescriptor1, const std::vector<float>& facedescriptor2);
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
