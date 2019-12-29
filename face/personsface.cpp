
#include "stdafx.h"

#include "personsface.h"

string PersonsFace::s_filepath;
vector<PersonFace> PersonsFace::s_persons;
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

string PersonsFace::tostring( const std::vector<float>& facedescriptor )
{
	char buf[64];

	string res;

	for( int i = 0; i < 128; i++ )
	{
		sprintf( buf, "%f", facedescriptor[i] );
		if( i != 0 )
		{
			res += " ";
		}
		res += buf;
	}

	return res;
}

double PersonsFace::distance( const std::vector<float>& facedescriptor1, const std::vector<float>& facedescriptor2 )
{
	double q = 0;
	for( int i = 0; i < 128; i++ )
	{
		double r = facedescriptor1[i] - facedescriptor2[i];
		q += r * r;
	}
	return sqrt( q );
}

void PersonsFace::init(const string& filepath )
{
	s_filepath = filepath;

	s_maxpersonid = 0;

	char buf[4096];

	FILE *fd = fopen(filepath.c_str(), "r");

	if( !fd )
		return;

	while (fgets(buf, 4096, fd))
	{
		s_persons.push_back(PersonFace());
		PersonFace& person = s_persons.back();

		char *p, *ptr;

		p = strtok_r(buf, ";", &ptr);

		if (!p)
			continue;

		person.id = atoi(trim(p));

		if (person.id > s_maxpersonid)
		{
			s_maxpersonid = person.id;
		}

		p = strtok_r(NULL, ";", &ptr);

		if (!p)
			continue;

		person.counter = atoi(trim(p));

		p = strtok_r(NULL, ";", &ptr);

		if (!p)
			continue;

		{
			int i = 0;
			char *ptr;

			for (char *q = strtok_r(p, " ", &ptr); q; q = strtok_r(NULL, " ", &ptr))
			{
				if (i == 128)
					break;
				person.facedescriptor.push_back(float(atof(trim(q))));
			}
		}

		p = strtok_r(NULL, ";", &ptr);

		if( !p )
			continue;

		person.name = trim(p);
	}

	fclose(fd);
}

void PersonsFace::update()
{
	FILE *fd = fopen(s_filepath.c_str(), "w");

	if (!fd)
		return;

	for (vector<PersonFace>::iterator it = s_persons.begin(); it != s_persons.end(); it++)
	{
		fprintf(fd, "%d;%d;%s;%s\n", (*it).id, (*it).counter, tostring((*it).facedescriptor).c_str(), (*it).name.c_str());
	}

	fclose(fd);
}

PersonFace& PersonsFace::get(const vector<float>& facedescriptor)
{
	size_t idx = -1;
	double best_len = -1;

	double threshold_len = 0.6;

	for (int i = 0; i < s_persons.size(); i++)
	{
		double len = distance(facedescriptor, s_persons[i].facedescriptor);

		if (len < threshold_len)
		{
			if (best_len == -1 || len < best_len)
			{
				best_len = len;
				idx = i;
			}
		}
	}

	if (idx == -1)
	{
		PersonFace personface;
		personface.id = ++s_maxpersonid;
		personface.counter = 0;
		s_persons.push_back( personface );
		idx = s_persons.size() - 1;
	}

	return s_persons[idx];
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
