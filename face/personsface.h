#ifndef __PERSONSFACE_H__
#define __PERSONSFACE_H__

#include "stdlib.h"
#include "stdio.h"

#include <vector>

using namespace std;

struct PersonFace {

	static vector<float> parse(const char* str);
	static string tostring(const vector<float>& facedescriptor);
	static double distance(const vector<float>& facedescriptor1, const vector<float>& facedescriptor2);
	static vector<float> subtract(const vector<float>& facedescriptor1, const vector<float>& facedescriptor2);
	static double scalar(const vector<float>& facedescriptor1, const vector<float>& facedescriptor2);
	static vector<float> aggregate(const vector<float>& facedescriptor1, float deviation1, size_t counter1,
			const vector<float>& facedescriptor2, float deviation2, size_t counter2,
			float* deviation, size_t* counter);

	int id;
	size_t counter;
	float deviation;
	vector<float> facedescriptor;
	string name;
	void update(const vector<float>& facedescriptor_, float deviation_, size_t counter_, const string& name_ = "")
	{
		if (counter == 0)
		{
			facedescriptor = facedescriptor_;
			deviation_ = deviation_;
			counter = counter_;
		}
		else
		{
			facedescriptor = aggregate(facedescriptor, deviation, counter, facedescriptor_, deviation_, counter_, &deviation, &counter);
		}

		if (!name_.empty())
		{
			name = name_;
		}
	}
};

struct PersonsFace
{
	static void init(const string& filepath);
	static void update();
	static PersonFace& get(const vector<float>& facedescriptor, float deviation);

	PersonsFace();
	PersonsFace(const PersonsFace& personsface);
	PersonsFace& operator=(const PersonsFace& personsface);

	virtual ~PersonsFace();

	static string s_filepath;
	static vector<PersonFace> s_persons;
	static int s_maxpersonid;
};

#endif // __PERSONSFACE_H__
