#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
 
int main()
{
	struct dirent *dir;
	DIR *d;
	int fd, size;
	
	d = opendir("./");
	if (!d)
		return -1;

	while (dir = readdir(d)) {
		if (dir-> d_type != DT_DIR) {
			printf("file: %s\n", dir->d_name);
			
			if (strcmp(dir->d_name, "directory.c") == 0) {
				char buf[4096];
				
				fd = open(dir->d_name, 0);
				if (!fd)
					return -1;
					
				size = read(fd, buf, sizeof(buf));
				if (size) {
					buf[size] = '\0';
					printf("%s", buf);
				}
			}
		} else {
			if (dir -> d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
				printf("directory: %s\n", dir->d_name);
			}
		}
	}
	closedir(d);

	return 0;
}
