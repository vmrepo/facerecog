
#include "stdafx.h"

#include "imageface.h"

string ImageFace::s_logfile = "";
string ImageFace::s_imagepath = "./";
int ImageFace::s_neededfaces = 10;
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

void ImageFace::process(const std::vector<string> &filenames, const std::vector<string> &outnames)
{
	std::vector<float> lastdescriptor;
	std::vector<vector<float> > alldescriptors;
	PersonFace* person = nullptr;

	for (int i = 0; i < filenames.size(); i++)
	{
		log("image: %s\n", filenames[i].c_str());

		ifstream ifile(filenames[i].c_str());

		if (!(bool)ifile)
		{
			log("error: not opened\n");
			continue;
		}

		Mat image = imread(filenames[i]);

		if (!(image.cols > 0 && image.rows > 0))
		{
			log("error: bad image\n");
			continue;
		}

		std::vector<FrameFace> framefaces;
		RecogFace::detect(image, framefaces);

		std::vector<std::vector<FrameFace> > vectframefaces;
		vectframefaces.push_back(framefaces);
		RecogFace::recog(vectframefaces);

		std::vector<string> titles;

		for (int j = 0; j < vectframefaces[0].size(); j++)
		{
			FrameFace &frameface = vectframefaces[0][j];

			int faceid = ++s_maxfaceid;
			savemaxfaceid(s_maxfaceid);

			if (s_update == false)
			{
				int person_id = 0;
				string person_name = "";

				PersonFace* person = PersonsFace::get(frameface.facedescriptor);

				if (person)
				{
					person_id = person->id;
					person_name = person->name;
				}

				titles.push_back(person_id != 0 ? tostring(person_id) + " " + person_name : "");

				if (person_id != 0)
				{
					log("%d;%d;%d,%d;%d;%d;0[0];0[0]%s;%s\n",
						person_id,
						faceid,
						frameface.rect.x,
						frameface.rect.y,
						frameface.rect.width,
						frameface.rect.height,
						PersonFace::tostring(frameface.facedescriptor).c_str(),
						person_name.c_str());

					char filename[128];

					sprintf(filename, "%d-%d-%d-%d-%d-%d.jpg",
						person_id,
						faceid,
						frameface.rect.x,
						frameface.rect.y,
						frameface.rect.width,
						frameface.rect.height);

					{
						Mat image_;
						Scalar red = Scalar(0, 0, 255);
						image.copyTo(image_);
						cv::rectangle(image_, frameface.rect, red);
						imwrite(s_imagepath + "/" + filename, image_);
					}
					//imwrite(s_imagepath + "/" + filename, image);
				}
			}
			else//s_update == true
			{
				bool ok = true;

			 	if (lastdescriptor.size())
				{
					float len = PersonFace::distance(frameface.facedescriptor, lastdescriptor);

					if (len > 0.6)
					{
						log("all faces must be same person, ignored probably other person face: %s\n", filenames[i]);
						ok = false;
					} 
					else
					{
						for (int k = 0; k < alldescriptors.size(); k++)
						{
							float len = PersonFace::distance(frameface.facedescriptor, alldescriptors[k]);
							if( len < 0.1 ) {
								log( "faces must be diverse, ignored same face: %s\n", filenames[i] );
								ok = false;
								break;
							}
						}
					}
				}

				if (ok)
				{
					if (person == nullptr)
					{
						person = new PersonFace();
						person->id = 0;
						person->next = nullptr;
						person->counter = 0;
						person->deviation = 0;
					}
					person->update(frameface.facedescriptor, 0, 1);
					lastdescriptor = frameface.facedescriptor;
					alldescriptors.push_back(frameface.facedescriptor);
				}
			}
		}

		if (s_update == true)
		{
			continue;
		}

		if (i < outnames.size())
		{
			for (int j = 0; j < vectframefaces[0].size(); j++)
			{
				FrameFace &frameface = vectframefaces[0][j];

				Scalar red = Scalar(0, 0, 255);
				cv::rectangle(image, frameface.rect, red);
				if (titles[i].size())
				{
					putText(image, titles[j], frameface.rect.tl(), FONT_HERSHEY_DUPLEX, 0.8, red);
				}
			}

			imwrite(outnames[i], image);
		}
	}

	if (s_update == true)
	{
		if (person)
		{
			if (person->counter >= s_neededfaces)
			{
				PersonFace* p = PersonsFace::get(person->facedescriptor);
				if (p)
				{
					p->update(person->facedescriptor, person->deviation, person->counter);
					if (s_personname.size())
					{
						p->name = s_personname;
					}
					PersonsFace::update();
					log("updated existing person %d\n", person->id);
					delete person;
				}
				else
				{
					person->name = s_personname;
					PersonsFace::add(person);
					PersonsFace::update();
					log("appended new person %d\n", person->id);
				}
			}
			else
			{
				log("not enough faces %d, need %d:\n", person->counter, s_neededfaces);
				delete person;
			}
		}
		else
		{
			log("not found faces\n");
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
