#ifndef __PERSONSFACE_H__
#define __PERSONSFACE_H__

#include "stdlib.h"
#include "stdio.h"

#include <vector>

using namespace std;

struct PersonFace {
	int id;
	int counter;
	vector<float> facedescriptor;
	string name;
	void update(const vector<float>& facedescriptor_, int counter_, const string& name_ = "")
	{
		if (counter == 0)
		{
			facedescriptor = facedescriptor_;
		}
		else
		{
			for (int i = 0; i < 128; i++)
			{
				facedescriptor[i] = (counter * facedescriptor[i] + counter_ * facedescriptor_[i]) / (counter + counter_);
			}
		}

		counter += counter_;

		if (!name_.empty())
		{
			name = name_;
		}
	}
};

struct PersonsFace
{
	static string tostring( const std::vector<float>& facedescriptor );
	static double distance( const std::vector<float>& facedescriptor1, const std::vector<float>& facedescriptor );

	static void init(const string& filepath);
	static void update();
	static PersonFace& get(const vector<float>& facedescriptor);

	PersonsFace();
	PersonsFace(const PersonsFace& personsface);
	PersonsFace& operator=(const PersonsFace& personsface);

	virtual ~PersonsFace();

	static string s_filepath;
	static vector<PersonFace> s_persons;
	static int s_maxpersonid;
};

#endif // __PERSONSFACE_H__
