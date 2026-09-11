#include <stdio.h>

int main(int argc, char **argv)
{
	char *cmd = "arecord -D plughw:0 -f S16_LE -c 1 -r 16000 -t raw -q -";
	char buff[256];
	FILE *fp = popen(cmd,"r");

	for(int i = 0;i<16;i++)
	{
		int result = fread(buff, 1, sizeof(buff),fp);
		printf("Bytes >> %d bytes", result);

		
	}

}




