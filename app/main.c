#include<stdio.h> 
#include<stdlib.h>
#include<fcntl.h>
#include<sys/mman.h>
#include<unistd.h>
#include<string.h>
#include<time.h>
#include<signal.h>
#include<poll.h>
#include "../include/shared.h"

static volatile sig_atomic_t keep_running = 1;
struct pollfd fds;

void handle_sigint(int sig) {
  keep_running = 0;
}

int main(void) {
  int fd;
  signal(SIGINT, handle_sigint);

  fd = open("/dev/my_device",O_RDWR);
  if(fd < 0) {
    perror("failed to open device ");
    return -1;
  }
  printf("Device opened\n");
  fds.fd = fd;
  fds.events = POLLIN;
  struct rb_metadata *shared = mmap(NULL,BUFFER_SIZE,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0);

  if(shared == MAP_FAILED) {
    perror("mmap failed");
    close(fd);
    return -1;
  }
  printf("Consuming data....");
  int ret = 1;
  while (ret>0 && keep_running) {
    ret = poll(&fds,1,5000);
    int local_count = 0;
    while(shared->head != shared->tail && keep_running) {
      __sync_synchronize();     /* Ensure we see the most recent data written by the kernel */
      //char c = shared->data[shared->tail];
      //printf("%c ",c);      
      shared->tail = (shared->tail+1) % DATA_SIZE;
      local_count++;
    }
    __sync_fetch_and_add(&shared->total_bytes_consumed,local_count);
    fflush(stdout);
  }
  __sync_synchronize();
  printf("\ntotal bytes produced = %lu\nTotal overflow in bytes = %lu\nTotal bytes consumed by user = %lu\n",shared->total_bytes_produced,shared->overflow_count,shared->total_bytes_consumed);
  munmap(shared,BUFFER_SIZE);
  close(fd);
  printf("Program Terminated!!!\n");
  return 0;
}
