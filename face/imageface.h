#ifndef __IMAGEFACE_H__
#define __IMAGEFACE_H__

#include "stdlib.h"
#include "stdio.h"

#include "recogface.h"
#include "personsface.h"

using namespace std;
using namespace cv;

struct ImageFace
{
	static string tostring(size_t i);
	static void log(const char* format, ...);

	static int restoremaxfaceid();
	static void savemaxfaceid(int id);

	static void process(const std::vector<string> &filenames, const std::vector<string> &outnames);

	ImageFace();
	ImageFace(const ImageFace& imageface);
	ImageFace& operator=(const ImageFace& imageface);

	virtual ~ImageFace();

	static string s_logfile;
	static string s_imagepath;
	static int s_neededfaces;
	static int s_maxfaceid;
	static bool s_update;
	static string s_personname;
};

#endif // __IMAGEFACE_H__
