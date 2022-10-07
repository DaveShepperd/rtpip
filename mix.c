#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	struct stat st;
	int foo, bar, sts;
	unsigned char *buf;
	
	sts = stat("foo.dsk",&st);
	if ( sts )
	{
		perror("Can't stat foo.dsk");
		return 1;
	}
	buf = (unsigned char *)malloc(st.st_size);
	if ( !buf )
	{
		char err[128];
		snprintf(err,sizeof(err),"Can't malloc %ld bytes of memory", st.st_size);
		perror(err);
		return 1;
	}
	foo = open("foo.dsk", O_RDONLY);
	if ( !foo )
	{
		perror("Can't open foo.dsk");
		free(buf);
		return 1;
	}
	bar = open("bar.dsk",O_RDWR);
	if ( !bar )
	{
		perror("Can't open bar.dsk");
		close(foo);
		free(buf);
		return 1;
	}
	sts = read(foo,buf,st.st_size);
	if ( sts != st.st_size )
	{
		char err[128];
		snprintf(err,sizeof(err),"Failed to read %ld bytes from foo.dsk", st.st_size);
		perror(err);
		close(foo);
		close(bar);
		free(buf);
		return 1;
	}
	sts = write(bar,buf,st.st_size);
	if ( sts != st.st_size )
	{
		char err[128];
		snprintf(err,sizeof(err),"Failed to write %ld bytes to bar.dsk", st.st_size);
		perror(err);
		close(foo);
		close(bar);
		free(buf);
		return 1;
	}
	close(foo);
	close(bar);
	free(buf);
	return 0;
}

