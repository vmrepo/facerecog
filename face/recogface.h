#ifndef __RECOGFACE_H__
#define __RECOGFACE_H__

#include "stdlib.h"
#include "stdio.h"

#include <dlib/pixel.h>
#include <dlib/matrix.h>

#include "opencv2/opencv.hpp"

using namespace std;
using namespace cv;

struct FrameFace
{
	Rect rect;
	dlib::matrix<dlib::rgb_pixel> face;
	std::vector<float> facedescriptor;
};

struct RecogFace
{
	static void copy(const dlib::matrix<float, 0, 1>& facedescriptorsrc, std::vector<float>& facedescriptordst);

	static bool init(const string &path);
	static void detect(const Mat& frame, std::vector<FrameFace>& framefaces);
	static void recog(std::vector<std::vector<FrameFace> > &vectframefaces);

	RecogFace();
	RecogFace(const RecogFace& recogface);
	RecogFace& operator=(const RecogFace& recogface);

	virtual ~RecogFace();
};

#endif // __RECOGFACE_H__
