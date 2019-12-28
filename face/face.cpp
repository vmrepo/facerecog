// face.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"

#include "stdlib.h"
#include "stdio.h"

#include "imageface.h"
#include "videoface.h"

#if _WIN32 || _WIN64
#define strcasecmp _stricmp
#define strdup _strdup
#endif

char* basename(char* path)
{
	char* ptmp = path;

	for (char* p = ptmp; *p != '\0'; p++)
	{
		// remember last directory/drive separator
		if (*p == '\\' || *p == '/' || *p == ':') {
			ptmp = p + 1;
		}
	}

	return ptmp;
}

using namespace std;
using namespace cv;

enum Status
{
	ExpectAny = 0,
	ExpectImage,
	ExpectOut,
	ExpectVideo,
	ExpectFrameskip,
	ExpectBufferSize,
	ExpectLogFile,
	ExpectFramePath,
	ExpectPersonPath,
	ExpectPerson
};

#define UNINIT

int main(int argc, char** argv)
{
	setlocale(LC_ALL, "");

	char* path = strdup(argv[0]);

	char* ptmp = basename(path);

	*ptmp = 0;

	string pathself = path;

	free(path);

	vector<string> images;
	vector<string> outs;
	string video = "";

	Status status = ExpectAny;

	if (argc == 1)
	{
		printf("Bad parameters");
		UNINIT
		return 0;
	}

	for (int i = 1; i < argc; i++)
	{
		if (!strcasecmp(argv[i], "-image"))
		{
			if (status != ExpectAny || images.size() != 0 || outs.size() != 0  || video.size() != 0)
			{
				printf("Bad parameter %s", argv[i]);
				UNINIT
				return 0;
			}

			status = ExpectImage;
		}
		else if (!strcasecmp(argv[i], "-out"))
		{
			if ((status != ExpectAny && status != ExpectImage) || images.size() == 0 || outs.size() != 0 || video.size() != 0)
			{
				printf("Bad parameter %s", argv[i]);
				UNINIT
				return 0;
			}

			status = ExpectOut;
		}
		else if (!strcasecmp(argv[i], "-video"))
		{
			if (status != ExpectAny || images.size() != 0 || outs.size() != 0 || video.size() != 0)
			{
				printf("Bad parameter %s", argv[i]);
				UNINIT
				return 0;
			}

			status = ExpectVideo;
		}
		else if (!strcasecmp(argv[i], "-frameskip"))
		{
			if (status != ExpectAny || images.size() != 0 || outs.size() != 0 || video.size() == 0)
			{
				printf("Bad parameter %s", argv[i]);
				UNINIT
				return 0;
			}

			status = ExpectFrameskip;
		}
		else if (!strcasecmp(argv[i], "-buffersize"))
		{
			if (status != ExpectAny || images.size() != 0 || outs.size() != 0 || video.size() == 0)
			{
				printf("Bad parameter %s", argv[i]);
				UNINIT
				return 0;
			}

			status = ExpectBufferSize;
		}
		else if (!strcasecmp(argv[i], "-logfile"))
		{
			if ((status != ExpectAny && status != ExpectImage && status != ExpectOut) || (images.size() == 0 && video.size() == 0))
			{
				printf("Bad parameter %s", argv[i]);
				UNINIT
				return 0;
			}

			status = ExpectLogFile;
		}
		else if (!strcasecmp(argv[i], "-framepath"))
		{
			if ((status != ExpectAny && status != ExpectImage && status != ExpectOut) || (images.size() == 0 && video.size() == 0))
			{
				printf("Bad parameter %s", argv[i]);
				UNINIT
				return 0;
			}

			status = ExpectFramePath;
		}
		else if (!strcasecmp( argv[i], "-personpath"))
		{
			if ((status != ExpectAny && status != ExpectImage && status != ExpectOut) || (images.size() == 0 && video.size() == 0))
			{
				printf("Bad parameter %s", argv[i]);
				UNINIT
				return 0;
			}

			status = ExpectPersonPath;
		}
		else if (!strcasecmp(argv[i], "-showon"))
		{
			if (status != ExpectAny || images.size() != 0 || outs.size() != 0 || video.size() == 0)
			{
				printf("Bad parameter %s", argv[i]);
				UNINIT
				return 0;
			}
			else
			{
				VideoFace::s_show = true;
				VideoFace::s_interpolation = true;
			}

			status = ExpectAny;
		}
		else if (!strcasecmp(argv[i], "-kalmanoff"))
		{
			if (status != ExpectAny || images.size() != 0 || outs.size() != 0 || video.size() == 0)
			{
				printf("Bad parameter %s", argv[i]);
				UNINIT
				return 0;
			}
			else
			{
				VideoFace::s_kalman = false;
			}

			status = ExpectAny;
		}
		else if( !strcasecmp( argv[i], "-update" ) )
		{
			if ((status != ExpectAny && status != ExpectImage && status != ExpectOut) || (images.size() == 0 && video.size() == 0) || (status == ExpectOut && outs.size() == 0))
			{
				printf( "Bad parameter %s", argv[i] );
				UNINIT
				return 0;
			}
			else
			{
				ImageFace::s_update = true;
				VideoFace::s_update = true;
			}

			status = ExpectAny;
		}
		else if( !strcasecmp( argv[i], "-person" ) )
		{
			if ((status != ExpectAny && status != ExpectImage && status != ExpectOut) || images.size() == 0 || video.size() != 0 || (status == ExpectOut && outs.size() == 0))
			{
				printf( "Bad parameter %s", argv[i] );
				UNINIT
				return 0;
			}

			status = ExpectPerson;
		}
		else
		{
			switch (status)
			{
			case ExpectImage:
			{
				images.push_back(argv[i]);
			}
			break;

			case ExpectOut:
			{
				if (outs.size() == images.size())
				{
					printf("Bad parameter %s", argv[i]);
					UNINIT
					return 0;
				}
				outs.push_back(argv[i]);
			}
			break;

			case ExpectVideo:
			{
				if (video.size() != 0)
				{
					printf("Bad parameter %s", argv[i]);
					UNINIT
					return 0;
				}
				video = argv[i];

				status = ExpectAny;
			}
			break;

			case ExpectFrameskip:
			{
				VideoFace::s_frameskip = atoi(argv[i]);

				status = ExpectAny;
			}
			break;

			case ExpectBufferSize:
			{
				VideoFace::s_bufsize = atoi(argv[i]);

				status = ExpectAny;
			}
			break;

			case ExpectLogFile:
			{
				ImageFace::s_logfile = argv[i];
				VideoFace::s_logfile = argv[i];

				status = ExpectAny;
			}
			break;

			case ExpectFramePath:
			{
				ImageFace::s_imagepath = argv[i];
				VideoFace::s_imagepath = argv[i];

				status = ExpectAny;
			}
			break;

			case ExpectPersonPath:
			{
				ImageFace::s_personpath = argv[i];
				VideoFace::s_personpath = argv[i];

				status = ExpectAny;
			}
			break;

			case ExpectPerson:
			{
				ImageFace::s_personname = argv[i];

				status = ExpectAny;
			}
			break;

			default:
			{
				printf("Bad  parameter %s", argv[i]);
				UNINIT
				return 0;
			}
			}
		}
	}

	if ((status == ExpectImage && images.size() == 0) || 
		(status == ExpectOut && outs.size() == 0) ||
		status == ExpectFrameskip || 
		status == ExpectBufferSize ||
		status == ExpectLogFile ||
		status == ExpectFramePath ||
		status == ExpectPersonPath ||
		status == ExpectPerson ||
		status == ExpectVideo)
	{
		printf("Bad  parameters");
		UNINIT
		return 0;
	}

	if (ImageFace::init(pathself))
	{
		ImageFace::s_maxfaceid = ImageFace::restoremaxfaceid();
		ImageFace::process(images, outs);
	}

	if (video.size())
	{
		if (VideoFace::init(pathself))
		{
			VideoFace::s_maxfaceid = VideoFace::restoremaxfaceid();
			VideoFace::process(video);
		}
	}

	UNINIT
	return 0;
}
