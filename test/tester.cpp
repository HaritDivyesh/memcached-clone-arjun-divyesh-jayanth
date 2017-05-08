#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <mutex>

#define MEMCACHED_PORT 11211
#define BUFFER_SIZE 1024
#define GENERATED_RESULT_BUFF_SIZE 1024*1024
#define COMMENT_BEGINNER '#'

#define INPUT_FILE "test/input/basic.txt"
#define EXPECTED_OUTPUT_FILE "test/expected_output/basic.txt"

void compare(char *generated)
{
	char expected_line[BUFFER_SIZE] = {0};
	unsigned line_number = 0;
	char *generated_line;
	FILE* fp = fopen(EXPECTED_OUTPUT_FILE, "r");
	if (!fp) {
		fprintf(stderr, "Could not open expected output file: %s\n", EXPECTED_OUTPUT_FILE);
		exit(1);
	}

	while (fgets(expected_line, sizeof expected_line, fp)) {
		++line_number;
		/* ignore comments that begin with COMMENT_BEGINNER and blank lines */
		if (expected_line[0] == COMMENT_BEGINNER || expected_line[0] == '\n') {
 			continue;
 		}
		expected_line[strlen(expected_line) - 1] = '\0';
		generated_line = strtok(line_number == 1 ? generated : NULL, "\r\n");
		if (!generated_line) {
 			goto error;
 		}
 		/*if (strcmp(generated_line, expected_line) != 0) {
 			// look for the word ARBITRARY in expected_line.
 			//If exists, ignore corresponding word in generated_line
 			printf("%s|%s\n", generated_line, expected_line);
			char *word = strtok(expected_line, " ");
			while (word) {
				if (strcmp(word, "ARBITRARY") == 0) {
					word = strtok(NULL, " ");
					continue;
				}
				if (!strstr(generated_line, word)) {
					printf("%s\n", "XXXXXX");
					goto error;
 				}
				word = strtok(NULL, " ");
			}
 		}*/
	}

	printf("[TEST SUCCESS] for %s\n", INPUT_FILE);
	return;
error:
	fprintf(stderr, "[TEST FAILURE] Generated output not matching %s:%u\n",
		EXPECTED_OUTPUT_FILE, line_number);
 	exit(1);
}

int main(void)
{
	int sockfd;
	struct sockaddr_in server_addr;
	static unsigned short port = MEMCACHED_PORT;
	char buffer[BUFFER_SIZE] = {0};
	char *generated_result = (char*)malloc(GENERATED_RESULT_BUFF_SIZE);
	memset(generated_result, 0, GENERATED_RESULT_BUFF_SIZE);

	/*
	* create a TCP socket
	*/
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Connect to the memcached server */
	connect(sockfd, (struct sockaddr*) &server_addr, sizeof server_addr);

	//const char *file = INPUT_FILE;
	FILE* fp = fopen(INPUT_FILE, "r");
	if (!fp) {
		fprintf(stderr, "Could not open input file: %s\n", INPUT_FILE);
		exit(1);
	}

	unsigned line_number = 0;
 	while (fgets(buffer, sizeof buffer, fp)) {
 		char *W;
 		int w;
 		++line_number;
 		/* ignore comments that begin with COMMENT_BEGINNER and blank lines */
 		if (buffer[0] == COMMENT_BEGINNER || buffer[0] == '\n') {
 			memset(buffer, 0, sizeof buffer);
 			continue;
 		}
 		/* W is the number of lines of writes required for the buffer */
 		W = strtok(buffer, "\n");
 		if (!W) {
 			goto error;
 		}
 		w = atoi(W);
 		/*printf("w=%d\n", w);*/
 		if (!w) {
 			goto error;
 		}
 		while (w--) {
 			char *s = fgets(buffer, sizeof buffer, fp);
 			++line_number;
 			if (!s) {
	 			goto error;
 			}
 			buffer[strcspn(buffer, "\n")] = '\r';
 			buffer[strcspn(buffer, "\r") + 1] = '\n';
 			buffer[strcspn(buffer, "\n") + 1] = '\0';
 			printf("%s", buffer);
 			strcat(generated_result, buffer);
 			/*printf("strlen of %s: %zu\n", buffer, strlen(buffer));*/
 			write(sockfd, buffer, strlen(buffer));
 			usleep(100);
 			memset(buffer, 0, sizeof buffer);
 		}
 		
		/*printf("%s\n", "Blocking on read");*/
		read(sockfd, buffer, sizeof buffer);
		printf("%s", buffer);
		strcat(generated_result, buffer);
		if (strncmp(buffer, "ERROR", strlen("ERROR")) == 0 ||
			strncmp(buffer, "CLIENT_ERROR", strlen("CLIENT_ERROR")) == 0 ||
			strncmp(buffer, "SERVER_ERROR", strlen("SERVER_ERROR")) == 0) {
			fprintf(stderr, "[TEST FAILURE] for %s:%u, received reply:\n%s\n",
				INPUT_FILE, line_number, buffer);
			exit(1);
		}
		memset(buffer, 0, sizeof buffer);
 	}

 	compare(generated_result);
 	fclose(fp);
 	return 0;
 error:
 	fprintf(stderr, "Error in input file: %s:%u\n", INPUT_FILE, line_number);
 	exit(1);
}
