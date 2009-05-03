#ifndef COMMON_H__
#define COMMON_H__

#include <string>
#include <string.h>

using namespace std;

extern const char* prompt_line;

void prompt();
void perros(const char* s);

int readall(int fd, char * buf, size_t len);
int read_to_char(int fd, string & dest, int needle);
int readln(int fd, string & dest);

int writeall(int fd, const char * buf, size_t len);
int writeall(int fd, const string & str);
int writeln(int fd, const string & s_);

int connect_to(int must_bind, char* host, char* port);

#endif //COMMON_H__

