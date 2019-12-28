#ifndef __IMAGEFACE_H__
#define __IMAGEFACE_H__

#include "stdlib.h"
#include "stdio.h"

#include <dlib/matrix.h>

#include "opencv2/opencv.hpp"

#include "personsface.h"

using namespace std;
using namespace cv;

struct ImageFace
{
	static string tostring(size_t i);
	static void log(const char* format, ...);

	static int restoremaxfaceid();
	static void savemaxfaceid( int id );

	static string tostring(const std::vector<float>& facedescriptor);
	static double distance(const std::vector<float>& facedescriptor1, const std::vector<float>& facedescriptor2);
	static void copy(const dlib::matrix<float, 0, 1>& facedescriptorsrc, std::vector<float>& facedescriptordst);

	static bool init(const string &path);
	static void process(const std::vector<string> &filenames, const std::vector<string> &outnames);

	ImageFace();
	ImageFace(const ImageFace& imageface);
	ImageFace& operator=(const ImageFace& imageface);

	virtual ~ImageFace();

	static string s_logfile;
	static string s_imagepath;
	static string s_personpath;
	static int s_maxfaceid;
	static bool s_update;
	static string s_personname;
	static PersonsFace s_persons;
};

#endif // __IMAGEFACE_H__
