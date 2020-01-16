
#include "stdafx.h"

#include "personsface.h"

#if _WIN32 || _WIN64
#define strcasecmp _stricmp
#define strdup _strdup
#endif

string PersonsFace::s_filepath;
ListFace PersonsFace::s_persons;
int PersonsFace::s_maxpersonid;

char* strtok_r(char* s, const char* delim, char** ptrptr)
{
	char* p1;
	char* p2;
	const char* d;
	int  f;

	if (s)
		p1 = s;
	else {
		if (!*ptrptr)
			return 0;
		p1 = *ptrptr;
	}

	f = 1;
	while (*p1 && f) {
		f = 0;
		for (d = delim; *d; d++) {
			if (*p1 == *d) {
				f = 1;
				break;
			}
		}
		if (f)
			p1++;
	}
	if (!*p1) {
		*ptrptr = 0;
		return 0;
	}

	for (p2 = p1; *p2; p2++) {
		for (d = delim; *d; d++) {
			if (*p2 == *d) {
				*p2 = 0;
				*ptrptr = p2 + 1;
				return p1;
			}
		}
	}

	*ptrptr = p2;
	return p1;
}

char* trimleft(char* s)
{
	char* p = s;
	char* q = s;
	if (*p) {
		while (_istspace(*p))
			p++;
		while (*q++ = *p++);
	}
	return s;
}

char* trimright(char* s)
{
	char* p = s;
	if (*p) {
		while (*p)
			p++;
		p--;
		while (_istspace(*p))
			p--;
		p++;
		*p = 0;
	}
	return s;
}

char* trim(char* s)
{
	return trimleft(trimright(s));
}

vector<float> PersonFace::parse(const char* str)
{
	vector<float> facedescriptor;

	char *p = strdup(str);

	int i = 0;
	char *ptr;

	for (char *q = strtok_r(p, " ", &ptr); q; q = strtok_r(NULL, " ", &ptr))
	{
		if (i == 128)
		{
			break;
		}

		facedescriptor.push_back(float(atof(trim(q))));
	}

	free(p);

	return facedescriptor;
}

string PersonFace::tostring(const vector<float>& facedescriptor)
{
	char buf[64];

	string res;

	for (int i = 0; i < 128; i++)
	{
		sprintf(buf, "%f", facedescriptor[i]);
		if (i != 0)
		{
			res += " ";
		}
		res += buf;
	}

	return res;
}

double PersonFace::distance(const vector<float>& facedescriptor1, const vector<float>& facedescriptor2)
{
	double q = 0;
	for (int i = 0; i < 128; i++)
	{
		double r = facedescriptor1[i] - facedescriptor2[i];
		q += r * r;
	}
	return sqrt(q);
}

vector<float> PersonFace::subtract(const vector<float>& facedescriptor1, const vector<float>& facedescriptor2)
{
	vector<float> facedescriptor;

	for (int i = 0; i < 128; i++)
	{
		facedescriptor.push_back(facedescriptor1[i] - facedescriptor2[i]);
	}

	return facedescriptor;
}

double PersonFace::scalar(const vector<float>& facedescriptor1, const vector<float>& facedescriptor2)
{
	double r = 0;

	for (int i = 0; i < 128; i++)
	{
		r += facedescriptor1[i] * facedescriptor2[i];
	}

	return r;
}

vector<float> PersonFace::aggregate(const vector<float>& facedescriptor1, float deviation1, size_t counter1,
			const vector<float>& facedescriptor2, float deviation2, size_t counter2,
			float* deviation, size_t* counter)
{
	vector<float> facedescriptor;

	for (int i = 0; i < 128; i++)
	{
		facedescriptor.push_back((counter1 * facedescriptor1[i] + counter2 * facedescriptor2[i]) / (counter1 + counter2));
	}

	(*deviation) = float(sqrt((2 * counter1 * scalar(facedescriptor1, subtract(facedescriptor1, facedescriptor)) +
		2 * counter2 * scalar(facedescriptor2, subtract(facedescriptor2, facedescriptor)) +
		(counter1 + counter2) * scalar(facedescriptor, facedescriptor) -
		counter1 * scalar(facedescriptor1, facedescriptor1) -
		counter2 * scalar(facedescriptor2, facedescriptor2) +
		counter1 * deviation1 * deviation1 +
		counter2 * deviation2 * deviation2) / (counter1 + counter2)));

	(*counter) = counter1 + counter2;

	return facedescriptor;
}

void PersonsFace::init(const string& filepath)
{
	s_filepath = filepath;

	s_maxpersonid = 0;

	char buf[4096];

	FILE *fd = fopen(filepath.c_str(), "r");

	if (!fd)
		return;

	PersonFace* person = nullptr;

	while (fgets(buf, 4096, fd))
	{
		if (person == nullptr)
			person = s_persons.Add();

		char *p, *ptr;

		p = strtok_r(buf, ";", &ptr);

		if (!p)
			continue;

		person->id = atoi(trim(p));

		if (person->id > s_maxpersonid)
			s_maxpersonid = person->id;

		p = strtok_r(NULL, ";", &ptr);

		if (!p)
			continue;

		person->counter = atoi(trim(p));

		p = strtok_r(NULL, ";", &ptr);

		if (!p)
			continue;

		person->deviation = float(atof(trim(p)));

		p = strtok_r(NULL, ";", &ptr);

		if (!p)
			continue;

		person->facedescriptor = PersonFace::parse(trim(p));

		p = strtok_r(NULL, ";", &ptr);

		if (p)
			person->name = trim(p);

		person = nullptr;
	}

	fclose(fd);
}

void PersonsFace::update()
{
	FILE *fd = fopen(s_filepath.c_str(), "w");

	if( !fd )
	{
		return;
	}

	for (PersonFace* person = s_persons.Bottom(); person != nullptr; person = person->next)
	{
		fprintf(fd, "%d;%zd;%f;%s;%s\n", person->id, person->counter, person->deviation, PersonFace::tostring(person->facedescriptor).c_str(), person->name.c_str());
	}

	fclose(fd);
}

PersonFace& PersonsFace::get(const vector<float>& facedescriptor, float deviation)
{
	PersonFace* best_person = nullptr;
	double best_len = -1;

	double threshold_len = 0.6;
	double factor_dev = 1.1;

	for (PersonFace* person = s_persons.Bottom(); person != nullptr; person = person->next)
	{
		double len = PersonFace::distance(facedescriptor, person->facedescriptor);

		if (len < threshold_len && len < person->deviation * factor_dev)
		{
			if (best_len == -1 || len < best_len)
			{
				best_len = len;
				best_person = person;
			}
		}
	}

	if (best_person != nullptr)
	{
		return *best_person;
	}

	PersonFace* person = s_persons.Add();
	person->id = ++s_maxpersonid;
	person->counter = 0;
	person->deviation = 0;

	return *person;
}

PersonsFace::PersonsFace()
{
}

PersonsFace::PersonsFace(const PersonsFace& personsface)
{
}

PersonsFace& PersonsFace::operator=(const PersonsFace& personsface)
{
	return *this;
}

PersonsFace::~PersonsFace()
{
}
