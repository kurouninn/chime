//#include <wiringPi.h>

#include <stdlib.h>

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <fcntl.h>

#include <strings.h>
#include <string>
#include <deque>

#include <sys/epoll.h>
#include <pthread.h>

#include <sys/types.h>

#include <sys/stat.h>

int readconf(const char *name);

void timer_handler(int signum);

void abrt_handler(int sig, siginfo_t *info, void *ctx);

#define LINESIZE 256

#define DEFAULT_CONF "chime.conf"
#define PATH "path"
#define MUSIC "music"
#define MUSIC_NONE "music_none"

#define WATCHDOG 300000 //ms

#define GPIO_PIN "17"

#define GPIO_PATH "/sys/class/gpio"
#define GPIO_EXPORT GPIO_PATH "/export"
#define GPIO_UNEXPORT GPIO_PATH "/unexport"
#define GPIO_GPIOPIN GPIO_PATH "/gpio" GPIO_PIN
#define GPIO_DIRECTION GPIO_GPIOPIN "/direction"
#define GPIO_EDGE GPIO_GPIOPIN "/edge"
#define GPIO_VALUE GPIO_GPIOPIN "/value"


std::string path;
std::deque<std::string> musics;

int epfd = -1;
bool gpiosetup = false;
volatile bool loop = true;
void release();
int gpioinit();


int main(int argc, char *argv[])
{
	int ret = 0;
	if (argc == 1)
		ret = readconf(DEFAULT_CONF);
	else
		ret = readconf(argv[1]);

	if (ret <= 1) {
		printf("music files none\n");
		return -1;
	}

	if ((epfd = epoll_create(255)) < 0) {
		printf("failure epoll_create\n");
		return -1;
	}

	struct sigaction sa_sigabrt = { 0 };

	sa_sigabrt.sa_sigaction = abrt_handler;
	sa_sigabrt.sa_flags = SA_SIGINFO;

	if (sigaction(SIGINT, &sa_sigabrt, NULL) < 0) {
		printf("failure sigaction\n");
		return -1;
	}

	if ((ret = gpioinit()) < 0) {
		printf("error gpioinit %d - errno %d\n",ret, errno);
		release();
		return 0;
	}
	
	epoll_event ee = { 0 };
	
	for (unsigned int i = 1; loop;) {
		if ((ret = epoll_wait(epfd, &ee, 1, WATCHDOG)) == 0) {
			system(("aplay " + path + musics[0]).c_str());
		}
		else if(ret > 0){
			system(("aplay " + path + musics[i]).c_str());
			printf(("play " + path + musics[i] + "\n").c_str());

			if (musics.size() <= ++i)
				i = 1;
			epoll_wait(epfd, &ee, 1, 0);
		}else
			printf("play epoll_wait error\n");

	}

	//while (true) {
	//	if (epoll_wait(epfd, &ee, 1, WATCHDOG) == 0) {
	//		printf("watchdog\n");
	//	}
	//	else {
	//		for (int i = 1; i < musics.size(); i++)
	//			printf(("play " + musics[i] + "\n").c_str());
	//	}
	//}

	release();
	return 0;
}

void release()
{
	loop = false;
	if (!(epfd < 0)) {
		close(epfd);
		epfd = -1;
	}

	if (gpiosetup) {
		int fd = open(GPIO_UNEXPORT, O_WRONLY);
		write(fd, GPIO_PIN, 2);
		close(fd);
		gpiosetup = false;
	}
}

void abrt_handler(int sig, siginfo_t *info, void *ctx) {
	release();
}

int readconf(const char *name)
{
	char s[LINESIZE] = {};
	char key[LINESIZE], value[LINESIZE];
	char *ret = nullptr;
	FILE *f = fopen(name, "r");
	if (f == nullptr)
		return -1;

	//printf(" %s open \n", name);

	while ( (ret = fgets(s, LINESIZE, f)) != nullptr) {
		if (s[0] == '#')
			continue;

		sscanf(s, " %[^= ] = %s%*[^\n]", key, value);

		//printf("key = %s, value = %s\n", key, value);

		if (strcasecmp(key, MUSIC) == 0) {
			musics.push_back(value);
		}else if (strcasecmp(key, MUSIC_NONE) == 0) {
			musics.push_front(value);
		}else if (strcasecmp(key, PATH) == 0) {
			path = value;
		}
	}

	if(f != nullptr)
		fclose(f);

	return musics.size();
}

int gpioinit()
{
	int fd;

	if ((fd = open(GPIO_EXPORT, O_WRONLY)) == -1) {
		return -1;
	}

	write(fd, GPIO_PIN, 2);
	close(fd);

	gpiosetup = true;

	usleep(1000000); //"Permission denied" counterplan 

	if ((fd = open(GPIO_DIRECTION, O_WRONLY)) == -1) {
		return -2;
	}
	write(fd, "in", 2);
	close(fd);

	if ((fd = open(GPIO_EDGE, O_WRONLY)) == -1) {
		return -3;
	}
	write(fd, "rising", 6);
//	write(fd, "both", 4);
	close(fd);

	if ((fd = open(GPIO_VALUE, O_RDONLY)) == -1) {
		return -4;
	}

	epoll_event ee = { 0 };
	ee.events = EPOLLET;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ee) != 0) {
		return -5;
	}

	epoll_wait(epfd, &ee, 1, 0);

	return 0;
}
