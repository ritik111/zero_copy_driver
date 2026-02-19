#include<linux/init.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/fs.h>
#include<linux/uaccess.h>
#include<asm/io.h>
#include<linux/mm.h>
#include<linux/cdev.h>
#include<linux/poll.h>
#include<linux/timer.h>
#include "../include/shared.h"

#define DEVICE_NAME "my_device"
#define BURST_SIZE 512    //Transfer 512 bytes per "interrupt"
#define TIMER_INTERVAL_MS 50   

void simulated_dma_transfer(struct timer_list *);

static DEFINE_MUTEX(msg_lock);
static DECLARE_WAIT_QUEUE_HEAD(read_queue);

static dev_t dev_num;              //holds major and minor numbers
static struct cdev my_cdev;       // character device structure
static struct class *my_class;     //for automatic /dev/ node creation

static unsigned long buffer_addr;
static char* msg = NULL;

static struct timer_list dma_timer;
static int dma_running = 0;

void simulated_dma_transfer(struct timer_list *t) {
  struct rb_metadata *rb = (struct rb_metadata *)msg;
  int bytes_written = 0;
  
  while(bytes_written < BURST_SIZE) {
    unsigned int next_head = (rb->head + 1) % DATA_SIZE;
    if(next_head == rb->tail) {
      __sync_fetch_and_add(&rb->overflow_count, (BURST_SIZE-bytes_written));    //because of overflow
      break;
    }
    rb->data[rb->head] = 'A'+(rb->head % 26);
    rb->head = next_head;
    bytes_written++;
  }
  __sync_fetch_and_add(&rb->total_bytes_produced, bytes_written);    //atomic instruction
  if (bytes_written < BURST_SIZE) {
    unsigned int dropped = BURST_SIZE - bytes_written;
    __sync_fetch_and_add(&rb->overflow_count, dropped);
  }
  
  if (bytes_written > 0) {
    smp_wmb(); //commit all burst data before user sees the new head;
    wake_up_interruptible(&read_queue);
  }

  if(dma_running) {
      mod_timer(&dma_timer, jiffies+msecs_to_jiffies(TIMER_INTERVAL_MS));   // Reschedule the timer to simulate TIMER_INTERVAL_MS hardware frequency
  }
}

static int dev_mmap(struct file *filp,struct vm_area_struct *vma) {
  printk(KERN_INFO "IN MMAP\n");
  unsigned long size = vma->vm_end - vma->vm_start;
  
  if(size > (PAGE_SIZE << BUFFER_ORDER)) {
    return -EINVAL;
  }
  
  unsigned long physical = virt_to_phys((void *)msg);
  unsigned long pfn = physical >> PAGE_SHIFT;
  
  vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
  
  //automatically map BUFFER_ORDER pages in one go
  if (remap_pfn_range(vma,vma->vm_start,pfn,size,vma->vm_page_prot)) {
    return -EAGAIN;
  }
  return 0;
}

static __poll_t dev_poll(struct file *filep, poll_table *wait) {
  __poll_t mask = 0;
  struct rb_metadata *rb = (struct rb_metadata *) msg;
  
  poll_wait(filep,&read_queue,wait);
  
  if(rb->head != rb->tail) {
    mask |= EPOLLIN | EPOLLRDNORM;
  }
  
  return mask;
}

static int dev_open(struct inode *inodep,struct file *filep) {
  printk(KERN_INFO "Device : Device has been opened\n");
  return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
  printk(KERN_INFO "Device : Device successfully closed\n");
  return 0;
}

static struct file_operations fops = {
  .owner = THIS_MODULE,
  .open = dev_open,
  .poll = dev_poll,
  //.read = dev_read,     //not providing read functionality from driver
  //.write = dev_write,   //not providing write functionality
  .mmap = dev_mmap,
  .release = dev_release,
};

static int __init dev_init(void) {
  int ret;
  ret = alloc_chrdev_region(&dev_num,0,1,DEVICE_NAME);
  if(ret < 0) {
    printk(KERN_ERR "Error : Failed to allocate major number");
    return ret;
  }
  
  cdev_init(&my_cdev, &fops);
  my_cdev.owner = THIS_MODULE;
  
  ret = cdev_add(&my_cdev, dev_num,1);
  if(ret < 0) {
    unregister_chrdev_region(dev_num,1);
    return ret;
  }
  
  my_class = class_create("my_class");
  if (IS_ERR(my_class)) {
    ret = PTR_ERR(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    return ret;
  }
  device_create(my_class,NULL,dev_num,NULL,DEVICE_NAME);
  
  printk(KERN_INFO "Device registered!\n");

  // 1. Allocate 1 whole page (4096 bytes on most systems)
  // The '0' is the order (2^0 = 1 page).
  // __GFP_ZERO ensures the user doesn't see old kernel secrets.
  buffer_addr = __get_free_pages(GFP_KERNEL | __GFP_ZERO, BUFFER_ORDER);
  if(!buffer_addr) {
    printk(KERN_ERR "MyDevice : Failed to allocate memory pages\n");
    //cleanup
    device_destroy(my_class,dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num,1);
    return -ENOMEM;
  }
  msg = (char*) buffer_addr;
  for (int i = 0; i < (1 << BUFFER_ORDER); i++) {
    SetPageReserved(virt_to_page(msg + (i * PAGE_SIZE)));   //Linux kernel might try to swap out your pages or manage them thats why we are using this
  }
  //strcpy(msg,"Hello from kernel side via mmap!");
  struct rb_metadata * rb = (struct rb_metadata *)msg;
  rb->head = 0;
  rb->tail = 0;
  rb->total_bytes_produced = 0;
  rb->overflow_count = 0;
  rb->total_bytes_consumed = 0;
  printk(KERN_INFO "MyDevice : Page allocated at virtual address %p, head and tail initialized to 0.\n",msg);
  printk(KERN_INFO "Initiating timer setup for DMA simulation....\n");
  timer_setup(&dma_timer, simulated_dma_transfer,0);
  dma_running = 1;
  mod_timer(&dma_timer, jiffies+ msecs_to_jiffies(TIMER_INTERVAL_MS));
  printk(KERN_INFO "DMA simulation setup completed.\n");
  return 0;
}

static void __exit dev_exit(void) {
  dma_running = 0;
  timer_shutdown_sync(&dma_timer);
  if(msg) {
    for(int i=0;i<(1 << BUFFER_ORDER);i++) {
      ClearPageReserved(virt_to_page(msg + (i*PAGE_SIZE)));
    }
    free_pages((unsigned long)msg,BUFFER_ORDER);
  }
  
  device_destroy(my_class,dev_num); //destroy the /dev/ device class
  class_destroy(my_class);
  
  cdev_del(&my_cdev);               //delete the cdev and unregister its region
  unregister_chrdev_region(dev_num,1);
  
  //unregister_chrdev(major_number,DEVICE_NAME);
  printk(KERN_INFO "Device unregistered.\n");
}

module_init(dev_init);
module_exit(dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ABC");
MODULE_DESCRIPTION("SIMPLE HELLO WORLD MODULE");
