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
	PersonFace* next;

	void update(const vector<float>& facedescriptor_, float deviation_, size_t counter_, const string& name_ = "")
	{
		if (counter == 0)
		{
			facedescriptor = facedescriptor_;
			deviation = deviation_;
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

struct ListFace
{
	ListFace()
	{
		top = nullptr;
		bottom = nullptr;
	}

private:
	ListFace(const ListFace& listFace);
	ListFace& operator=(const ListFace& listFace);

public:
	virtual ~ListFace()
	{
		for (PersonFace* person = bottom; person != nullptr;)
		{
			PersonFace* next = person->next;
			delete person;
			person = next;
		}
	}

	PersonFace* Bottom()
	{
		return bottom;
	};

	PersonFace* Add(PersonFace *person = nullptr)
	{
		if (!person)
		{
			person = new PersonFace();
		}

		person->next = nullptr;

		if (bottom == nullptr)
		{
			bottom = person;
		}
		else
		{
			top->next = person;
		}

		top = person;

		return top;
	}

private:
	PersonFace* top;
	PersonFace* bottom;
};

struct PersonsFace
{
	static void init(const string& filepath);
	static void update();
	static PersonFace* get(const vector<float>& facedescriptor);
	static void add(PersonFace* person);

	PersonsFace();
	PersonsFace(const PersonsFace& personsface);
	PersonsFace& operator=(const PersonsFace& personsface);

	virtual ~PersonsFace();

	static string s_filepath;
	static ListFace s_persons;
	static int s_maxpersonid;
};

#endif // __PERSONSFACE_H__
