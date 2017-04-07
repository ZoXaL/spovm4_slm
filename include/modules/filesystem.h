#ifndef FILESYSTEM_H
#define FILESYSTEM_H
struct FileSystem {
	void (*test)(void);
	int (*proceed)(int argc, char* argv[]);
	void (*showUsage)();
	void (*gracefullShutdown)();
};

extern const struct FileSystem FS;
#endif 