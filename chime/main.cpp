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
#include <vector>

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

#define WATCHDOG 180000 //ms

#define GPIO_PIN "17"

#define GPIO_PATH "/sys/class/gpio"
#define GPIO_EXPORT GPIO_PATH "/export"
#define GPIO_UNEXPORT GPIO_PATH "/unexport"
#define GPIO_GPIOPIN GPIO_PATH "/gpio" GPIO_PIN
#define GPIO_DIRECTION GPIO_GPIOPIN "/direction"
#define GPIO_EDGE GPIO_GPIOPIN "/edge"
#define GPIO_VALUE GPIO_GPIOPIN "/value"


std::string path;
std::string none;
std::vector<std::string> musics;

int epfd = -1;
int gvfd = -1;
bool gpiosetup = false;
volatile bool loop = true;
void release();
int gpioinit();
void random_shuffle(std::vector<int> &no);


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

	std::vector<int> no;
	int musiccount = (int)musics.size();
	no.resize(musiccount);

	for (int i = 0; i < musiccount; i++) {
		no[i] = i;
	}

	random_shuffle(no);
	//for (int i = 0; i < musiccount; i++) {
	//	printf("%d ", no[i]);
	//}
	//printf("\n");

	for (int i = 0; i < musiccount; i++) {
		printf((path + musics[i] + "\n").c_str());
	}

	for (int i = 0; loop;) {
		if ((ret = epoll_wait(epfd, &ee, 1, WATCHDOG)) == 0) {
			system(("aplay -q " + path + none).c_str());
		} else if(ret > 0 && (ee.events & EPOLLIN)){
			lseek(gvfd, 0, SEEK_SET);
			char c = 0;
			int n = read(gvfd, &c, 1);

			if (n > 0 && c == '1') {
				system(("aplay -q " + path + musics[no[i++]]).c_str());
				if (i >= musiccount) {
					i = 0;
					random_shuffle(no); 
				}
			} else if (n > 0 && c == '0') {

			}

//			epoll_wait(epfd, &ee, 1, 0);
		}else
			printf("play epoll_wait error %d\n",ee.events);
	}

	release();
	return 0;
}

void random_shuffle(std::vector<int> &no)
{
	int size = no.size();
	int a, b;
	size_t i;
	srand((unsigned)time(NULL));

	for (i = size; i > 1; --i) {
		a = i - 1;
		b = rand() % i;
		std::swap(no[a], no[b]);
	}
}

void release()
{
	loop = false;
	if (!(epfd < 0)) {
		close(epfd);
		epfd = -1;
	}

	if (gvfd != -1) {
		close(gvfd);
		gvfd = -1;
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

//	printf(" %s open \n", name);

	while ( (ret = fgets(s, LINESIZE, f)) != nullptr) {
		if (s[0] == '#')
			continue;

		if (sscanf(s, " %[^= ] = %s%*[^\n]", key, value) > 0) {
//			printf("key = %s, value = %s\n", key, value);

			if (strcasecmp(key, MUSIC) == 0) {
				musics.push_back(value);
			}
			else if (strcasecmp(key, MUSIC_NONE) == 0) {
				none = value;
			}
			else if (strcasecmp(key, PATH) == 0) {
				path = value;
			}
			key[0] = value[0] = '\0';
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

	if ((gvfd = open(GPIO_VALUE, O_RDONLY | O_NONBLOCK)) == -1) {
		return -4;
	}

	epoll_event ee = { 0 };
	ee.events = EPOLLET | EPOLLIN;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ee) != 0) {
		return -5;
	}

	epoll_wait(epfd, &ee, 1, 0);

	return 0;
}
