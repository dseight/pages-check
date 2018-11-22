#include <stdio.h>
#include <string.h>
#include <unistd.h>

void print_pages(FILE *f)
{
	char buf[65] = {0};
	unsigned offset = 0;

	fseek(f, 0, SEEK_SET);
	while (fread(buf, 1, sizeof(buf) - 1, f) != 0) {
		printf("%08x: %s\n", offset++, buf);
	}
}

int main(void)
{
	long size = 4 * sysconf(_SC_PAGESIZE);
	unsigned *data;
	unsigned i;
	FILE *f;

	/* Ask for some memory.  Note: pages *must* be accessed at least once
	 * to be allocated (thus, visible in /proc/pages_view). */
	data = sbrk(size);
	if (data == (void *)-1) {
		perror("sbrk");
		return 1;
	}

	/* Fill allocated memory with a known pattern. */
	for (i = 0; i < size / sizeof(*data); ++i)
		data[i] = 0xb6b6b6b6;

	f = fopen("/proc/pages_view", "r");
	if (!f) {
		perror("fopen");
		return 2;
	}

	printf("before freeing pages:\n");
	print_pages(f);

	sbrk(-size);

	printf("after freeing pages:\n");
	print_pages(f);

	return 0;
}
